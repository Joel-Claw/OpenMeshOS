// OpenMeshOS — ScreenHome.h
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Home / chat screen.  Shows the channel list, message bubbles,
// and the text input bar at the bottom.

#pragma once

#include <lvgl.h>

namespace oms { namespace ui {

class ScreenHome {
public:
    static void create();

private:
    static lv_obj_t* _screen;
    static lv_obj_t* _channelTab;
    static lv_obj_t* _msgList;
    static lv_obj_t* _inputBar;
};

}}  // namespace oms::ui