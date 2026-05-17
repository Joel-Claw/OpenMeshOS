// OpenMeshOS — ScreenHome.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// The main chat screen.  Layout:
//
//   +------------------------------+ 240px
//   | [CH1] [CH2] [DM]  ⚡3.7V 📶-42dBm  [⚙] [🗺] [>] |
//   |------------------------------|
//   |                              |
//   |   Message bubbles            |  scrollable area
//   |   12:03 <sender> hello       |
//   |                              |
//   |------------------------------|
//   | [input text...        ] [➤] |  input bar (36px)
//   +------------------------------+ 320px wide
//
// Channel tabs switch between #Public, Channel1, and DM views.
// Status bar shows battery voltage and LoRa RSSI.

#include "ScreenHome.h"
#include "ScreenMap.h"
#include "ScreenSettings.h"
#include "ScreenTerminal.h"
#include "Theme.h"
#include "../mesh/MessageBus.h"
#include "../mesh/MeshService.h"
#include "../mesh/TDeckBoard.h"
#include "../hardware/Board.h"
#include "../utils/Log.h"
#include "../version.h"

namespace oms { namespace ui {

// ── Static state ──────────────────────────────────────────────────
lv_obj_t* ScreenHome::_screen    = nullptr;
lv_obj_t* ScreenHome::_statusBar = nullptr;
lv_obj_t* ScreenHome::_msgList   = nullptr;
lv_obj_t* ScreenHome::_inputBar  = nullptr;
lv_obj_t* ScreenHome::_battLabel  = nullptr;
lv_obj_t* ScreenHome::_rssiLabel = nullptr;

ChatTab   ScreenHome::_activeTab       = ChatTab::Public;
uint32_t  ScreenHome::_lastStatusUpdate = 0;

// Active channel tab buttons (for highlighting)
static lv_obj_t* s_tabBtns[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* s_textarea = nullptr;

// ── Per-tab message buffers ────────────────────────────────────────
// Store messages per tab so they survive tab switches
static constexpr size_t TAB_MSG_BUF_SIZE = 64;

struct TabMsg {
    char text[MSG_MAX_LEN + 24];
    bool used;
};

static TabMsg s_publicMsgs[TAB_MSG_BUF_SIZE];
static TabMsg s_ch1Msgs[TAB_MSG_BUF_SIZE];
static TabMsg s_dmMsgs[TAB_MSG_BUF_SIZE];
static size_t s_publicCount = 0;
static size_t s_ch1Count = 0;
static size_t s_dmCount = 0;

static TabMsg* tabBuffer(ChatTab tab) {
    switch (tab) {
        case ChatTab::Public:   return s_publicMsgs;
        case ChatTab::Channel1: return s_ch1Msgs;
        case ChatTab::DM:       return s_dmMsgs;
        default:                return s_publicMsgs;
    }
}

static size_t& tabCount(ChatTab tab) {
    switch (tab) {
        case ChatTab::Public:   return s_publicCount;
        case ChatTab::Channel1: return s_ch1Count;
        case ChatTab::DM:       return s_dmCount;
        default:                return s_publicCount;
    }
}

static constexpr size_t TAB_MSG_MAX = TAB_MSG_BUF_SIZE;
static const char* channelName(ChatTab tab) {
    switch (tab) {
        case ChatTab::Public:    return "#Public";
        case ChatTab::Channel1:  return "CH1";
        case ChatTab::DM:        return "DM";
        default:                 return "???";
    }
}

// ── Message display ────────────────────────────────────────────────
void ScreenHome::updateMessages() {
    if (!_msgList) return;

    bool newMsgs = false;
    InboxMessage msg;
    while (MessageBus::inbox().pop(msg)) {
        // Determine which tab this message belongs to
        ChatTab targetTab;
        if (msg.kind == MsgKind::DirectMessage) {
            targetTab = ChatTab::DM;
        } else if (msg.kind == MsgKind::SystemInfo) {
            targetTab = ChatTab::Public;  // system info goes to public
        } else {
            targetTab = ChatTab::Public;  // group channels go to public for now
        }

        // Store in per-tab buffer
        TabMsg* buf = tabBuffer(targetTab);
        size_t& count = tabCount(targetTab);
        if (count < TAB_MSG_BUF_SIZE) {
            // Format timestamp (HH:MM from millis)
            uint32_t secs = msg.timestamp / 1000;
            uint32_t mins = (secs / 60) % 60;
            uint32_t hrs  = (secs / 3600) % 24;
            char timeBuf[8];
            snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", hrs, mins);

            const char* prefix = (msg.kind == MsgKind::DirectMessage) ? "[DM]" : "[CH]";
            snprintf(buf[count].text, sizeof(TabMsg::text), "%s %s %s: %s",
                     timeBuf, prefix, msg.sender, msg.text);
            buf[count].used = true;
            count++;
        }

        newMsgs = true;
    }

    // If messages arrived for the active tab, refresh the list
    if (newMsgs) {
        refreshMessageList();
    }
}

// ── Refresh message list from tab buffer ────────────────────────────
void ScreenHome::refreshMessageList() {
    if (!_msgList) return;

    lv_obj_clean(_msgList);

    TabMsg* buf = tabBuffer(_activeTab);
    size_t count = tabCount(_activeTab);

    if (count == 0) {
        const char* placeholder = (_activeTab == ChatTab::DM)
            ? "No direct messages yet."
            : "No messages on this channel.";
        lv_obj_t* ph = lv_list_add_btn(_msgList, nullptr, placeholder);
        lv_obj_set_style_text_color(ph, theme::TEXT_MUTED, 0);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (!buf[i].used) continue;
        lv_obj_t* btn = lv_list_add_btn(_msgList, nullptr, buf[i].text);
        lv_obj_set_style_text_color(btn, theme::TEXT, 0);
        lv_obj_set_style_bg_color(btn, theme::BG, 0);
    }

    // Auto-scroll to bottom
    lv_obj_scroll_to_y(_msgList, LV_COORD_MAX, LV_ANIM_OFF);
}

// ── Send input ────────────────────────────────────────────────────
void ScreenHome::sendInput() {
    if (!s_textarea) return;
    const char* text = lv_textarea_get_text(s_textarea);
    if (!text || text[0] == '\0') return;

    switch (_activeTab) {
        case ChatTab::Public:
            MeshService::instance().sendChannel("#Public", text);
            break;
        case ChatTab::Channel1:
            MeshService::instance().sendChannel("CH1", text);
            break;
        case ChatTab::DM:
            // DM tab sends as channel message for now
            // (DM destination selection is a future Phase 2 feature)
            MeshService::instance().sendChannel("#Public", text);
            break;
    }

    // Clear input
    lv_textarea_set_text(s_textarea, "");
}

// ── Switch tab ────────────────────────────────────────────────────
void ScreenHome::switchTab(ChatTab tab) {
    if (tab == _activeTab) return;
    _activeTab = tab;

    // Highlight the active tab button
    for (int i = 0; i < 3; i++) {
        if (s_tabBtns[i]) {
            lv_obj_set_style_bg_color(s_tabBtns[i],
                (i == static_cast<int>(tab)) ? theme::ACCENT : theme::BG_CARD, 0);
        }
    }

    // Refresh message list from the new tab's buffer
    refreshMessageList();

    OMS_LOG("UI", "Switched to tab: %s", channelName(tab));
}

// ── Status bar update ─────────────────────────────────────────────
void ScreenHome::updateStatusBar() {
    if (!_battLabel && !_rssiLabel) return;

    // Update every 5 seconds to avoid flicker
    uint32_t now = millis();
    if (now - _lastStatusUpdate < 5000) return;
    _lastStatusUpdate = now;

    // Battery voltage
    if (_battLabel) {
        uint16_t mv = MeshService::instance().board().getBattMilliVolts();
        float v = mv / 1000.0f;
        // Show battery icon based on voltage level
        const char* icon;
        if (v >= 4.0f)       icon = LV_SYMBOL_BATTERY_FULL;
        else if (v >= 3.7f)  icon = LV_SYMBOL_BATTERY_3;
        else if (v >= 3.5f)  icon = LV_SYMBOL_BATTERY_2;
        else if (v >= 3.3f)  icon = LV_SYMBOL_BATTERY_1;
        else                  icon = LV_SYMBOL_BATTERY_EMPTY;
        char buf[16];
        snprintf(buf, sizeof(buf), "%s%.1fV", icon, v);
        lv_label_set_text(_battLabel, buf);
    }

    // RSSI
    if (_rssiLabel) {
        int rssi = MeshService::instance().rssi();
        char buf[16];
        if (rssi != 0) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "%ddBm", rssi);
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "--");
        }
        lv_label_set_text(_rssiLabel, buf);
    }
}

// ── Create home screen ────────────────────────────────────────────
void ScreenHome::create() {
    OMS_LOG("UI", "Creating home screen");

    lv_obj_t* old = lv_screen_active();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, OMS_SCREEN_W, OMS_SCREEN_H);
    lv_obj_set_style_bg_color(_screen, theme::BG, 0);

    // ── Status bar (top 28px) ────────────────────────────────────
    _statusBar = lv_obj_create(_screen);
    lv_obj_set_size(_statusBar, OMS_SCREEN_W, 28);
    lv_obj_align(_statusBar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_statusBar, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(_statusBar, 0, 0);
    lv_obj_set_style_radius(_statusBar, 0, 0);
    lv_obj_set_scrollbar_mode(_statusBar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(_statusBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_statusBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_statusBar, 4, 0);

    // Channel tabs
    s_tabBtns[0] = lv_button_create(_statusBar);
    lv_obj_set_size(s_tabBtns[0], LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_color(s_tabBtns[0], theme::ACCENT, 0);  // active by default
    lv_obj_add_event_cb(s_tabBtns[0], [](lv_event_t*){ switchTab(ChatTab::Public); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_public = lv_label_create(s_tabBtns[0]);
    lv_label_set_text(lbl_public, "#Public");

    s_tabBtns[1] = lv_button_create(_statusBar);
    lv_obj_set_size(s_tabBtns[1], LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_color(s_tabBtns[1], theme::BG_CARD, 0);
    lv_obj_add_event_cb(s_tabBtns[1], [](lv_event_t*){ switchTab(ChatTab::Channel1); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_ch1 = lv_label_create(s_tabBtns[1]);
    lv_label_set_text(lbl_ch1, "CH1");

    s_tabBtns[2] = lv_button_create(_statusBar);
    lv_obj_set_size(s_tabBtns[2], LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_color(s_tabBtns[2], theme::BG_CARD, 0);
    lv_obj_add_event_cb(s_tabBtns[2], [](lv_event_t*){ switchTab(ChatTab::DM); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_dm = lv_label_create(s_tabBtns[2]);
    lv_label_set_text(lbl_dm, "DM");

    // Spacer (push battery/RSSI to the right)
    lv_obj_t* spacer = lv_obj_create(_statusBar);
    lv_obj_set_size(spacer, 10, 1);  // flexible spacer
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);

    // Battery label
    _battLabel = lv_label_create(_statusBar);
    lv_label_set_text(_battLabel, LV_SYMBOL_BATTERY_FULL "?V");
    lv_obj_set_style_text_color(_battLabel, theme::TEXT_MUTED, 0);

    // RSSI label
    _rssiLabel = lv_label_create(_statusBar);
    lv_label_set_text(_rssiLabel, LV_SYMBOL_WIFI "--");
    lv_obj_set_style_text_color(_rssiLabel, theme::TEXT_MUTED, 0);

    // Terminal button
    lv_obj_t* btn_term = lv_button_create(_statusBar);
    lv_obj_set_size(btn_term, 24, 24);
    lv_obj_add_event_cb(btn_term, [](lv_event_t*){ ScreenTerminal::create(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_term = lv_label_create(btn_term);
    lv_label_set_text(lbl_term, ">");

    // Map button
    lv_obj_t* btn_map = lv_button_create(_statusBar);
    lv_obj_set_size(btn_map, 24, 24);
    lv_obj_add_event_cb(btn_map, [](lv_event_t*){ ScreenMap::create(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_map = lv_label_create(btn_map);
    lv_label_set_text(lbl_map, LV_SYMBOL_GPS);

    // Settings gear
    lv_obj_t* btn_settings = lv_button_create(_statusBar);
    lv_obj_set_size(btn_settings, 24, 24);
    lv_obj_add_event_cb(btn_settings, [](lv_event_t*){ ScreenSettings::create(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, LV_SYMBOL_SETTINGS);

    // ── Message list (middle, scrollable) ───────────────────────
    _msgList = lv_list_create(_screen);
    lv_obj_set_size(_msgList, OMS_SCREEN_W, OMS_SCREEN_H - 28 - 36);
    lv_obj_align(_msgList, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_style_bg_color(_msgList, theme::BG, 0);
    lv_obj_set_style_border_width(_msgList, 0, 0);

    // Welcome message with version
    char welcomeBuf[64];
    snprintf(welcomeBuf, sizeof(welcomeBuf),
             "OpenMeshOS v%s\nReady to mesh.", OMS_VERSION_STRING);
    lv_obj_t* welcome = lv_list_add_btn(_msgList, nullptr, welcomeBuf);
    lv_obj_set_style_text_color(welcome, theme::TEXT_MUTED, 0);

    // ── Input bar (bottom 36px) ──────────────────────────────────
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
    lv_textarea_set_max_length(textarea, 200);
    lv_textarea_set_one_line(textarea, true);
    // Enter key on keyboard sends the message (LV_EVENT_READY fires on Enter)
    lv_obj_add_event_cb(textarea, [](lv_event_t*){ ScreenHome::sendInput(); }, LV_EVENT_READY, nullptr);
    s_textarea = textarea;

    lv_obj_t* send_btn = lv_button_create(_inputBar);
    lv_obj_set_size(send_btn, 36, 30);
    lv_obj_align(send_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(send_btn, theme::PRIMARY, 0);
    lv_obj_add_event_cb(send_btn, [](lv_event_t*){ ScreenHome::sendInput(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* send_lbl = lv_label_create(send_btn);
    lv_label_set_text(send_lbl, LV_SYMBOL_RIGHT);

    // ── Load screen ──────────────────────────────────────────────
    lv_screen_load(_screen);
    if (old) lv_obj_del(old);

    // Initial status bar update
    updateStatusBar();
}

}}  // namespace oms::ui