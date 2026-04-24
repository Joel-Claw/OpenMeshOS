// OpenMeshOS — ScreenHome.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// The main chat screen.  Layout:
//
//   +------------------------------+ 240px
//   | [CH1] [CH2] [DM]  [⚙] [📡] |  status bar (28px)
//   |------------------------------|
//   |                              |
//   |   Message bubbles            |  scrollable area
//   |                              |
//   |                              |
//   |------------------------------|
//   | [input text...        ] [➤] |  input bar (36px)
//   +------------------------------+ 320px wide
//
// On a 320x240 landscape display, every pixel counts.
// We use LVGL's flex layout for automatic positioning.

#include "ScreenHome.h"
#include "ScreenMap.h"
#include "ScreenSettings.h"
#include "ScreenTerminal.h"
#include "Theme.h"
#include "../utils/Log.h"

namespace oms { namespace ui {

lv_obj_t* ScreenHome::_screen    = nullptr;
lv_obj_t* ScreenHome::_channelTab = nullptr;
lv_obj_t* ScreenHome::_msgList   = nullptr;
lv_obj_t* ScreenHome::_inputBar  = nullptr;

void ScreenHome::create() {
    OMS_LOG("UI", "Creating home screen");

    lv_obj_t* old = lv_screen_active();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, OMS_SCREEN_W, OMS_SCREEN_H);
    lv_obj_set_style_bg_color(_screen, theme::BG, 0);

    // ── Status bar (top 28px) ───────────────────────────────────────
    lv_obj_t* statusbar = lv_obj_create(_screen);
    lv_obj_set_size(statusbar, OMS_SCREEN_W, 28);
    lv_obj_align(statusbar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(statusbar, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(statusbar, 0, 0);
    lv_obj_set_style_radius(statusbar, 0, 0);
    lv_obj_set_scrollbar_mode(statusbar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(statusbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(statusbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(statusbar, 4, 0);

    // Channel tabs
    _channelTab = statusbar;  // we add buttons to the flex container

    lv_obj_t* btn_public = lv_button_create(statusbar);
    lv_obj_set_style_bg_color(btn_public, theme::ACCENT, 0);
    lv_obj_set_size(btn_public, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_t* lbl_public = lv_label_create(btn_public);
    lv_label_set_text(lbl_public, "#Public");

    lv_obj_t* btn_dm = lv_button_create(statusbar);
    lv_obj_set_size(btn_dm, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_t* lbl_dm = lv_label_create(btn_dm);
    lv_label_set_text(lbl_dm, "DM");

    // Terminal button
    lv_obj_t* btn_term = lv_button_create(statusbar);
    lv_obj_set_size(btn_term, 24, 24);
    lv_obj_add_event_cb(btn_term, [](lv_event_t*){ ScreenTerminal::create(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_term = lv_label_create(btn_term);
    lv_label_set_text(lbl_term, ">");

    // Map button (next to settings)
    lv_obj_t* btn_map = lv_button_create(statusbar);
    lv_obj_set_size(btn_map, 24, 24);
    lv_obj_add_event_cb(btn_map, [](lv_event_t*){ ScreenMap::create(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_map = lv_label_create(btn_map);
    lv_label_set_text(lbl_map, LV_SYMBOL_GPS);

    // Settings gear (far right)
    lv_obj_t* btn_settings = lv_button_create(statusbar);
    lv_obj_set_size(btn_settings, 24, 24);
    lv_obj_add_event_cb(btn_settings, [](lv_event_t*){ ScreenSettings::create(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, LV_SYMBOL_SETTINGS);

    // ── Message list (middle, scrollable) ────────────────────────────
    _msgList = lv_list_create(_screen);
    lv_obj_set_size(_msgList, OMS_SCREEN_W, OMS_SCREEN_H - 28 - 36);
    lv_obj_align(_msgList, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_style_bg_color(_msgList, theme::BG, 0);
    lv_obj_set_style_border_width(_msgList, 0, 0);

    // Placeholder welcome message
    lv_obj_t* welcome = lv_list_add_btn(_msgList, nullptr,
        "OpenMeshOS v0.1.0\nReady to mesh.");
    lv_obj_set_style_text_color(welcome, theme::TEXT_MUTED, 0);

    // ── Input bar (bottom 36px) ─────────────────────────────────────
    _inputBar = lv_obj_create(_screen);
    lv_obj_set_size(_inputBar, OMS_SCREEN_W, 36);
    lv_obj_align(_inputBar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_inputBar, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(_inputBar, 0, 0);
    lv_obj_set_style_radius(_inputBar, 0, 0);

    lv_obj_t* textarea = lv_textarea_create(_inputBar);
    lv_obj_set_size(textarea, OMS_SCREEN_W - 44, 30);
    lv_obj_align(textarea, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(textarea, theme::BG, 0);
    lv_obj_set_style_text_color(textarea, theme::TEXT, 0);
    lv_textarea_set_placeholder_text(textarea, "Type a message...");

    lv_obj_t* send_btn = lv_button_create(_inputBar);
    lv_obj_set_size(send_btn, 36, 30);
    lv_obj_align(send_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(send_btn, theme::PRIMARY, 0);
    lv_obj_t* send_lbl = lv_label_create(send_btn);
    lv_label_set_text(send_lbl, LV_SYMBOL_RIGHT);

    // ── Load screen ──────────────────────────────────────────────────
    lv_screen_load(_screen);
    if (old) lv_obj_del(old);
}

}}  // namespace oms::ui