// OpenMeshOS — Theme.cpp
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal

#include "Theme.h"

namespace oms { namespace theme {

void apply(lv_display_t* disp) {
    lv_theme_t* th = lv_theme_default_init(
        disp,
        PRIMARY,       // primary
        ACCENT,        // secondary
        true,          // dark mode
        LV_FONT_DEFAULT
    );
    (void)th;
    // Additional style overrides can be added here
    // e.g. lv_style_set_bg_color for specific widgets
}

}}  // namespace oms::theme