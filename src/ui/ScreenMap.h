// OpenMeshOS — ScreenMap.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Offline map screen. Shows mesh nodes on a tile-based map
// rendered from SD card tiles. Pan with trackball, zoom with
// click+scroll.

#pragma once

#include <lvgl.h>

namespace oms { namespace ui {

class ScreenMap {
public:
    static void create();

    // Navigation back to home
    static void goBack(lv_event_t* e);

    // Update map display (called from tick when this screen is active)
    static void refresh();

    // Trackball input while on map screen
    static void feedInput(int16_t dx, int16_t dy, bool pressed);

    // Whether this screen is currently displayed
    static bool isActive();

private:
    static lv_obj_t* _screen;
    static lv_obj_t* _mapCanvas;
    static lv_obj_t* _zoomLabel;
    static lv_obj_t* _coordLabel;
    static lv_obj_t* _backBtn;
    static lv_obj_t* _nodeInfo;

    static bool _active;
    static int32_t _panAccX;  // accumulated pan pixels
    static int32_t _panAccY;
    static int8_t  _zoomHold; // 0=idle, 1=held for zoom mode
    static uint32_t _pressStart;
};

}}  // namespace oms::ui