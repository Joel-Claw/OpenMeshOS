// OpenMeshOS — Theme.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Theme management with dark/light mode switching.

#include "Theme.h"

namespace oms { namespace theme {

static bool sLightMode = false;

void apply(lv_display_t* disp) {
    (void)disp;
    // LVGL 9 theme is applied at init time.
    // Colour switching is done by each screen reading from active:: namespace.
}

void setLightMode(bool light) {
    sLightMode = light;
}

bool isLightMode() {
    return sLightMode;
}

// ── Active palette delegates ────────────────────────────────────────
namespace active {

lv_color_t bg() {
    return sLightMode ? light::BG : BG;
}
lv_color_t bgCard() {
    return sLightMode ? light::BG_CARD : BG_CARD;
}
lv_color_t text() {
    return sLightMode ? light::TEXT : TEXT;
}
lv_color_t textMuted() {
    return sLightMode ? light::TEXT_MUTED : TEXT_MUTED;
}
lv_color_t accent() {
    return sLightMode ? light::ACCENT : ACCENT;
}
lv_color_t primary() {
    return sLightMode ? light::PRIMARY : PRIMARY;
}
lv_color_t green() {
    return sLightMode ? light::GREEN : GREEN;
}
lv_color_t red() {
    return sLightMode ? light::RED : RED;
}
lv_color_t orange() {
    return sLightMode ? light::ORANGE : ORANGE;
}
lv_color_t border() {
    return sLightMode ? light::BORDER : BORDER;
}

}  // namespace active
}}  // namespace oms::theme