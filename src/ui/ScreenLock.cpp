// OpenMeshOS — ScreenLock.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Lock screen. Layout:
//
//   +------------------------------+ 240px
//   |                              |
//   |        12:34                 |  large time
//   |        May 17, 2026          |  date
//   |                              |
//   |   ⚡ 3.7V   📶 -42dBm       |  battery & signal
//   |   3 nodes nearby            |  mesh status
//   |                              |
//   |   Press any key to unlock   |  hint
//   +------------------------------+ 320px wide
//
// The lock screen activates after the configured screen timeout.
// Any key press or trackball press unlocks.

#include "ScreenLock.h"
#include "ScreenHome.h"
#include "Theme.h"
#include "../mesh/MeshService.h"
#include "../mesh/TDeckBoard.h"
#include "../hardware/Board.h"
#include "../utils/Config.h"
#include "../utils/Log.h"

namespace oms { namespace ui {

lv_obj_t* ScreenLock::_screen     = nullptr;
lv_obj_t* ScreenLock::_timeLabel  = nullptr;
lv_obj_t* ScreenLock::_dateLabel  = nullptr;
lv_obj_t* ScreenLock::_battLabel  = nullptr;
lv_obj_t* ScreenLock::_nodeLabel  = nullptr;
lv_obj_t* ScreenLock::_hintLabel  = nullptr;

bool ScreenLock::_active          = false;
bool ScreenLock::_dimmed           = false;
uint32_t ScreenLock::_lastActivity = 0;
uint32_t ScreenLock::_lockTimeoutMs = 30000;  // default 30s

static void unlock_cb(lv_event_t* e) {
    ScreenLock::unlock();
}

void ScreenLock::create() {
    OMS_LOG("UI", "Creating lock screen");

    _active = true;
    _lastActivity = millis();

    lv_obj_t* old = lv_screen_active();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, OMS_SCREEN_W, OMS_SCREEN_H);
    lv_obj_set_style_bg_color(_screen, theme::BG, 0);

    // ── Time (large, centered) ───────────────────────────────────────
    _timeLabel = lv_label_create(_screen);
    lv_label_set_text(_timeLabel, "--:--");
    lv_obj_set_style_text_color(_timeLabel, theme::TEXT, 0);
    lv_obj_set_style_text_font(_timeLabel, &lv_font_montserrat_28, 0);
    lv_obj_align(_timeLabel, LV_ALIGN_CENTER, 0, -50);

    // ── Date ─────────────────────────────────────────────────────────
    _dateLabel = lv_label_create(_screen);
    lv_label_set_text(_dateLabel, "Loading...");
    lv_obj_set_style_text_color(_dateLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(_dateLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(_dateLabel, LV_ALIGN_CENTER, 0, -20);

    // ── Battery ──────────────────────────────────────────────────────
    _battLabel = lv_label_create(_screen);
    lv_label_set_text(_battLabel, LV_SYMBOL_BATTERY_FULL " ?V  " LV_SYMBOL_WIFI " --");
    lv_obj_set_style_text_color(_battLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(_battLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(_battLabel, LV_ALIGN_CENTER, 0, 20);

    // ── Node count ───────────────────────────────────────────────────
    _nodeLabel = lv_label_create(_screen);
    lv_label_set_text(_nodeLabel, "Mesh: standby");
    lv_obj_set_style_text_color(_nodeLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(_nodeLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(_nodeLabel, LV_ALIGN_CENTER, 0, 42);

    // ── Unlock hint (blinking) ───────────────────────────────────────
    _hintLabel = lv_label_create(_screen);
    lv_label_set_text(_hintLabel, "Press any key to unlock");
    lv_obj_set_style_text_color(_hintLabel, theme::ACCENT, 0);
    lv_obj_set_style_text_font(_hintLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(_hintLabel, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Any click on the screen unlocks
    lv_obj_add_event_cb(_screen, unlock_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(_screen, unlock_cb, LV_EVENT_KEY, nullptr);

    // ── Load screen ──────────────────────────────────────────────────
    lv_screen_load(_screen);
    if (old) lv_obj_del(old);

    // Initial update
    update();
}

void ScreenLock::unlock() {
    if (!_active) return;
    _active = false;
    _dimmed = false;
    _lastActivity = millis();

    // Restore backlight
    Board::instance().setBacklight(true);

    OMS_LOG("UI", "Screen unlocked");
    ScreenHome::create();
}

void ScreenLock::update() {
    if (!_active) return;

    // Update time display (from millis, crude until RTC/GPS provides real time)
    uint32_t secs = millis() / 1000;
    uint32_t mins = (secs / 60) % 60;
    uint32_t hrs  = (secs / 3600) % 24;

    if (_timeLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", hrs, mins);
        lv_label_set_text(_timeLabel, buf);
    }

    if (_dateLabel) {
        // Placeholder until we have RTC
        lv_label_set_text(_dateLabel, "OpenMeshOS");
    }

    // Battery and RSSI
    if (_battLabel && MeshService::instance().initialized()) {
        uint16_t mv = MeshService::instance().board().getBattMilliVolts();
        int rssi = MeshService::instance().rssi();
        char buf[32];
        snprintf(buf, sizeof(buf), LV_SYMBOL_BATTERY_FULL " %.1fV  " LV_SYMBOL_WIFI " %ddBm",
                 mv / 1000.0f, rssi);
        lv_label_set_text(_battLabel, buf);
    }
}

void ScreenLock::resetIdleTimer() {
    _lastActivity = millis();
    if (_dimmed && !_active) {
        // Wake from dim
        Board::instance().setBacklight(true);
        _dimmed = false;
    }
}

void ScreenLock::checkIdle() {
    const auto& cfg = config::get();

    // 0 = never timeout
    if (cfg.screenTimeoutSec == 0) return;

    uint32_t timeoutMs = (uint32_t)cfg.screenTimeoutSec * 1000;
    uint32_t elapsed = millis() - _lastActivity;

    if (!_dimmed && !_active && elapsed > (timeoutMs * 3 / 4)) {
        // Dim to 20% brightness before full lock
        _dimmed = true;
        OMS_LOG("UI", "Screen dimming (idle %lus)", elapsed / 1000);
    }

    if (!_active && elapsed > timeoutMs) {
        OMS_LOG("UI", "Screen lock (idle %lus, timeout %ds)",
                elapsed / 1000, cfg.screenTimeoutSec);
        create();
    }
}

}}  // namespace oms::ui