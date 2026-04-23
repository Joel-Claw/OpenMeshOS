// OpenMeshOS — UIScreen.cpp
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Initialises LVGL, the TFT_eSPI display driver, and loads the
// home screen.  The tick function drives LVGL's task handler.

#include "UIScreen.h"
#include "ScreenHome.h"
#include "Theme.h"
#include "../hardware/Board.h"
#include "../utils/Log.h"

#include <lvgl.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();

// LVGL display buffer — allocated in PSRAM at runtime to save DRAM
static lv_display_t* disp = nullptr;
static lv_color_t* buf1 = nullptr;

// LVGL input device (trackball as encoder)
static lv_indev_t* enc_indev = nullptr;

// Encoder state for LVGL
static int16_t enc_diff = 0;
static bool enc_pressed = false;

// LVGL encoder read callback
static void encoder_read(lv_indev_t* indev, lv_indev_data_t* data) {
    data->enc_diff = enc_diff;
    data->state = enc_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    enc_diff = 0;
    enc_pressed = false;
}

namespace oms { namespace ui {

void init() {
    OMS_LOG("UI", "Initialising display");

    // TFT init (TFT_eSPI uses build flags from platformio.ini)
    tft.begin();
    tft.setRotation(1);  // landscape
    tft.fillScreen(TFT_BLACK);

    // LVGL init
    lv_init();

    // Allocate display buffer in PSRAM
    buf1 = (lv_color_t*)ps_malloc(OMS_SCREEN_W * 40 * sizeof(lv_color_t));
    if (!buf1) {
        OMS_LOG("UI", "FATAL: cannot allocate display buffer in PSRAM");
        return;
    }

    // Display driver
    disp = lv_display_create(OMS_SCREEN_W, OMS_SCREEN_H);
    lv_display_set_flush_cb(disp, [](lv_display_t*, const lv_area_t* area, uint8_t* px) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        tft.pushImage(area->x1, area->y1, w, h, (uint16_t*)px);
        lv_display_flush_ready(disp);
    });
    lv_display_set_buffers(disp, buf1, nullptr, OMS_SCREEN_W * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Input: trackball → LVGL encoder
    enc_indev = lv_indev_create();
    lv_indev_set_type(enc_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(enc_indev, encoder_read);
    lv_indev_set_group(enc_indev, lv_group_get_default());

    // Create default group for encoder navigation
    lv_group_t* g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(enc_indev, g);

    // Apply theme
    theme::apply(disp);

    // Load home screen
    ScreenHome::create();

    OMS_LOG("UI", "Display ready (%dx%d)", OMS_SCREEN_W, OMS_SCREEN_H);
}

void tick() {
    // Feed trackball input into encoder state
    auto& board = Board::instance();
    int16_t dx, dy;
    board.consumeTrackballDelta(dx, dy);

    // Trackball vertical = encoder rotation
    if (dy > 0) enc_diff += dy;
    if (dy < 0) enc_diff += dy;  // negative = scroll up
    if (dx != 0) enc_diff += dx; // horizontal also useful

    // Trackball press = encoder click
    if (board.consumeTrackballPress()) {
        enc_pressed = true;
    }

    // Drive LVGL
    lv_timer_handler();
}

}}  // namespace oms::ui