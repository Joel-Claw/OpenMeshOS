// OpenMeshOS — KeyboardInput.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BBQ10KB to LVGL input bridge.
//
// In LVGL 9, keyboard input goes through the indev system.
// We maintain an internal key buffer that the LVGL indev driver
// reads from when polled.
//
// BBQ10KB special key codes (from KBD16C firmware):
//   0x08  Backspace  -> LV_KEY_BACKSPACE
//   0x0D  Enter      -> LV_KEY_ENTER
//   0x1B  Escape     -> LV_KEY_ESC
//   0x09  Tab        -> LV_KEY_NEXT
//   0x11  Sym        -> modifier (Fn-like)
//   0x12  Shift      -> modifier
//   0x13  Ctrl       -> modifier
//   0x14  Alt        -> modifier

#include "KeyboardInput.h"

namespace oms {

// ── LVGL indev read callback ─────────────────────────────────────────
// This is called by LVGL to read keyboard state.
void KeyboardInput::indevReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
    KeyboardInput *self = (KeyboardInput *)lv_indev_get_user_data(indev);
    if (!self || self->_keyBufLen == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = 0;
        data->continue_reading = false;
        return;
    }

    // Pop the oldest key from the buffer
    data->key = self->_keyBuf[0];
    data->state = LV_INDEV_STATE_PRESSED;

    // Shift buffer
    self->_keyBufLen--;
    for (uint8_t i = 0; i < self->_keyBufLen; i++) {
        self->_keyBuf[i] = self->_keyBuf[i + 1];
    }

    data->continue_reading = (self->_keyBufLen > 0);
}

// ── update ───────────────────────────────────────────────────────────
void KeyboardInput::update(Keyboard &kbd) {
    Keyboard::KeyEvent events[MAX_EVENTS];
    uint8_t count = kbd.poll(events, MAX_EVENTS);

    for (uint8_t i = 0; i < count; i++) {
        char key = events[i].key;
        auto state = events[i].state;

        // Only act on press events
        if (state == Keyboard::StateRelease) {
            switch (key) {
                case 0x12: _shift = false; break;
                case 0x13: _ctrl  = false; break;
                case 0x14: _alt   = false; break;
                case 0x11: _sym   = false; break;
            }
            continue;
        }

        if (state != Keyboard::StatePress && state != Keyboard::StateLong)
            continue;

        // Modifier keys: set flags, don't push to LVGL
        switch (key) {
            case 0x12: _shift = true; continue;
            case 0x13: _ctrl  = true; continue;
            case 0x14: _alt   = true; continue;
            case 0x11: _sym   = true; continue;
        }

        // Map to LVGL key code
        lv_key_t lvKey;
        if (mapSpecialKey(key, lvKey)) {
            pushKey(lvKey);
            continue;
        }

        // Printable character
        if (key >= 'a' && key <= 'z' && _shift) {
            key = key - 'a' + 'A';
        }
        pushKey((lv_key_t)key);
    }
}

// ── mapSpecialKey ────────────────────────────────────────────────────
bool KeyboardInput::mapSpecialKey(char key, lv_key_t &lvKey) {
    switch (key) {
        case 0x08: lvKey = LV_KEY_BACKSPACE; return true;
        case 0x0D: lvKey = LV_KEY_ENTER;     return true;
        case 0x1B: lvKey = LV_KEY_ESC;       return true;
        case 0x09: lvKey = LV_KEY_NEXT;      return true;
        default:   return false;
    }
}

// ── pushKey ──────────────────────────────────────────────────────────
void KeyboardInput::pushKey(lv_key_t key) {
    if (_keyBufLen >= KEY_BUF_SIZE) return;
    _keyBuf[_keyBufLen++] = key;
}

// ── initIndev ────────────────────────────────────────────────────────
void KeyboardInput::initIndev() {
    _kbIndev = lv_indev_create();
    lv_indev_set_type(_kbIndev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(_kbIndev, indevReadCb);
    lv_indev_set_user_data(_kbIndev, this);

    // Assign default group
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(_kbIndev, g);
}

}  // namespace oms