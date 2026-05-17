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
#include "Theme.h"
#include "../mesh/MeshService.h"
#include "../mesh/TDeckBoard.h"
#include "../hardware/Board.h"
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

static void display_cb(lv_event_t* e) {
    ScreenSettings::showDisplay();
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

    item = lv_list_add_btn(_menuList, LV_SYMBOL_IMAGE, "Display");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, display_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_SAVE, "Export / Import");
    lv_obj_set_style_text_color(item, theme::TEXT, 0);
    lv_obj_add_event_cb(item, export_import_cb, LV_EVENT_CLICKED, nullptr);

    item = lv_list_add_btn(_menuList, LV_SYMBOL_BELL, "About");
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

}}  // namespace oms::ui