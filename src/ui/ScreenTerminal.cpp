// OpenMeshOS — ScreenTerminal.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Terminal screen. Layout:
//
//   +------------------------------+ 240px
//   | [←]  Terminal                |  top bar (24px)
//   |------------------------------|
//   | > help                       |
//   | Commands: info, ping,        |  output (scrollable)
//   |   reboot, version, mesh...   |
//   |                              |
//   |------------------------------|
//   | $ ______                     |  input line (28px)
//   +------------------------------+ 320px wide
//
// Implements a simple command interpreter for device management.
// Keyboard input from BBQ10KB (I2C keyboard on T-Deck).

#include "ScreenTerminal.h"
#include "ScreenHome.h"
#include "Theme.h"
#include "../mesh/MeshService.h"
#include "../utils/Config.h"
#include "../utils/Log.h"
#include "../version.h"

namespace oms { namespace ui {

lv_obj_t* ScreenTerminal::_screen      = nullptr;
lv_obj_t* ScreenTerminal::_outputArea  = nullptr;
lv_obj_t* ScreenTerminal::_inputField  = nullptr;
lv_obj_t* ScreenTerminal::_backBtn     = nullptr;
bool      ScreenTerminal::_active      = false;

// Simple command buffer (no dynamic allocation)
static char sCmdBuf[128];
static uint8_t sCmdLen = 0;

// Output text buffer (ring of lines, no dynamic allocation)
static constexpr int MAX_LINES = 20;
static constexpr int LINE_LEN = 64;
static char sLines[MAX_LINES][LINE_LEN];
static int sLineHead = 0;
static int sLineCount = 0;

static void addLine(const char* text) {
    int idx = (sLineHead + sLineCount) % MAX_LINES;
    snprintf(sLines[idx], LINE_LEN, "%s", text);
    if (sLineCount < MAX_LINES) {
        sLineCount++;
    } else {
        sLineHead = (sLineHead + 1) % MAX_LINES;
    }
}

static void refreshOutput() {
    if (!ScreenTerminal::_outputArea) return;
    // Build a single text string from the ring buffer
    char buf[MAX_LINES * (LINE_LEN + 1)];
    int pos = 0;
    for (int i = 0; i < sLineCount; i++) {
        int idx = (sLineHead + i) % MAX_LINES;
        int len = snprintf(buf + pos, sizeof(buf) - pos, "%s\n", sLines[idx]);
        if (len > 0) pos += len;
        if (pos >= (int)sizeof(buf) - 2) break;
    }
    buf[pos] = '\0';
    lv_label_set_text(ScreenTerminal::_outputArea, buf);
}

// Built-in command interpreter
static void execCommand(const char* cmd) {
    // Echo the command
    char echo[72];
    snprintf(echo, sizeof(echo), "> %s", cmd);
    addLine(echo);

    // Simple command parsing (no strcmp for small commands)
    if (strcmp(cmd, "help") == 0) {
        addLine("Commands:");
        addLine("  help     - this text");
        addLine("  version  - firmware version");
        addLine("  info     - device info");
        addLine("  reboot   - restart device");
        addLine("  mesh     - mesh status");
        addLine("  config   - show config");
        addLine("  clear    - clear terminal");
        addLine("  free     - memory info");
    } else if (strcmp(cmd, "version") == 0) {
        addLine(OMS_VERSION_STRING);
    } else if (strcmp(cmd, "info") == 0) {
        addLine("OpenMeshOS / T-Deck");
        char buf[LINE_LEN];
        snprintf(buf, LINE_LEN, "ESP32-S3 @ 240MHz");
        addLine(buf);
        snprintf(buf, LINE_LEN, "Heap: %u KB free", (unsigned)(ESP.getFreeHeap() / 1024));
        addLine(buf);
    } else if (strcmp(cmd, "reboot") == 0) {
        addLine("Rebooting...");
        refreshOutput();
        delay(500);
        ESP.restart();
    } else if (strcmp(cmd, "mesh") == 0) {
        addLine("Mesh: not connected");
        // When MeshService is fully wired, query real status
    } else if (strcmp(cmd, "config") == 0) {
        const auto& cfg = oms::config::get();
        char buf[LINE_LEN];
        snprintf(buf, LINE_LEN, "Region: %s", cfg.radioRegion);
        addLine(buf);
        snprintf(buf, LINE_LEN, "Callsign: %s", cfg.callsign);
        addLine(buf);
        snprintf(buf, LINE_LEN, "Channel: %d", cfg.channel);
        addLine(buf);
    } else if (strcmp(cmd, "clear") == 0) {
        sLineHead = 0;
        sLineCount = 0;
        addLine("Terminal cleared.");
    } else if (strcmp(cmd, "free") == 0) {
        char buf[LINE_LEN];
        snprintf(buf, LINE_LEN, "Heap: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
        addLine(buf);
        snprintf(buf, LINE_LEN, "Min free: %u KB", (unsigned)(ESP.getMinFreeHeap() / 1024));
        addLine(buf);
        snprintf(buf, LINE_LEN, "PSRAM: %u KB", (unsigned)(ESP.getFreePsram() / 1024));
        addLine(buf);
    } else if (strlen(cmd) == 0) {
        // empty line, no output
    } else {
        char buf[LINE_LEN];
        snprintf(buf, LINE_LEN, "Unknown: %s", cmd);
        addLine(buf);
    }
}

static void back_cb(lv_event_t* e) {
    ScreenTerminal::goBack(e);
}

void ScreenTerminal::create() {
    OMS_LOG("UI", "Creating terminal screen");

    _active = true;

    lv_obj_t* old = lv_screen_active();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_size(_screen, OMS_SCREEN_W, OMS_SCREEN_H);
    lv_obj_set_style_bg_color(_screen, theme::BG, 0);

    // ── Top bar (24px) ────────────────────────────────────────────────
    lv_obj_t* topbar = lv_obj_create(_screen);
    lv_obj_set_size(topbar, OMS_SCREEN_W, 24);
    lv_obj_align(topbar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(topbar, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(topbar, 0, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 2, 0);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(topbar, 6, 0);

    _backBtn = lv_button_create(topbar);
    lv_obj_set_size(_backBtn, 20, 20);
    lv_obj_set_style_bg_color(_backBtn, theme::ACCENT, 0);
    lv_obj_add_event_cb(_backBtn, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(_backBtn);
    lv_label_set_text(back_lbl, "<");
    lv_obj_set_style_text_color(back_lbl, theme::BG, 0);

    lv_obj_t* title = lv_label_create(topbar);
    lv_label_set_text(title, "Terminal");
    lv_obj_set_style_text_color(title, theme::TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // ── Output area (scrollable text) ────────────────────────────────
    lv_obj_t* output_cont = lv_obj_create(_screen);
    lv_obj_set_size(output_cont, OMS_SCREEN_W, OMS_SCREEN_H - 24 - 28);
    lv_obj_align(output_cont, LV_ALIGN_TOP_LEFT, 0, 24);
    lv_obj_set_style_bg_color(output_cont, theme::BG, 0);
    lv_obj_set_style_border_width(output_cont, 0, 0);
    lv_obj_set_style_pad_all(output_cont, 2, 0);

    _outputArea = lv_label_create(output_cont);
    lv_obj_set_size(_outputArea, OMS_SCREEN_W - 4, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(_outputArea, theme::GREEN, 0);
    lv_obj_set_style_text_font(_outputArea, &lv_font_montserrat_10, 0);
    lv_label_set_text(_outputArea, "OpenMeshOS Terminal\nType 'help' for commands.\n");

    // ── Input line (bottom 28px) ─────────────────────────────────────
    lv_obj_t* input_cont = lv_obj_create(_screen);
    lv_obj_set_size(input_cont, OMS_SCREEN_W, 28);
    lv_obj_align(input_cont, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(input_cont, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(input_cont, 0, 0);
    lv_obj_set_style_radius(input_cont, 0, 0);
    lv_obj_set_style_pad_all(input_cont, 2, 0);

    lv_obj_t* prompt = lv_label_create(input_cont);
    lv_label_set_text(prompt, "$ ");
    lv_obj_set_style_text_color(prompt, theme::ACCENT, 0);
    lv_obj_set_style_text_font(prompt, &lv_font_montserrat_12, 0);
    lv_obj_align(prompt, LV_ALIGN_LEFT_MID, 2, 0);

    _inputField = lv_textarea_create(input_cont);
    lv_obj_set_size(_inputField, OMS_SCREEN_W - 30, 22);
    lv_obj_align(_inputField, LV_ALIGN_LEFT_MID, 18, 0);
    lv_obj_set_style_bg_color(_inputField, theme::BG, 0);
    lv_obj_set_style_text_color(_inputField, theme::TEXT, 0);
    lv_obj_set_style_text_font(_inputField, &lv_font_montserrat_12, 0);
    lv_textarea_set_placeholder_text(_inputField, "cmd...");
    lv_textarea_set_one_line(_inputField, true);
    // Handle Enter key
    lv_obj_add_event_cb(_inputField, [](lv_event_t* e) {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
        const char* text = lv_textarea_get_text(ta);
        ScreenTerminal::submitInput(text);
    }, LV_EVENT_READY, nullptr);

    // ── Load ──────────────────────────────────────────────────────────
    lv_screen_load(_screen);
    if (old) lv_obj_del(old);

    // Welcome message
    addLine("OpenMeshOS Terminal v" OMS_VERSION_STRING);
    addLine("Type 'help' for commands.");
    refreshOutput();
}

void ScreenTerminal::goBack(lv_event_t* e) {
    (void)e;
    _active = false;
    ScreenHome::create();
}

bool ScreenTerminal::isActive() {
    return _active;
}

void ScreenTerminal::print(const char* text) {
    addLine(text);
    refreshOutput();
}

void ScreenTerminal::submitInput(const char* line) {
    if (!_active) return;

    execCommand(line);
    refreshOutput();

    // Clear input field
    if (_inputField) {
        lv_textarea_set_text(_inputField, "");
    }
}

}}  // namespace oms::ui