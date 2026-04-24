// OpenMeshOS — ScreenSettings.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Settings screen. Layout:
//
//   +------------------------------+ 240px
//   | [←]  Settings                |  top bar (28px)
//   |------------------------------|
//   | Device Info          >       |  menu items
//   | Mesh Config          >       |
//   | Export / Import       >       |
//   | About                 >       |
//   |                              |
//   |                              |
//   | v0.1.0-alpha.1               |  version
//   +------------------------------+ 320px wide
//
// Each menu item opens a sub-page within this screen
// (we reuse the screen object and swap content).

#include "ScreenSettings.h"
#include "ScreenHome.h"
#include "Theme.h"
#include "../utils/Config.h"
#include "../utils/ConfigExport.h"
#include "../utils/Log.h"
#include "../version.h"

namespace oms { namespace ui {

lv_obj_t* ScreenSettings::_screen       = nullptr;
lv_obj_t* ScreenSettings::_menuList      = nullptr;
lv_obj_t* ScreenSettings::_versionLabel  = nullptr;
bool      ScreenSettings::_active        = false;

static void back_cb(lv_event_t* e) {
    ScreenSettings::goBack(e);
}

// Menu item click handlers
static void device_info_cb(lv_event_t* e) {
    ScreenSettings::showDeviceInfo();
}

static void mesh_config_cb(lv_event_t* e) {
    ScreenSettings::showMeshConfig();
}

static void export_import_cb(lv_event_t* e) {
    ScreenSettings::showExportImport();
}

static void about_cb(lv_event_t* e) {
    ScreenSettings::showAbout();
}

void ScreenSettings::create() {
    OMS_LOG("UI", "Creating settings screen");

    _active = true;

    lv_obj_t* old = lv_screen_active();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, OMS_SCREEN_W, OMS_SCREEN_H);
    lv_obj_set_style_bg_color(_screen, theme::BG, 0);

    // ── Top bar (28px) ────────────────────────────────────────────────
    lv_obj_t* topbar = lv_obj_create(_screen);
    lv_obj_set_size(topbar, OMS_SCREEN_W, 28);
    lv_obj_align(topbar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(topbar, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(topbar, 0, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 2, 0);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(topbar, 6, 0);

    lv_obj_t* back_btn = lv_button_create(topbar);
    lv_obj_set_size(back_btn, 24, 24);
    lv_obj_set_style_bg_color(back_btn, theme::ACCENT, 0);
    lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme::BG, 0);

    lv_obj_t* title = lv_label_create(topbar);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, theme::TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // ── Menu list ─────────────────────────────────────────────────────
    _menuList = lv_list_create(_screen);
    lv_obj_set_size(_menuList, OMS_SCREEN_W, OMS_SCREEN_H - 28 - 20);
    lv_obj_align(_menuList, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_style_bg_color(_menuList, theme::BG, 0);
    lv_obj_set_style_border_width(_menuList, 0, 0);

    lv_obj_t* item;

    item = lv_list_add_btn(_menuList, LV_SYMBOL_WARNING, "Device Info");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, device_info_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_WIFI, "Mesh Config");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, mesh_config_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_SAVE, "Export / Import");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, export_import_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_IMAGE, "About");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, about_cb, LV_EVENT_CLICKED, nullptr);

    // ── Version label (bottom) ────────────────────────────────────────
    _versionLabel = lv_label_create(_screen);
    lv_label_set_text(_versionLabel, OMS_VERSION_STRING);
    lv_obj_set_style_text_color(_versionLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(_versionLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(_versionLabel, LV_ALIGN_BOTTOM_RIGHT, -4, -2);

    // ── Load ──────────────────────────────────────────────────────────
    lv_screen_load(_screen);
    if (old) lv_obj_del(old);
}

void ScreenSettings::goBack(lv_event_t* e) {
    (void)e;
    _active = false;
    ScreenHome::create();
}

bool ScreenSettings::isActive() {
    return _active;
}

// ── Sub-pages ──────────────────────────────────────────────────────────
// Each sub-page clears _menuList and fills it with the sub-page content.
// A "Back" item at top returns to the main settings menu.

static void settings_menu_back_cb(lv_event_t* e) {
    // Recreate the settings screen to get back to main menu
    lv_obj_del(ScreenSettings::_screen);
    ScreenSettings::create();
}

static lv_obj_t* create_sub_page(const char* title) {
    // Delete old menu content
    lv_obj_clean(ScreenSettings::_menuList);

    // Add back button at top
    lv_obj_t* back_item = lv_list_add_btn(ScreenSettings::_menuList,
        LV_SYMBOL_LEFT, title);
    lv_obj_set_style_text_color(back_item, theme::ACCENT, 0);
    lv_obj_add_event_cb(back_item, settings_menu_back_cb, LV_EVENT_CLICKED, nullptr);

    return ScreenSettings::_menuList;
}

void ScreenSettings::showDeviceInfo() {
    OMS_LOG("UI", "Settings: device info");
    lv_obj_t* list = create_sub_page("Device Info");

    char buf[64];

    snprintf(buf, sizeof(buf), "Board: T-Deck");
    lv_obj_t* item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Flash: 16MB  PSRAM: 8MB");
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Firmware: " OMS_VERSION_STRING);
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Heap free: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Uptime: %lu s", (unsigned long)(millis() / 1000));
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
}

void ScreenSettings::showMeshConfig() {
    OMS_LOG("UI", "Settings: mesh config");
    lv_obj_t* list = create_sub_page("Mesh Config");

    const auto& cfg = oms::config::get();

    char buf[64];

    snprintf(buf, sizeof(buf), "Region: %s", cfg.radioRegion);
    lv_obj_t* item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Callsign: %s", cfg.callsign);
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Channel: %d", cfg.channel);
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    snprintf(buf, sizeof(buf), "Brightness: %d", cfg.brightness);
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    item = lv_list_add_btn(list, nullptr, "Reconnect on change: restart");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
}

void ScreenSettings::showExportImport() {
    OMS_LOG("UI", "Settings: export/import");
    lv_obj_t* list = create_sub_page("Export / Import");

    lv_obj_t* item;

    item = lv_list_add_btn(list, LV_SYMBOL_UPLOAD, "Export to SD");
    lv_obj_set_style_text_color(item, theme::GREEN, 0);

    item = lv_list_add_btn(list, LV_SYMBOL_DOWNLOAD, "Import from SD");
    lv_obj_set_style_text_color(item, theme::ACCENT, 0);

    item = lv_list_add_btn(list, nullptr, "Formats: config.json,");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
    item = lv_list_add_btn(list, nullptr, "  identity.id, regions.bin");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
}

void ScreenSettings::showAbout() {
    OMS_LOG("UI", "Settings: about");
    lv_obj_t* list = create_sub_page("About");

    lv_obj_t* item;

    item = lv_list_add_btn(list, nullptr, "OpenMeshOS");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);

    item = lv_list_add_btn(list, nullptr, OMS_VERSION_STRING);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    item = lv_list_add_btn(list, nullptr, "WTFPL v2");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    item = lv_list_add_btn(list, nullptr, "Vibecoded by GLM-5.1");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    item = lv_list_add_btn(list, nullptr, "github.com/Joel-Claw/");
    lv_obj_set_style_text_color(item, theme::ACCENT, 0);
}

}}  // namespace oms::ui