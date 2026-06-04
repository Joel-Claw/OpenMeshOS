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
//   | Display              >       |
//   | Export / Import       >       |
//   | About                 >       |
//   |                              |
//   | v0.1.0-alpha.1               |  version
//   +------------------------------+ 320px wide
//
// Each menu item opens a sub-page with interactive controls.
// Mesh Config: callsign (textarea), region (roller).
// Display: brightness (slider), timeout (roller), sound (switch).

#include "ScreenSettings.h"
#include "ScreenHome.h"
#include "ScreenScanner.h"
#include "Theme.h"
#include "../mesh/MeshService.h"
#include "../mesh/NodeTracker.h"
#include "../mesh/BLECompanion.h"
#include "../mesh/TDeckBoard.h"
#include "../hardware/Board.h"
#include "../utils/Config.h"
#include "../utils/ConfigExport.h"
#include "../utils/Log.h"
#include "../version.h"
#include <Update.h>
#include <SD.h>

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

static void display_cb(lv_event_t* e) {
    ScreenSettings::showDisplay();
}

static void export_import_cb(lv_event_t* e) {
    ScreenSettings::showExportImport();
}

static void about_cb(lv_event_t* e) {
    ScreenSettings::showAbout();
}

static void scanner_cb(lv_event_t* e) {
    oms::ui::ScreenScanner::create();
}

static void whitelist_cb(lv_event_t* e) {
    ScreenSettings::showWhitelist();
}

static void ota_cb(lv_event_t* e) {
    ScreenSettings::showOTAUpdate();
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

    item = lv_list_add_btn(_menuList, LV_SYMBOL_IMAGE, "Display");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, display_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_SAVE, "Export / Import");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, export_import_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_BELL, "About");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, about_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_WIFI, "Node Scanner");
    lv_obj_set_style_text_color(item, theme::GREEN, 0);
    lv_obj_add_event_cb(item, scanner_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_OK, "Whitelist");
    lv_obj_set_style_text_color(item, theme::ACCENT, 0);
    lv_obj_add_event_cb(item, whitelist_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_UPLOAD, "OTA Update");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, ota_cb, LV_EVENT_CLICKED, nullptr);

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

// ── Device Info ────────────────────────────────────────────────────
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

    // Battery voltage (live)
    if (MeshService::instance().initialized()) {
        uint16_t mv = MeshService::instance().board().getBattMilliVolts();
        snprintf(buf, sizeof(buf), "Battery: %.2f V", mv / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "Battery: --");
    }
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    // MCU temperature
    if (MeshService::instance().initialized()) {
        float temp = MeshService::instance().board().getMCUTemperature();
        snprintf(buf, sizeof(buf), "MCU temp: %.1f C", temp);
    } else {
        snprintf(buf, sizeof(buf), "MCU temp: --");
    }
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    // GPS info (if present)
    if (oms::Board::instance().hasGPSFix()) {
        snprintf(buf, sizeof(buf), "GPS: %.4f %.4f (%d sats)",
                 oms::Board::instance().gpsLat(),
                 oms::Board::instance().gpsLng(),
                 oms::Board::instance().gpsSatellites());
    } else {
        snprintf(buf, sizeof(buf), "GPS: no fix");
    }
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
}

// ── Mesh Config (interactive) ──────────────────────────────────────
static lv_obj_t* s_callsignArea = nullptr;
static lv_obj_t* s_regionRoller = nullptr;

static void save_callsign_cb(lv_event_t* e) {
    if (!s_callsignArea) return;
    const char* text = lv_textarea_get_text(s_callsignArea);
    if (text && text[0] != '\0') {
        oms::config::setCallsign(text);
        OMS_LOG("UI", "Callsign set to: %s", text);
    }
}

static void save_region_cb(lv_event_t* e) {
    if (!s_regionRoller) return;
    uint16_t sel = lv_roller_get_selected(s_regionRoller);
    const char* regions[] = {"EU868", "US915", "AU915", "AS923", "KR920", "IN865"};
    if (sel < 6) {
        oms::config::setRegion(regions[sel]);
        OMS_LOG("UI", "Region set to: %s (restart required)", regions[sel]);
    }
}

void ScreenSettings::showMeshConfig() {
    OMS_LOG("UI", "Settings: mesh config");
    lv_obj_t* list = create_sub_page("Mesh Config");

    const auto& cfg = oms::config::get();

    // ── Callsign ─────────────────────────────────────────────────
    lv_obj_t* label = lv_label_create(list);
    lv_label_set_text(label, "Callsign:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);

    s_callsignArea = lv_textarea_create(list);
    lv_textarea_set_text(s_callsignArea, cfg.callsign);
    lv_textarea_set_max_length(s_callsignArea, 15);
    lv_textarea_set_one_line(s_callsignArea, true);
    lv_obj_set_width(s_callsignArea, OMS_SCREEN_W - 20);
    lv_obj_set_style_bg_color(s_callsignArea, theme::BG, 0);
    lv_obj_set_style_text_color(s_callsignArea, theme::TEXT, 0);
    lv_obj_add_event_cb(s_callsignArea, save_callsign_cb, LV_EVENT_READY, nullptr);

    // ── Region ────────────────────────────────────────────────────
    label = lv_label_create(list);
    lv_label_set_text(label, "Radio Region:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);

    s_regionRoller = lv_roller_create(list);
    lv_roller_set_options(s_regionRoller,
        "EU868\nUS915\nAU915\nAS923\nKR920\nIN865",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_regionRoller, 3);
    lv_obj_set_width(s_regionRoller, OMS_SCREEN_W - 20);

    // Set current selection
    const char* regions[] = {"EU868", "US915", "AU915", "AS923", "KR920", "IN865"};
    uint16_t sel = 0;
    for (int i = 0; i < 6; i++) {
        if (strncmp(cfg.radioRegion, regions[i], sizeof(cfg.radioRegion)) == 0) {
            sel = i;
            break;
        }
    }
    lv_roller_set_selected(s_regionRoller, sel, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_regionRoller, save_region_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── Info ──────────────────────────────────────────────────────
    char buf[64];
    snprintf(buf, sizeof(buf), "Channel: %d", cfg.channel);
    lv_obj_t* item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    item = lv_list_add_btn(list, nullptr, "Changes auto-saved.");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    // ── TX Power ──────────────────────────────────────────────────
    label = lv_label_create(list);
    lv_label_set_text(label, "TX Power:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);

    static lv_obj_t* s_txPowerSlider = nullptr;
    s_txPowerSlider = lv_slider_create(list);
    lv_slider_set_range(s_txPowerSlider, 5, 22);  // 5-22 dBm for SX1262
    lv_slider_set_value(s_txPowerSlider, cfg.txPower, LV_ANIM_OFF);
    lv_obj_set_width(s_txPowerSlider, OMS_SCREEN_W - 30);
    lv_obj_add_event_cb(s_txPowerSlider, [](lv_event_t* e) {
        int val = lv_slider_get_value(s_txPowerSlider);
        oms::config::setTxPower(val);
        OMS_LOG("UI", "TX power: %d dBm", val);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    static lv_obj_t* s_txPowerLabel = nullptr;
    s_txPowerLabel = lv_label_create(list);
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d dBm", cfg.txPower);
    lv_label_set_text(s_txPowerLabel, pbuf);
    lv_obj_set_style_text_color(s_txPowerLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(s_txPowerLabel, &lv_font_montserrat_10, 0);
    // Update label when slider changes
    lv_obj_add_event_cb(s_txPowerSlider, [](lv_event_t* e) {
        int val = lv_slider_get_value(s_txPowerSlider);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d dBm", val);
        lv_label_set_text(s_txPowerLabel, buf);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    item = lv_list_add_btn(list, nullptr, "Region: restart to apply.");
    lv_obj_set_style_text_color(item, theme::ORANGE, 0);
}

// ── Display Settings (interactive) ─────────────────────────────────
static lv_obj_t* s_brightnessSlider = nullptr;
static lv_obj_t* s_timeoutRoller = nullptr;
static lv_obj_t* s_soundSwitch = nullptr;

static void brightness_cb(lv_event_t* e) {
    if (!s_brightnessSlider) return;
    int val = lv_slider_get_value(s_brightnessSlider);
    oms::Config mutable_cfg = oms::config::get();
    mutable_cfg.brightness = val;
    // Apply immediately to backlight
    oms::Board::instance().setBacklight(val > 0);
    // Save to SPIFFS
    oms::config::save();
    OMS_LOG("UI", "Brightness: %d", val);
}

static void timeout_cb(lv_event_t* e) {
    if (!s_timeoutRoller) return;
    uint16_t sel = lv_roller_get_selected(s_timeoutRoller);
    const int timeouts[] = {10, 15, 30, 60, 120, 0};  // 0 = never
    oms::Config mutable_cfg = oms::config::get();
    mutable_cfg.screenTimeoutSec = timeouts[sel];
    oms::config::save();
    OMS_LOG("UI", "Screen timeout: %d s", timeouts[sel]);
}

static void sound_cb(lv_event_t* e) {
    if (!s_soundSwitch) return;
    bool on = lv_obj_has_state(s_soundSwitch, LV_STATE_CHECKED);
    oms::Config mutable_cfg = oms::config::get();
    mutable_cfg.notifySound = on;
    oms::config::save();
    OMS_LOG("UI", "Sound: %s", on ? "on" : "off");
}

void ScreenSettings::showDisplay() {
    OMS_LOG("UI", "Settings: display");
    lv_obj_t* list = create_sub_page("Display");

    const auto& cfg = oms::config::get();

    // ── Brightness ────────────────────────────────────────────────
    lv_obj_t* label = lv_label_create(list);
    lv_label_set_text(label, "Brightness:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);

    s_brightnessSlider = lv_slider_create(list);
    lv_slider_set_range(s_brightnessSlider, 20, 255);
    lv_slider_set_value(s_brightnessSlider, cfg.brightness, LV_ANIM_OFF);
    lv_obj_set_width(s_brightnessSlider, OMS_SCREEN_W - 30);
    lv_obj_add_event_cb(s_brightnessSlider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── Screen Timeout ───────────────────────────────────────────
    label = lv_label_create(list);
    lv_label_set_text(label, "Screen Timeout:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);

    s_timeoutRoller = lv_roller_create(list);
    lv_roller_set_options(s_timeoutRoller,
        "10s\n15s\n30s\n60s\n120s\nNever",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_timeoutRoller, 3);
    lv_obj_set_width(s_timeoutRoller, OMS_SCREEN_W - 20);

    // Set current selection
    const int timeouts[] = {10, 15, 30, 60, 120, 0};
    uint16_t sel = 0;
    for (int i = 0; i < 6; i++) {
        if (cfg.screenTimeoutSec == timeouts[i]) { sel = i; break; }
    }
    lv_roller_set_selected(s_timeoutRoller, sel, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_timeoutRoller, timeout_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── Sound ─────────────────────────────────────────────────────
    lv_obj_t* row = lv_obj_create(list);
    lv_obj_set_size(row, OMS_SCREEN_W - 20, 30);
    lv_obj_set_style_bg_color(row, theme::BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label = lv_label_create(row);
    lv_label_set_text(label, "Sound on message:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);
    lv_obj_set_flex_grow(label, 1);

    s_soundSwitch = lv_switch_create(row);
    if (cfg.notifySound) {
        lv_obj_add_state(s_soundSwitch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_soundSwitch, sound_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── Theme toggle (dark/light) ──────────────────────────────────
    lv_obj_t* theme_row = lv_obj_create(list);
    lv_obj_set_size(theme_row, OMS_SCREEN_W - 20, 30);
    lv_obj_set_style_bg_color(theme_row, theme::BG, 0);
    lv_obj_set_style_border_width(theme_row, 0, 0);
    lv_obj_set_flex_flow(theme_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(theme_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label = lv_label_create(theme_row);
    lv_label_set_text(label, "Light theme:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);
    lv_obj_set_flex_grow(label, 1);

    static lv_obj_t* s_themeSwitch = nullptr;
    s_themeSwitch = lv_switch_create(theme_row);
    if (cfg.theme == 1) {
        lv_obj_add_state(s_themeSwitch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_themeSwitch, [](lv_event_t* e) {
        bool on = lv_obj_has_state(s_themeSwitch, LV_STATE_CHECKED);
        oms::config::setTheme(on ? 1 : 0);
        theme::setLightMode(on);
        OMS_LOG("UI", "Theme: %s", on ? "light" : "dark");
        // Note: full theme refresh requires re-creating current screen
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── BLE Companion ─────────────────────────────────────────────
    lv_obj_t* ble_row = lv_obj_create(list);
    lv_obj_set_size(ble_row, OMS_SCREEN_W - 20, 30);
    lv_obj_set_style_bg_color(ble_row, theme::BG, 0);
    lv_obj_set_style_border_width(ble_row, 0, 0);
    lv_obj_set_flex_flow(ble_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ble_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label = lv_label_create(ble_row);
    lv_label_set_text(label, "BLE companion:");
    lv_obj_set_style_text_color(label, theme::TEXT, 0);
    lv_obj_set_flex_grow(label, 1);

    static lv_obj_t* s_bleSwitch = nullptr;
    s_bleSwitch = lv_switch_create(ble_row);
    if (BLECompanion::instance().enabled()) {
        lv_obj_add_state(s_bleSwitch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_bleSwitch, [](lv_event_t* e) {
        bool on = lv_obj_has_state(s_bleSwitch, LV_STATE_CHECKED);
        BLECompanion::instance().setEnabled(on);
        OMS_LOG("UI", "BLE: %s", on ? "on" : "off");
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

// ── Export / Import ────────────────────────────────────────────────
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

// ── About ──────────────────────────────────────────────────────────
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

// ── OTA Firmware Update ──────────────────────────────────────────────

static lv_obj_t* s_otaStatusLabel = nullptr;
static lv_obj_t* s_otaProgress = nullptr;
static bool s_otaInProgress = false;

static void ota_start_sd_cb(lv_event_t* e) {
    if (s_otaInProgress) return;

    // Check SD card
    if (!SD.begin(39)) {  // T-Deck SD card CS (official: BOARD_SDCARD_CS=39)
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, "No SD card!");
        return;
    }

    const char* firmwarePath = "/oms/firmware.bin";
    if (!SD.exists(firmwarePath)) {
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, "No /oms/firmware.bin");
        return;
    }

    s_otaInProgress = true;
    if (s_otaStatusLabel)
        lv_label_set_text(s_otaStatusLabel, "Reading firmware.bin...");

    File f = SD.open(firmwarePath, FILE_READ);
    if (!f) {
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, "Failed to open file");
        s_otaInProgress = false;
        return;
    }

    size_t fileSize = f.size();
    if (fileSize == 0 || fileSize > 6400000) {
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, "Invalid firmware size");
        f.close();
        s_otaInProgress = false;
        return;
    }

    if (!Update.begin(fileSize)) {
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, "Not enough OTA space");
        f.close();
        s_otaInProgress = false;
        return;
    }

    if (s_otaStatusLabel)
        lv_label_set_text(s_otaStatusLabel, "Flashing...");

    size_t written = Update.writeStream(f);
    f.close();

    if (written == fileSize && Update.end(true)) {
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, "Success! Rebooting...");
        delay(2000);
        ESP.restart();
    } else {
        char errBuf[48];
        snprintf(errBuf, sizeof(errBuf), "Failed: %s", Update.getError() == UPDATE_ERROR_OK ? "size mismatch" : "write error");
        if (s_otaStatusLabel)
            lv_label_set_text(s_otaStatusLabel, errBuf);
        s_otaInProgress = false;
    }
}

static void ota_start_ble_cb(lv_event_t* e) {
    // BLE OTA is now supported via the companion app
    // The companion app sends firmware size, then chunks, then end command
    if (s_otaStatusLabel)
        lv_label_set_text(s_otaStatusLabel, "Use companion app for BLE OTA");
    OMS_LOG("UI", "BLE OTA: available via companion app");
}

void ScreenSettings::showOTAUpdate() {
    OMS_LOG("UI", "Settings: OTA update");
    lv_obj_t* list = create_sub_page("OTA Update");

    lv_obj_t* item;

    // Current version
    char buf[48];
    snprintf(buf, sizeof(buf), "Current: " OMS_VERSION_STRING);
    item = lv_list_add_btn(list, nullptr, buf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

    // Update from SD card
    item = lv_list_add_btn(list, LV_SYMBOL_SD_CARD, "Update from SD");
    lv_obj_set_style_text_color(item, theme::GREEN, 0);
    lv_obj_add_event_cb(item, ota_start_sd_cb, LV_EVENT_CLICKED, nullptr);

    // Update via BLE (future)
    item = lv_list_add_btn(list, LV_SYMBOL_BLUETOOTH, "Update via BLE");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
    lv_obj_add_event_cb(item, ota_start_ble_cb, LV_EVENT_CLICKED, nullptr);

    // Status label
    s_otaStatusLabel = lv_label_create(list);
    lv_label_set_text(s_otaStatusLabel, "Place /oms/firmware.bin on SD");
    lv_obj_set_style_text_color(s_otaStatusLabel, theme::TEXT, 0);
    lv_obj_set_style_text_font(s_otaStatusLabel, &lv_font_montserrat_10, 0);

    // Progress bar
    s_otaProgress = lv_bar_create(list);
    lv_obj_set_width(s_otaProgress, OMS_SCREEN_W - 30);
    lv_obj_set_height(s_otaProgress, 10);

    // Warning
    item = lv_list_add_btn(list, LV_SYMBOL_WARNING, "Do not power off during update!");
    lv_obj_set_style_text_color(item, theme::ORANGE, 0);
}

// ── Whitelist Management ───────────────────────────────────────────
void ScreenSettings::showWhitelist() {
    OMS_LOG("UI", "Settings: whitelist");
    lv_obj_t* list = create_sub_page("Whitelist");

    NodeTracker& tracker = NodeTracker::instance();
    size_t total = tracker.count();
    size_t wlCount = tracker.countWhitelisted();

    // Count display
    char headerBuf[48];
    snprintf(headerBuf, sizeof(headerBuf), "%zu of %zu nodes whitelisted", wlCount, total);
    lv_obj_t* item = lv_list_add_btn(list, nullptr, headerBuf);
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(item, &lv_font_montserrat_10, 0);

    if (wlCount == 0) {
        item = lv_list_add_btn(list, nullptr, "No whitelisted nodes.");
        lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);

        item = lv_list_add_btn(list, nullptr, "Long-press a node in");
        lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_10, 0);
        item = lv_list_add_btn(list, nullptr, "Node Scanner to add.");
        lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_10, 0);
        return;
    }

    // List each whitelisted node with a remove button
    for (size_t i = 0; i < total; i++) {
        const TrackedNode* node = tracker.get(i);
        if (!node || !node->whitelisted) continue;

        const char* typeStr = "??";
        switch (node->type) {
            case NODE_TYPE_CHAT:     typeStr = "CH"; break;
            case NODE_TYPE_REPEATER: typeStr = "RP"; break;
            case NODE_TYPE_ROOM:     typeStr = "RM"; break;
            case NODE_TYPE_SENSOR:   typeStr = "SE"; break;
        }

        char rowText[64];
        snprintf(rowText, sizeof(rowText), "%s %s", typeStr, node->name);

        // Click removes from whitelist
        item = lv_list_add_btn(list, LV_SYMBOL_OK, rowText);
        lv_obj_set_style_text_color(item, theme::GREEN, 0);

        // User data carries the index
        lv_obj_add_event_cb(item, [](lv_event_t* e) {
            size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
            NodeTracker::instance().toggleWhitelist(idx);  // toggle off + save
            OMS_LOG("UI", "Removed node %zu from whitelist", idx);
            // Refresh the whitelist page
            ScreenSettings::showWhitelist();
        }, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }

    // Clear all button
    item = lv_list_add_btn(list, LV_SYMBOL_TRASH, "Clear all");
    lv_obj_set_style_text_color(item, theme::RED, 0);
    lv_obj_add_event_cb(item, [](lv_event_t* e) {
        // Remove whitelist from all tracked nodes
        NodeTracker& t = NodeTracker::instance();
        for (size_t i = 0; i < t.count(); i++) {
            const TrackedNode* n = t.get(i);
            if (n && n->whitelisted) {
                t.toggleWhitelist(i);  // toggle off + save each
            }
        }
        OMS_LOG("UI", "Whitelist cleared");
        ScreenSettings::showWhitelist();
    }, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(list, nullptr, "Tap a node to remove.");
    lv_obj_set_style_text_color(item, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(item, &lv_font_montserrat_10, 0);
}

}}  // namespace oms::ui