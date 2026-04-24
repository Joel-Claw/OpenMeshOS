// OpenMeshOS — ScreenTerminal.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Terminal screen. Provides a serial-style text interface
// for sending commands to the device (AT-style or shell).
// Input via BBQ10KB keyboard on T-Deck.

#pragma once

#include <lvgl.h>

namespace oms { namespace ui {

class ScreenTerminal {
public:
    static void create();

    // Navigation back to home
    static void goBack(lv_event_t* e);

    // Whether this screen is currently displayed
    static bool isActive();

    // Append output text to the terminal
    static void print(const char* text);

    // Feed a line of input (from keyboard)
    static void submitInput(const char* line);

    // State (public for static callback access)
    static lv_obj_t* _screen;
    static lv_obj_t* _outputArea;
    static lv_obj_t* _inputField;
    static lv_obj_t* _backBtn;
    static bool _active;
};

}}  // namespace oms::ui