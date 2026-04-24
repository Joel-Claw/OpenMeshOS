// OpenMeshOS — ScreenMap.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Offline map screen. Layout:
//
//   +------------------------------+ 240px
//   | [←]   Zoom:12  49.61/6.13  |  top bar (24px)
//   |------------------------------|
//   |                              |
//   |   Tile map (canvas)          |  map area
//   |   with node markers          |
//   |                              |
//   |------------------------------|
//   | Node: lux-repeater  RSSI:-42 |  info bar (20px)
//   +------------------------------+ 320px wide
//
// Trackball pan: scroll the map.
// Click+scroll: zoom in/out.
// Long press on node: show info.

#include "ScreenMap.h"
#include "ScreenHome.h"
#include "Theme.h"
#include "../map/MapEngine.h"
#include "../hardware/Board.h"
#include "../utils/Log.h"

namespace oms { namespace ui {

lv_obj_t* ScreenMap::_screen    = nullptr;
lv_obj_t* ScreenMap::_mapCanvas = nullptr;
lv_obj_t* ScreenMap::_zoomLabel = nullptr;
lv_obj_t* ScreenMap::_coordLabel = nullptr;
lv_obj_t* ScreenMap::_backBtn   = nullptr;
lv_obj_t* ScreenMap::_nodeInfo  = nullptr;

bool     ScreenMap::_active     = false;
int32_t  ScreenMap::_panAccX    = 0;
int32_t  ScreenMap::_panAccY    = 0;
int8_t   ScreenMap::_zoomHold   = 0;
uint32_t ScreenMap::_pressStart = 0;

static MapEngine sMap;

static void back_cb(lv_event_t* e) {
    ScreenMap::goBack(e);
}

void ScreenMap::create() {
    OMS_LOG("UI", "Creating map screen");

    _active = true;
    _panAccX = 0;
    _panAccY = 0;
    _zoomHold = 0;

    sMap.init();

    // Clean up previous screen
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

    _zoomLabel = lv_label_create(topbar);
    lv_label_set_text(_zoomLabel, "Z:10");
    lv_obj_set_style_text_color(_zoomLabel, theme::TEXT, 0);

    _coordLabel = lv_label_create(topbar);
    lv_label_set_text(_coordLabel, "49.61 6.13");
    lv_obj_set_style_text_color(_coordLabel, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(_coordLabel, &lv_font_montserrat_10, 0);

    // ── Map canvas (center) ───────────────────────────────────────────
    _mapCanvas = lv_canvas_create(_screen);
    lv_obj_set_size(_mapCanvas, OMS_SCREEN_W, OMS_SCREEN_H - 24 - 20);
    lv_obj_align(_mapCanvas, LV_ALIGN_TOP_LEFT, 0, 24);

    // Allocate canvas buffer in PSRAM (320 x 196 x 2 bytes)
    uint32_t canvasW = OMS_SCREEN_W;
    uint32_t canvasH = OMS_SCREEN_H - 24 - 20;
    lv_color_t* cbuf = (lv_color_t*)ps_malloc(canvasW * canvasH * sizeof(lv_color_t));
    if (cbuf) {
        lv_canvas_set_buffer(_mapCanvas, cbuf, canvasW, canvasH, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(_mapCanvas, theme::BG, 0);
    } else {
        OMS_LOG("UI", "WARN: map canvas PSRAM alloc failed");
    }

    // ── Node info bar (bottom 20px) ───────────────────────────────────
    _nodeInfo = lv_obj_create(_screen);
    lv_obj_set_size(_nodeInfo, OMS_SCREEN_W, 20);
    lv_obj_align(_nodeInfo, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_nodeInfo, theme::BG_CARD, 0);
    lv_obj_set_style_border_width(_nodeInfo, 0, 0);
    lv_obj_set_style_radius(_nodeInfo, 0, 0);
    lv_obj_set_style_pad_hor(_nodeInfo, 4, 0);

    lv_obj_t* info_lbl = lv_label_create(_nodeInfo);
    lv_label_set_text(info_lbl, "No nodes nearby");
    lv_obj_set_style_text_color(info_lbl, theme::TEXT_MUTED, 0);
    lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_10, 0);

    // ── Load ──────────────────────────────────────────────────────────
    lv_screen_load(_screen);
    if (old) lv_obj_del(old);

    // Initial render
    refresh();
}

void ScreenMap::goBack(lv_event_t* e) {
    (void)e;
    _active = false;
    ScreenHome::create();
}

void ScreenMap::refresh() {
    if (!_active || !_mapCanvas) return;

    // Update zoom/coord labels
    if (_zoomLabel) {
        char zbuf[8];
        snprintf(zbuf, sizeof(zbuf), "Z:%d", sMap.zoom());
        lv_label_set_text(_zoomLabel, zbuf);
    }
    if (_coordLabel) {
        char cbuf[24];
        snprintf(cbuf, sizeof(cbuf), "%.2f %.2f", sMap.centerLat(), sMap.centerLng());
        lv_label_set_text(_coordLabel, cbuf);
    }

    // Render map tiles to canvas
    sMap.renderFrame();

    // Draw node markers on top of canvas
    if (sMap.nodeCount() > 0 && _nodeInfo) {
        const auto& n = sMap.nodes()[0];
        char ibuf[48];
        snprintf(ibuf, sizeof(ibuf), "%s  RSSI:%d", n.name, n.rssi);
        lv_obj_t* lbl = lv_obj_get_child(_nodeInfo, 0);
        if (lbl) lv_label_set_text(lbl, ibuf);
    }
}

void ScreenMap::feedInput(int16_t dx, int16_t dy, bool pressed) {
    if (!_active) return;

    if (pressed && _zoomHold == 0) {
        _zoomHold = 1;
        _pressStart = millis();
        return;
    }

    if (!pressed && _zoomHold == 1) {
        // Short press = normal click (no zoom)
        _zoomHold = 0;
        // If barely scrolled, treat as tap on map (select node?)
        if (_panAccX == 0 && _panAccY == 0) {
            // Could select nearest node
        }
        _panAccX = 0;
        _panAccY = 0;
        return;
    }

    // While held, scrolling = zoom
    if (_zoomHold == 1 && (dx != 0 || dy != 0)) {
        _zoomHold = 2; // entered zoom mode
    }

    if (_zoomHold == 2) {
        // Vertical scroll while held = zoom
        if (dy > 0) sMap.zoomIn();
        if (dy < 0) sMap.zoomOut();
        // Horizontal scroll while held = nothing extra
    } else {
        // Normal pan
        _panAccX += dx;
        _panAccY += dy;
        sMap.pan(dx * 4, dy * 4); // scale trackball to pixels
    }

    refresh();
}

bool ScreenMap::isActive() {
    return _active;
}

}}  // namespace oms::ui