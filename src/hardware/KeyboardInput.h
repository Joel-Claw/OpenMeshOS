// OpenMeshOS — KeyboardInput.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Bridges BBQ10KB keyboard events to LVGL input.
// Uses LVGL 9 indev keypad driver to inject key events.

#pragma once

#include <lvgl.h>
#include "Keyboard.h"

namespace oms {

class KeyboardInput {
public:
    KeyboardInput() = default;

    // Create LVGL indev for keyboard input (call once after lv_init)
    void initIndev();

    // Process keyboard events and push to LVGL key buffer
    // Call from main loop after board.tick()
    void update(Keyboard &kbd);

    // Get LVGL indev handle
    lv_indev_t *indev() const { return _kbIndev; }

    // Modifier state queries
    bool shiftHeld() const { return _shift; }
    bool ctrlHeld() const  { return _ctrl; }
    bool altHeld() const   { return _alt; }
    bool symHeld() const   { return _sym; }

    // LVGL indev read callback (static, accesses this via user_data)
    static void indevReadCb(lv_indev_t *indev, lv_indev_data_t *data);

private:
    // Map raw BBQ10KB key code to LVGL key
    bool mapSpecialKey(char key, lv_key_t &lvKey);

    // Push a key into the LVGL buffer
    void pushKey(lv_key_t key);

    bool _shift = false;
    bool _ctrl  = false;
    bool _alt   = false;
    bool _sym   = false;

    lv_indev_t *_kbIndev = nullptr;

    // Key buffer for LVGL indev driver
    static constexpr uint8_t KEY_BUF_SIZE = 16;
    lv_key_t _keyBuf[KEY_BUF_SIZE];
    uint8_t  _keyBufLen = 0;

    // Event buffer from keyboard poll
    static constexpr uint8_t MAX_EVENTS = 8;
};

}  // namespace oms