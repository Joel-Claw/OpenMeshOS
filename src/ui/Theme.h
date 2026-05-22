// OpenMeshOS — Theme.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Dark theme inspired by MeshOS aesthetics but cleaner.
// All colour constants live here so the UI stays consistent.

#pragma once

#include <lvgl.h>

namespace oms { namespace theme {

// ── Colour palette ──────────────────────────────────────────────────
static constexpr lv_color_t BG          = LV_COLOR_MAKE(13, 17, 23);    // #0d1117
static constexpr lv_color_t BG_CARD     = LV_COLOR_MAKE(22, 27, 34);   // #161b22
static constexpr lv_color_t TEXT        = LV_COLOR_MAKE(230, 237, 243); // #e6edf3
static constexpr lv_color_t TEXT_MUTED  = LV_COLOR_MAKE(139, 148, 158); // #8b949e
static constexpr lv_color_t ACCENT      = LV_COLOR_MAKE(88, 166, 255);  // #58a6ff
static constexpr lv_color_t PRIMARY     = LV_COLOR_MAKE(0, 51, 170);    // #0033AA
static constexpr lv_color_t GREEN       = LV_COLOR_MAKE(63, 185, 80);   // #3fb950
static constexpr lv_color_t RED         = LV_COLOR_MAKE(248, 81, 73);   // #f85149
static constexpr lv_color_t ORANGE      = LV_COLOR_MAKE(210, 153, 34);  // #d29922
static constexpr lv_color_t BORDER     = LV_COLOR_MAKE(48, 54, 61);    // #30363d

// ── Light theme colours ──────────────────────────────────────────────
namespace light {
static constexpr lv_color_t BG          = LV_COLOR_MAKE(255, 255, 255);    // #ffffff
static constexpr lv_color_t BG_CARD     = LV_COLOR_MAKE(246, 248, 250);   // #f6f8fa
static constexpr lv_color_t TEXT        = LV_COLOR_MAKE(31, 35, 40);      // #1f2328
static constexpr lv_color_t TEXT_MUTED  = LV_COLOR_MAKE(110, 119, 129);  // #6e7781
static constexpr lv_color_t ACCENT      = LV_COLOR_MAKE(9, 105, 218);    // #0969da
static constexpr lv_color_t PRIMARY     = LV_COLOR_MAKE(0, 51, 170);    // #0033AA
static constexpr lv_color_t GREEN       = LV_COLOR_MAKE(26, 127, 55);   // #1a7f37
static constexpr lv_color_t RED         = LV_COLOR_MAKE(207, 34, 46);    // #cf222e
static constexpr lv_color_t ORANGE      = LV_COLOR_MAKE(159, 95, 9);    // #9f5f09
static constexpr lv_color_t BORDER     = LV_COLOR_MAKE(208, 215, 222);  // #d0d7de
}  // namespace light

// ── Theme switching ──────────────────────────────────────────────────
void apply(lv_display_t* disp);
void setLightMode(bool light);
bool isLightMode();

// ── Convenience: returns current active palette based on mode ──────
// These delegate to dark:: or light:: at runtime
namespace active {
lv_color_t bg();
lv_color_t bgCard();
lv_color_t text();
lv_color_t textMuted();
lv_color_t accent();
lv_color_t primary();
lv_color_t green();
lv_color_t red();
lv_color_t orange();
lv_color_t border();
}  // namespace active

}}  // namespace oms::theme