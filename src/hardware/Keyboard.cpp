// OpenMeshOS — Keyboard.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BBQ10KB I2C keyboard driver for T-Deck family.
// Talks to the KBD16C firmware on the BBQ10KB via I2C at 0x1F.
//
// Key FIFO: each entry is 16 bits.
//   High byte: ASCII key code (or raw scan code for special keys)
//   Low byte:  state (0=idle, 1=press, 2=long, 3=release)
//
// Special key codes:
//   0x08  Backspace
//   0x0D  Enter
//   0x1B  Escape
//   0x09  Tab
//   0x11  Sym (function key)
//   0x12  Shift
//   0x13  Ctrl
//   0x14  Alt

#include "Keyboard.h"
#include "../utils/Log.h"

namespace oms {

// ── begin ────────────────────────────────────────────────────────────
bool Keyboard::begin(TwoWire *wire, uint8_t addr) {
    _wire = wire;
    _addr = addr;

    // Try to read version register to confirm keyboard is present
    uint8_t ver = readReg8(REG_VER);
    if (ver == 0 && readReg8(REG_KEY) == 0 && readReg8(REG_CFG) == 0) {
        // All zeros likely means no device responded
        _present = false;
        OMS_LOG("Keyboard", "No response from BBQ10KB at 0x%02X", _addr);
        return false;
    }

    _present = true;
    OMS_LOG("Keyboard", "BBQ10KB detected, FW v%d", ver);

    // Reset to known state
    reset();

    // Enable key interrupt in config
    writeReg8(REG_CFG, CFG_KEY_INT | CFG_OVERFLOW_INT);

    // Set backlight to 50%
    setBacklight(0.5f);

    return true;
}

// ── reset ───────────────────────────────────────────────────────────
void Keyboard::reset() {
    writeReg8(REG_RST, 0x01);
    delay(100);
    // Clear any pending interrupts
    writeReg8(REG_INT, 0x00);
}

// ── poll ─────────────────────────────────────────────────────────────
uint8_t Keyboard::poll(KeyEvent *outEvents, uint8_t maxOut) {
    if (!_present) return 0;

    uint8_t count = keyCount();
    if (count == 0) return 0;

    uint8_t outIdx = 0;
    while (count > 0 && outIdx < maxOut) {
        uint16_t fifo = readReg16(REG_FIF);
        char key = (char)(fifo >> 8);
        KeyState state = KeyState(fifo & 0xFF);

        if (state != StateIdle && key != '\0') {
            outEvents[outIdx].key = key;
            outEvents[outIdx].state = state;
            outIdx++;
        }
        count--;
    }

    return outIdx;
}

// ── keyCount ─────────────────────────────────────────────────────────
uint8_t Keyboard::keyCount() const {
    if (!_present) return 0;
    return readReg8(REG_KEY) & KEY_COUNT_MASK;
}

// ── backlight ───────────────────────────────────────────────────────
float Keyboard::backlight() const {
    if (!_present) return 0.0f;
    return readReg8(REG_BKL) / 255.0f;
}

void Keyboard::setBacklight(float value) {
    if (!_present) return;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    writeReg8(REG_BKL, (uint8_t)(value * 255.0f));
}

// ── lock state ───────────────────────────────────────────────────────
bool Keyboard::capsLock() const {
    if (!_present) return false;
    return (readReg8(REG_KEY) & KEY_CAPSLOCK) != 0;
}

bool Keyboard::numLock() const {
    if (!_present) return false;
    return (readReg8(REG_KEY) & KEY_NUMLOCK) != 0;
}

// ── I2C register access ─────────────────────────────────────────────
uint8_t Keyboard::readReg8(uint8_t reg) const {
    if (!_wire) return 0;

    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) return 0;

    _wire->requestFrom(_addr, (uint8_t)1);
    if (_wire->available() < 1) return 0;
    return _wire->read();
}

uint16_t Keyboard::readReg16(uint8_t reg) const {
    if (!_wire) return 0;

    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) return 0;

    _wire->requestFrom(_addr, (uint8_t)2);
    if (_wire->available() < 2) return 0;

    uint8_t low  = _wire->read();
    uint8_t high = _wire->read();
    return ((uint16_t)high << 8) | low;
}

void Keyboard::writeReg8(uint8_t reg, uint8_t val) {
    if (!_wire) return;

    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(val);
    _wire->endTransmission();
}

}  // namespace oms