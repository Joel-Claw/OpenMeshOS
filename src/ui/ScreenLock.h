// OpenMeshOS — ScreenLock.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Lock screen. Shows time, date, battery, and node count.
// Press any key or trackball to unlock.

#pragma once

#include <lvgl.h>
#include <cstdint>

namespace oms { namespace ui {

class ScreenLock {
public:
    /// Show the lock screen (called after screen timeout).
    static void create();

    /// Dismiss the lock screen and return to home.
    static void unlock();

    /// Whether the lock screen is currently active.
    static bool isActive() { return _active; }

    /// Called from main loop to update time and battery display.
    static void update();

    /// Reset the inactivity timer (call on any user input).
    static void resetIdleTimer();

    /// Check if the screen should be locked (called from tick).
    static void checkIdle();

private:
    static lv_obj_t* _screen;
    static lv_obj_t* _timeLabel;
    static lv_obj_t* _dateLabel;
    static lv_obj_t* _battLabel;
    static lv_obj_t* _nodeLabel;
    static lv_obj_t* _hintLabel;

    static bool _active;
    static bool _dimmed;       // backlight dimmed but not locked
    static uint32_t _lastActivity;
    static uint32_t _lockTimeoutMs;
};

}}  // namespace oms::ui