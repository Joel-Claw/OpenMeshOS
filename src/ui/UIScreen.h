// OpenMeshOS — UIScreen.h
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Top-level UI controller. Owns the LVGL display driver and
// manages which screen is active.  All screens are defined in
// the ui/ directory; this header provides the init/tick entry
// points called from main.cpp.

#pragma once

namespace oms { namespace ui {

void init();
void tick();

}}  // namespace oms::ui