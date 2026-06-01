// OpenMeshOS — Keyboard.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BBQ10KB I2C keyboard driver for the T-Deck.
// Reads key events from the Blackberry Q10 keyboard controller
// over I2C (address 0x1F) and feeds them into the UI system.

#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace oms {

class Keyboard {
public:
    // Key state from BBQ10KB FIFO
    enum class KeyState : uint8_t
    {
        Idle = 0,
        Press = 1,
        LongPress = 2,
        Release = 3
    };

    // A single key event
    struct KeyEvent
    {
        char key;
        KeyState state;
    };

    // Modifier key codes (sent when CFG_REPORT_MODS is set)
    static constexpr char KEY_MOD_ALT  = 0x1A;
    static constexpr char KEY_MOD_SHL  = 0x1B;
    static constexpr char KEY_MOD_SHR  = 0x1C;
    static constexpr char KEY_MOD_SYM  = 0x1D;

    // Special key codes (ASCII control chars we map)
    static constexpr char KEY_ENTER    = '\n';   // 0x0A
    static constexpr char KEY_BACKSPACE = 0x08;  // backspace
    static constexpr char KEY_TAB      = '\t';   // 0x09

    // BBQ10KB I2C default address
    static constexpr uint8_t DEFAULT_ADDR = 0x1F;

    static Keyboard& instance();

    // Call once in setup(), after Wire.begin()
    void init();

    // Poll the keyboard — call every loop iteration.
    // Returns the number of events read (0 if no keys pending).
    // Events are buffered internally; use nextEvent() to consume them.
    int poll();

    // Get the next buffered event. Returns false if buffer is empty.
    bool nextEvent(KeyEvent& out);

    // Check if keyboard is present on the I2C bus
    bool isPresent() const { return _present; }

    // Set keyboard backlight brightness (0.0 - 1.0)
    void setBacklight(float brightness);

    // Get keyboard backlight brightness (0.0 - 1.0)
    float getBacklight() const;

    // Key count currently in the FIFO (not yet read)
    uint8_t keyCount() const;

private:
    // I2C registers (BBQ10KB protocol)
    static constexpr uint8_t REG_VER  = 0x01;
    static constexpr uint8_t REG_CFG  = 0x02;
    static constexpr uint8_t REG_INT  = 0x03;
    static constexpr uint8_t REG_KEY  = 0x04;
    static constexpr uint8_t REG_BKL  = 0x05;
    static constexpr uint8_t REG_DEB  = 0x06;
    static constexpr uint8_t REG_FRQ = 0x07;
    static constexpr uint8_t REG_RST  = 0x08;
    static constexpr uint8_t REG_FIF  = 0x09;

    static constexpr uint8_t WRITE_MASK = 0x80;

    // Config register bits
    static constexpr uint8_t CFG_OVERFLOW_ON  = (1 << 0);
    static constexpr uint8_t CFG_OVERFLOW_INT = (1 << 1);
    static constexpr uint8_t CFG_CAPSLOCK_INT = (1 << 2);
    static constexpr uint8_t CFG_NUMLOCK_INT  = (1 << 3);
    static constexpr uint8_t CFG_KEY_INT      = (1 << 4);
    static constexpr uint8_t CFG_PANIC_INT    = (1 << 5);
    static constexpr uint8_t CFG_REPORT_MODS  = (1 << 6);
    static constexpr uint8_t CFG_USE_MODS     = (1 << 7);

    // Key status register bits
    static constexpr uint8_t KEY_COUNT_MASK = 0x1F;
    static constexpr uint8_t KEY_CAPSLOCK   = (1 << 5);
    static constexpr uint8_t KEY_NUMLOCK    = (1 << 6);

    // Internal event ring buffer (no dynamic allocation)
    static constexpr int EVENT_BUF_SIZE = 16;
    KeyEvent _events[EVENT_BUF_SIZE];
    int _head = 0;
    int _tail = 0;
    int _count = 0;

    bool _present = false;

    // I2C helpers
    uint8_t readReg8(uint8_t reg) const;
    uint16_t readReg16(uint8_t reg) const;
    void writeReg(uint8_t reg, uint8_t value);

    void pushEvent(const KeyEvent& ev);
};

}  // namespace oms