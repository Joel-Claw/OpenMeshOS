// OpenMeshOS — ScreenHome.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Home / chat screen.  Shows the channel tabs, message bubbles,
// status bar with battery/RSSI, and the text input bar.

#pragma once

#include <lvgl.h>
#include <cstdint>

namespace oms { namespace ui {

/// Active channel/tab identifier
enum class ChatTab : uint8_t {
    Public = 0,   // #Public channel (channel index 0)
    Channel1 = 1, // Custom channel 1
    DM = 2,       // Direct messages
};

class ScreenHome {
public:
    static void create();

    /// Drain MessageBus inbox and add messages to the list.
    /// Call this from the main loop (after MeshService::tick()).
    static void updateMessages();

    /// Send the current textarea content as a mesh message.
    static void sendInput();

    /// Switch to a different channel tab.
    static void switchTab(ChatTab tab);

    /// Get the current active tab.
    static ChatTab currentTab() { return _activeTab; }

    /// Periodic status bar update (called from tick).
    static void updateStatusBar();

    /// Re-render message list from current tab's buffer.
    static void refreshMessageList();

private:
    static lv_obj_t* _screen;
    static lv_obj_t* _statusBar;
    static lv_obj_t* _msgList;
    static lv_obj_t* _inputBar;
    static lv_obj_t* _battLabel;
    static lv_obj_t* _rssiLabel;

    static ChatTab _activeTab;
    static uint32_t _lastStatusUpdate;
};

}}  // namespace oms::ui