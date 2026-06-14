// OpenMeshOS — ScreenScanner.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Repeater/node scanner screen. Layout:
//
//   +------------------------------+ 240px
//   | [←]  Node Scanner    [scan]  |  top bar (28px)
//   |------------------------------|
//   | TY Name       Sig dBm  km Age |  column header
//   | CH Alpha      ++   -42  1.2 now|
//   | RP Repeater-1  +   -67  3.5 5m |
//   | SE Sensor-NE   -   -89  --   2h|
//   |                              |
//   | Nodes: 3   Repeaters: 1     |  status bar
//   +------------------------------+ 320px wide
//
// "Scan" button toggles scanning (starts/stops MeshCore advert
// listening). Each row shows node type prefix, name, RSSI, and
// estimated distance if GPS is available. Long-press toggles
// whitelist on a node (star icon).

#include "ScreenScanner.h"
#include "ScreenHome.h"
#include "Theme.h"
#include "../mesh/NodeTracker.h"
#include "../mesh/MeshService.h"
#include "../hardware/IBoard.h"
#include "../utils/Log.h"

namespace oms { namespace ui {

lv_obj_t* ScreenScanner::_screen      = nullptr;
lv_obj_t* ScreenScanner::_list        = nullptr;
lv_obj_t* ScreenScanner::_countLabel  = nullptr;
lv_obj_t* ScreenScanner::_scanLabel   = nullptr;
bool      ScreenScanner::_active      = false;
bool      ScreenScanner::_scanning    = true;  // start scanning by default

// ── Type prefix for display ────────────────────────────────────────
static const char* typePrefix(uint8_t type) {
    switch (type) {
        case NODE_TYPE_CHAT:     return "CH";
        case NODE_TYPE_REPEATER: return "RP";
        case NODE_TYPE_ROOM:     return "RM";
        case NODE_TYPE_SENSOR:   return "SE";
        default:                 return "??";
    }
}

// ── Back button ────────────────────────────────────────────────────
static void back_cb(lv_event_t* e) {
    ScreenScanner::goBack(e);
}

// ── Scan toggle ────────────────────────────────────────────────────
static void scan_toggle_cb(lv_event_t* e) {
    ScreenScanner::_scanning = !ScreenScanner::_scanning;
    if (ScreenScanner::_scanning) {
        lv_label_set_text(ScreenScanner::_scanLabel, LV_SYMBOL_REFRESH " Scanning...");
    } else {
        lv_label_set_text(ScreenScanner::_scanLabel, LV_SYMBOL_PAUSE " Paused");
    }
}

// ── Whitelist toggle on long-press ─────────────────────────────────
static void node_long_press_cb(lv_event_t* e) {
    size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
    NodeTracker::instance().toggleWhitelist(idx);
    ScreenScanner::refreshList();  // rebuild list to update star icon
}

void ScreenScanner::create() {
    OMS_LOG("UI", "Creating scanner screen");
    _active = true;

    lv_obj_t* old = lv_screen_active();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, OMS_SCREEN_W, OMS_SCREEN_H);
    lv_obj_set_style_bg_color(_screen, theme::BG, 0);

    // ── Top bar (28px) ────────────────────────────────────────────
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

    // Back button
    lv_obj_t* back_btn = lv_button_create(topbar);
    lv_obj_set_size(back_btn, 24, 24);
    lv_obj_set_style_bg_color(back_btn, theme::ACCENT, 0);
    lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme::BG, 0);

    // Title
    lv_obj_t* title = lv_label_create(topbar);
    lv_label_set_text(title, "Node Scanner");
    lv_obj_set_style_text_color(title, theme::TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // Scan toggle button (right side)
    lv_obj_t* scan_btn = lv_button_create(topbar);
    lv_obj_set_size(scan_btn, 24, 24);
    lv_obj_set_style_bg_color(scan_btn, theme::GREEN, 0);
    lv_obj_add_event_cb(scan_btn, scan_toggle_cb, LV_EVENT_CLICKED, nullptr);
    _scanLabel = lv_label_create(scan_btn);
    lv_label_set_text(_scanLabel, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(_scanLabel, theme::BG, 0);

    // ── Column header (20px) ──────────────────────────────────────
    lv_obj_t* header = lv_obj_create(_screen);
    lv_obj_set_size(header, OMS_SCREEN_W, 20);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_style_bg_color(header, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 2, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 4, 0);

    lv_obj_t* h_type = lv_label_create(header);
    lv_label_set_text(h_type, "TY");
    lv_obj_set_style_text_color(h_type, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(h_type, &lv_font_montserrat_10, 0);
    lv_obj_set_width(h_type, 22);

    lv_obj_t* h_name = lv_label_create(header);
    lv_label_set_text(h_name, "Name");
    lv_obj_set_style_text_color(h_name, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(h_name, &lv_font_montserrat_10, 0);
    lv_obj_set_flex_grow(h_name, 1);

    lv_obj_t* h_sig = lv_label_create(header);
    lv_label_set_text(h_sig, "Sig");
    lv_obj_set_style_text_color(h_sig, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(h_sig, &lv_font_montserrat_10, 0);
    lv_obj_set_width(h_sig, 24);

    lv_obj_t* h_rssi = lv_label_create(header);
    lv_label_set_text(h_rssi, "dBm");
    lv_obj_set_style_text_color(h_rssi, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(h_rssi, &lv_font_montserrat_10, 0);
    lv_obj_set_width(h_rssi, 32);

    lv_obj_t* h_dist = lv_label_create(header);
    lv_label_set_text(h_dist, "km");
    lv_obj_set_style_text_color(h_dist, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(h_dist, &lv_font_montserrat_10, 0);
    lv_obj_set_width(h_dist, 28);

    lv_obj_t* h_age = lv_label_create(header);
    lv_label_set_text(h_age, "Age");
    lv_obj_set_style_text_color(h_age, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(h_age, &lv_font_montserrat_10, 0);
    lv_obj_set_width(h_age, 24);

    // ── Node list ────────────────────────────────────────────────
    _list = lv_list_create(_screen);
    lv_obj_set_size(_list, OMS_SCREEN_W, OMS_SCREEN_H - 28 - 20 - 22);
    lv_obj_align(_list, LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_set_style_bg_color(_list, theme::BG, 0);
    lv_obj_set_style_border_width(_list, 0, 0);

    // ── Status bar (bottom) ──────────────────────────────────────
    lv_obj_t* statusbar = lv_obj_create(_screen);
    lv_obj_set_size(statusbar, OMS_SCREEN_W, 22);
    lv_obj_align(statusbar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(statusbar, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(statusbar, 0, 0);
    lv_obj_set_style_radius(statusbar, 0, 0);
    lv_obj_set_style_pad_all(statusbar, 2, 0);

    _countLabel = lv_label_create(statusbar);
    lv_label_set_text(_countLabel, "Nodes: 0");
    lv_obj_set_style_text_color(_countLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(_countLabel, &lv_font_montserrat_10, 0);

    // Populate list from tracker
    refreshList();

    lv_screen_load(_screen);
    if (old) lv_obj_del(old);
}

void ScreenScanner::goBack(lv_event_t* e) {
    (void)e;
    _active = false;
    _scanning = false;
    ScreenHome::create();
}

bool ScreenScanner::isActive() {
    return _active;
}

// ── Estimate distance in km using Haversine ──────────────────────
// Returns -1 if distance cannot be computed (no GPS fix or no node location)
static float estimateDistance(int32_t lat1e6, int32_t lon1e6, int32_t lat2e6, int32_t lon2e6) {
    if ((lat1e6 == 0 && lon1e6 == 0) || (lat2e6 == 0 && lon2e6 == 0)) {
        return -1.0f;  // unknown
    }
    // Haversine formula
    double lat1 = lat1e6 / 1000000.0 * DEG_TO_RAD;
    double lon1 = lon1e6 / 1000000.0 * DEG_TO_RAD;
    double lat2 = lat2e6 / 1000000.0 * DEG_TO_RAD;
    double lon2 = lon2e6 / 1000000.0 * DEG_TO_RAD;
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return (float)(6371.0 * c);  // Earth radius in km
}

// ── RSSI color ────────────────────────────────────────────────────
static lv_color_t rssiColor(int rssi) {
    if (rssi > -50) return theme::GREEN;   // excellent
    if (rssi > -70) return theme::ACCENT;  // good
    if (rssi > -85) return theme::ORANGE;   // fair
    return theme::RED;                       // poor
}

void ScreenScanner::refreshList() {
    if (!_list) return;

    lv_obj_clean(_list);

    NodeTracker& tracker = NodeTracker::instance();
    size_t count = tracker.count();

    if (count == 0) {
        lv_obj_t* empty = lv_list_add_btn(_list, nullptr, "No nodes found");
        lv_obj_set_style_text_color(empty, theme::TEXT_MUTED, 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
    }

    // Get our own GPS position for distance calculations
    float myLat = oms::theBoard()->gpsLat();
    float myLng = oms::theBoard()->gpsLng();
    bool hasGps = oms::theBoard()->hasGPSFix();

    for (size_t i = 0; i < count; i++) {
        const TrackedNode* node = tracker.get(i);
        if (!node) break;

        // Build row text: TYPE  Name         RSSI  km
        char rowText[80];
        const char* prefix = typePrefix(node->type);
        const char* star = node->whitelisted ? LV_SYMBOL_OK " " : "  ";

        // Estimate distance
        float distKm = -1.0f;
        if (hasGps && (node->lat != 0 || node->lon != 0)) {
            distKm = estimateDistance(
                (int32_t)(myLat * 1E6), (int32_t)(myLng * 1E6),
                node->lat, node->lon);
        }

        char distStr[12] = "--";
        if (distKm >= 0) {
            if (distKm < 1.0f) {
                snprintf(distStr, sizeof(distStr), "%dm", (int)(distKm * 1000));
            } else {
                snprintf(distStr, sizeof(distStr), "%.1f", distKm);
            }
        }

        // Age (how long ago seen)
        uint32_t ageSec = (millis() - node->lastSeenMs) / 1000;
        char ageStr[8] = "";
        if (ageSec < 5) {
            snprintf(ageStr, sizeof(ageStr), "now");
        } else if (ageSec < 60) {
            snprintf(ageStr, sizeof(ageStr), "%us", (unsigned)ageSec);
        } else if (ageSec < 3600) {
            snprintf(ageStr, sizeof(ageStr), "%um", (unsigned)(ageSec / 60));
        } else {
            snprintf(ageStr, sizeof(ageStr), "%uh", (unsigned)(ageSec / 3600));
        }

        // Signal quality label
        const char* qualityStr;
        if (node->rssi > -50) qualityStr = "+++";      // excellent
        else if (node->rssi > -70) qualityStr = "++";   // good
        else if (node->rssi > -85) qualityStr = "+";     // fair
        else qualityStr = "-";                           // poor

        snprintf(rowText, sizeof(rowText), "%s %-10s %3s %4d %3s %s",
                 prefix, node->name, qualityStr, node->rssi, distStr, ageStr);

        lv_obj_t* item = lv_list_add_btn(_list, star, rowText);
        lv_obj_set_style_text_color(item, rssiColor(node->rssi), 0);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_10, 0);

        // Long press to toggle whitelist
        lv_obj_add_event_cb(item, node_long_press_cb, LV_EVENT_LONG_PRESSED,
                           (void*)(uintptr_t)i);
    }

    // Update status bar
    if (_countLabel) {
        size_t repeaters = tracker.countByType(NODE_TYPE_REPEATER);
        size_t whitelisted = tracker.countWhitelisted();
        char status[64];
        snprintf(status, sizeof(status), "Nodes: %zu  RP: %zu  Star: %zu",
                 count, repeaters, whitelisted);
        lv_label_set_text(_countLabel, status);
    }
}

void ScreenScanner::tick() {
    if (!_active || !_scanning) return;

    // Refresh the node list every 5 seconds (don't hammer LVGL)
    static uint32_t lastRefresh = 0;
    uint32_t now = millis();
    if (now - lastRefresh > 5000) {
        refreshList();
        lastRefresh = now;
    }
}

}}  // namespace oms::ui