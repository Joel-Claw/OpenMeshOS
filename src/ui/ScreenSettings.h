// OpenMeshOS — ScreenSettings.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Settings screen. Device config, mesh params, export/import.

#pragma once

#include <lvgl.h>

namespace oms { namespace ui {

class ScreenSettings {
public:
    static void create();

    // Navigation back to home
    static void goBack(lv_event_t* e);

    // Whether this screen is currently displayed
    static bool isActive();

    // Sub-pages
    static void showDeviceInfo();
    static void showMeshConfig();
    static void showExportImport();
    static void showAbout();

    // State (public for static callback access)
    static lv_obj_t* _screen;
    static lv_obj_t* _menuList;
    static lv_obj_t* _versionLabel;
    static bool _active;
};

}}  // namespace oms::ui