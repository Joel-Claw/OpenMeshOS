// OpenMeshOS — ScreenScanner.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Repeater/node scanner screen. Shows discovered mesh nodes with
// type, signal strength, distance (if GPS available), and age.
// Supports whitelist toggling for repeaters.

#pragma once

#include <lvgl.h>

namespace oms { namespace ui {

class ScreenScanner {
public:
    static void create();

    // Navigation
    static void goBack(lv_event_t* e);

    // Refresh node list from NodeTracker
    static void refreshList();

    // Whether this screen is currently displayed
    static bool isActive();

    // Tick — call from main loop to update RSSI/age
    static void tick();

    // State
    static lv_obj_t* _screen;
    static lv_obj_t* _list;
    static lv_obj_t* _countLabel;
    static lv_obj_t* _scanLabel;
    static bool _active;
    static bool _scanning;
};

}}  // namespace oms::ui