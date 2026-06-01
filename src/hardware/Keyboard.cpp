// OpenMeshOS — Keyboard.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BBQ10KB I2C keyboard driver for the T-Deck.
// The T-Deck uses an ESP32-C3 as an I2C keyboard controller running
// arturo182's bbq10kbd_i2c_sw firmware. This driver communicates
// with it over I2C at address 0x1F.
//
// Protocol reference: https://github.com/arturo182/bbq10kbd_i2c_sw
// Key events are read from a FIFO register (0x09). Each event is 2 bytes:
//   byte 0: key code (ASCII or modifier code)
//   byte 1: state (1=press, 2=long press, 3=release)
//
// We enable CFG_REPORT_MODS so modifier keys (Shift, Alt, Sym)
// generate their own events. The UI layer interprets these for
// capitalization and special character input.

#include "Keyboard.h"
#include "../utils/Log.h"

namespace oms {

// ── Static instance ────────────────────────────────────────────────
static Keyboard s_keyboard;

Keyboard& Keyboard::instance()
{
    return s_keyboard;
}

// ── init ───────────────────────────────────────────────────────────
void Keyboard::init()
{
    OMS_LOG("KB", "Probing BBQ10KB at 0x%02X", DEFAULT_ADDR);

    // Check if device is present on the I2C bus
    Wire.beginTransmission(DEFAULT_ADDR);
    uint8_t err = Wire.endTransmission();

    if (err != 0)
    {
        OMS_LOG("KB", "BBQ10KB not found (error %d), keyboard disabled", err);
        _present = false;
        return;
    }

    _present = true;

    // Reset the keyboard controller
    writeReg(REG_RST, 0x00);
    delay(100);

    // Configure: report modifier keys, generate interrupt on key press,
    // use modifiers to modify character output
    uint8_t cfg = CFG_USE_MODS | CFG_REPORT_MODS | CFG_KEY_INT | CFG_OVERFLOW_INT;
    writeReg(REG_CFG, cfg);
    delay(50);

    // Read firmware version for logging
    uint8_t ver = readReg8(REG_VER);
    OMS_LOG("KB", "BBQ10KB v%d.%d ready", (ver >> 4) & 0x0F, ver & 0x0F);

    // Set default backlight to mid brightness
    setBacklight(0.5f);
}

// ── poll ───────────────────────────────────────────────────────────
int Keyboard::poll()
{
    if (!_present)
    {
        return 0;
    }

    uint8_t status = readReg8(REG_KEY);
    uint8_t count = status & KEY_COUNT_MASK;

    if (count == 0)
    {
        return 0;
    }

    int read = 0;

    // Read up to 'count' events from the FIFO.
    // Each read drains one event from the keyboard's FIFO.
    // We cap at count to avoid stale reads, and leave room in our ring buffer.
    int maxRead = count;
    if (maxRead > EVENT_BUF_SIZE - _count)
    {
        maxRead = EVENT_BUF_SIZE - _count;
    }

    for (int i = 0; i < maxRead; i++)
    {
        uint16_t raw = readReg16(REG_FIF);
        char key = (char)(raw >> 8);
        KeyState state = static_cast<KeyState>(raw & 0xFF);

        // Only buffer press and long-press events.
        // Release events are useful for some UI patterns but we
        // skip them for simplicity in v0.2.
        if (state == KeyState::Press || state == KeyState::LongPress)
        {
            KeyEvent ev;
            ev.key = key;
            ev.state = state;
            pushEvent(ev);
            read++;
        }
    }

    return read;
}

// ── nextEvent ──────────────────────────────────────────────────────
bool Keyboard::nextEvent(KeyEvent& out)
{
    if (_count == 0)
    {
        return false;
    }

    out = _events[_tail];
    _tail = (_tail + 1) % EVENT_BUF_SIZE;
    _count--;
    return true;
}

// ── setBacklight ───────────────────────────────────────────────────
void Keyboard::setBacklight(float brightness)
{
    if (!_present) return;

    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;

    writeReg(REG_BKL, (uint8_t)(brightness * 255.0f));
}

// ── getBacklight ───────────────────────────────────────────────────
float Keyboard::getBacklight() const
{
    if (!_present) return 0.0f;
    return readReg8(REG_BKL) / 255.0f;
}

// ── keyCount ──────────────────────────────────────────────────────
uint8_t Keyboard::keyCount() const
{
    if (!_present) return 0;
    return readReg8(REG_KEY) & KEY_COUNT_MASK;
}

// ── I2C register access ──────────────────────────────────────────
uint8_t Keyboard::readReg8(uint8_t reg) const
{
    Wire.beginTransmission(DEFAULT_ADDR);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(DEFAULT_ADDR, (uint8_t)1);
    if (Wire.available() < 1)
    {
        return 0;
    }
    return Wire.read();
}

uint16_t Keyboard::readReg16(uint8_t reg) const
{
    Wire.beginTransmission(DEFAULT_ADDR);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(DEFAULT_ADDR, (uint8_t)2);
    if (Wire.available() < 2)
    {
        return 0;
    }
    uint8_t lo = Wire.read();
    uint8_t hi = Wire.read();
    return (hi << 8) | lo;
}

void Keyboard::writeReg(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(DEFAULT_ADDR);
    Wire.write(reg | WRITE_MASK);
    Wire.write(value);
    Wire.endTransmission();
}

// ── pushEvent (internal ring buffer) ──────────────────────────────
void Keyboard::pushEvent(const KeyEvent& ev)
{
    if (_count >= EVENT_BUF_SIZE)
    {
        // Drop oldest event if buffer is full
        _tail = (_tail + 1) % EVENT_BUF_SIZE;
        _count--;
    }

    _events[_head] = ev;
    _head = (_head + 1) % EVENT_BUF_SIZE;
    _count++;
}

}  // namespace oms