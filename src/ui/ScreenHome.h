// OpenMeshOS — ScreenHome.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Home / chat screen.  Shows the channel list, message bubbles,
// and the text input bar at the bottom.

#pragma once

#include <lvgl.h>

namespace oms { namespace ui {

class ScreenHome {
public:
    static void create();

    /// Drain MessageBus inbox and add messages to the list.
    /// Call this from the main loop (after MeshService::tick()).
    static void updateMessages();

    /// Send the current textarea content as a mesh message.
    static void sendInput();

private:
    static lv_obj_t* _screen;
    static lv_obj_t* _channelTab;
    static lv_obj_t* _msgList;
    static lv_obj_t* _inputBar;
};

}}  // namespace oms::ui