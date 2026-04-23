// OpenMeshOS — main entry point
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Initialises hardware, MeshCore, and the UI task loop.

#include <Arduino.h>
#include "version.h"
#include "hardware/Board.h"
#include "mesh/MeshService.h"
#include "ui/UIScreen.h"
#include "utils/Log.h"
#include "utils/Config.h"

// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* wait up to 3s for serial */ }

    OMS_LOG("main", "OpenMeshOS v" OMS_VERSION_STRING " starting");

    // 1) Load persistent config from SPIFFS / SD
    oms::config::init();

    // 2) Initialise board-level hardware (display, keyboard, trackball, LoRa, GPS)
    oms::Board::instance().init();

    // 3) Initialise MeshCore radio + protocol stack
    oms::MeshService::instance().init();

    // 4) Initialise UI (LVGL + screen driver)
    oms::ui::init();

    OMS_LOG("main", "Ready");
}

// ── Loop ────────────────────────────────────────────────────────────
void loop() {
    oms::Board::instance().tick();
    oms::MeshService::instance().tick();
    oms::ui::tick();
}