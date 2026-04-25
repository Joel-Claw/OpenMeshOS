// OpenMeshOS — Keyboard.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BBQ10KB I2C keyboard driver for T-Deck family.
// I2C address: 0x1F
//
// Register map (from BBQ10KB firmware):
//   0x01 VER  - firmware version
//   0x02 CFG  - configuration
//   0x03 INT  - interrupt status
//   0x04 KEY  - key status (bits 0-4: count, bit 5: capslock, bit 6: numlock)
//   0x05 BKL  - backlight
//   0x06 DEB  - debounce config
//   0x07 FRQ  - poll frequency config
//   0x08 RST  - reset
//   0x09 FIF  - FIFO (16-bit: high=key, low=state)
//   0x0A BK2  - backlight 2
//   0x0B DIR  - GPIO direction
//   0x0C PUE  - GPIO pull enable
//   0x0D PUD  - GPIO pull direction
//   0x0E GIO  - GPIO value
//   0x0F GIC  - GPIO interrupt config
//   0x10 GIN  - GPIO interrupt status

#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace oms {

class Keyboard {
public:
    // Key states from FIFO
    enum KeyState : uint8_t {
        StateIdle   = 0,
        StatePress  = 1,
        StateLong   = 2,
        StateRelease = 3
    };

    // Key event returned to consumers
    struct KeyEvent {
        char     key;
        KeyState state;
    };

    static constexpr uint8_t DEFAULT_ADDR = 0x1F;

    Keyboard() = default;

    // Initialise I2C and reset keyboard
    bool begin(TwoWire *wire = &Wire, uint8_t addr = DEFAULT_ADDR);

    // Check keyboard presence
    bool isPresent() const { return _present; }

    // Poll for key events (call from main loop)
    // Returns number of events placed in outEvents (max maxOut)
    uint8_t poll(KeyEvent *outEvents, uint8_t maxOut);

    // Number of keys in FIFO
    uint8_t keyCount() const;

    // Backlight (0.0 - 1.0)
    float backlight() const;
    void setBacklight(float value);

    // Caps/Num lock state
    bool capsLock() const;
    bool numLock() const;

    // Reset keyboard firmware
    void reset();

private:
    // I2C register addresses
    static constexpr uint8_t REG_VER  = 0x01;
    static constexpr uint8_t REG_CFG  = 0x02;
    static constexpr uint8_t REG_INT  = 0x03;
    static constexpr uint8_t REG_KEY  = 0x04;
    static constexpr uint8_t REG_BKL  = 0x05;
    static constexpr uint8_t REG_DEB  = 0x06;
    static constexpr uint8_t REG_FRQ  = 0x07;
    static constexpr uint8_t REG_RST  = 0x08;
    static constexpr uint8_t REG_FIF  = 0x09;
    static constexpr uint8_t REG_BK2  = 0x0A;

    // Config bits
    static constexpr uint8_t CFG_OVERFLOW_ON  = (1 << 0);
    static constexpr uint8_t CFG_OVERFLOW_INT = (1 << 1);
    static constexpr uint8_t CFG_CAPSLOCK_INT = (1 << 2);
    static constexpr uint8_t CFG_NUMLOCK_INT  = (1 << 3);
    static constexpr uint8_t CFG_KEY_INT      = (1 << 4);
    static constexpr uint8_t CFG_PANIC_INT    = (1 << 5);

    // Key status bits
    static constexpr uint8_t KEY_COUNT_MASK = 0x1F;
    static constexpr uint8_t KEY_CAPSLOCK  = (1 << 5);
    static constexpr uint8_t KEY_NUMLOCK   = (1 << 6);

    // Read/write I2C registers
    uint8_t  readReg8(uint8_t reg) const;
    uint16_t readReg16(uint8_t reg) const;
    void     writeReg8(uint8_t reg, uint8_t val);

    TwoWire *_wire = nullptr;
    uint8_t  _addr = DEFAULT_ADDR;
    bool     _present = false;
};

}  // namespace oms