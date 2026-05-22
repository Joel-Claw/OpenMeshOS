// OpenMeshOS — main entry point
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Initialises hardware, MeshCore, and the UI task loop.

#include <Arduino.h>
#include <SPIFFS.h>
#include "version.h"
#include "hardware/Board.h"
#include "hardware/KeyboardInput.h"
#include "hardware/Notification.h"
#include "mesh/MeshService.h"
#include "mesh/BLECompanion.h"
#include "ui/UIScreen.h"
#include "ui/ScreenHome.h"
#include "ui/ScreenMap.h"
#include "ui/ScreenLock.h"
#include "utils/Log.h"
#include "utils/Config.h"
#include "hardware/Watchdog.h"
#include "hardware/CrashLog.h"

static oms::KeyboardInput s_kbInput;

// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* wait up to 3s for serial */ }

    OMS_LOG("main", "OpenMeshOS v" OMS_VERSION_STRING " starting");

    // 0) Check for previous crash
    if (oms::CrashLog::hasCrash()) {
        oms::CrashLog::showCrashReport();
    }
    oms::CrashLog::installHandler();

    // 1) Initialise SPIFFS (must come before config and mesh)
    if (!SPIFFS.begin(true)) {
        OMS_LOG("main", "WARNING: SPIFFS mount failed, formatting");
        SPIFFS.format();
        if (!SPIFFS.begin(true)) {
            OMS_LOG("main", "ERROR: SPIFFS unavailable, some features will fail");
        }
    }

    // 2) Load persistent config from SPIFFS / SD
    oms::config::init();

    // 2) Initialise board-level hardware (display, keyboard, trackball, LoRa, GPS)
    oms::Board::instance().init();

    // 2b) Initialise notification system (audio + screen wake)
    oms::Notification::instance().init();

    // 3) Initialise MeshCore radio + protocol stack
    oms::MeshService::instance().init();

    // 3b) Initialise BLE companion service (after mesh is ready)
    oms::BLECompanion::instance().init();

    // 4) Initialise UI (LVGL + screen driver)
    oms::ui::init();

    // 5) Initialise keyboard LVGL indev
    s_kbInput.initIndev();

    // 6) Start watchdog (30s timeout, auto-reboot on hang)
    oms::Watchdog::init(30);

    OMS_LOG("main", "Ready");
}

// ── Loop ────────────────────────────────────────────────────────────
void loop() {
    oms::Watchdog::feed();  // feed watchdog first thing
    oms::Board::instance().tick();
    s_kbInput.update(oms::Board::instance().keyboard());

    // Feed trackball input to active screen
    {
        int16_t tbDx = 0, tbDy = 0;
        oms::Board::instance().trackball().consumeDelta(tbDx, tbDy);
        bool tbPress = oms::Board::instance().trackball().consumePress();
        if (oms::ui::ScreenMap::isActive()) {
            oms::ui::ScreenMap::feedInput(tbDx, tbDy, tbPress);
        }
        if (tbDx != 0 || tbDy != 0 || tbPress) {
            oms::ui::ScreenLock::resetIdleTimer();
        }
    }

    oms::MeshService::instance().tick();
    oms::BLECompanion::instance().tick();
    oms::Notification::instance().tick();
    oms::ui::tick();

    // Reset idle timer on any keyboard activity
    if (s_kbInput.hasEvents()) {
        oms::ui::ScreenLock::resetIdleTimer();
    }

    // Drain mesh message inbox into UI
    oms::ui::ScreenHome::updateMessages();
    // Update status bar (battery, RSSI) every ~5 seconds
    oms::ui::ScreenHome::updateStatusBar();

    // Check idle timeout and lock screen if needed
    oms::ui::ScreenLock::checkIdle();
    // Update lock screen clock display
    if (oms::ui::ScreenLock::isActive()) {
        oms::ui::ScreenLock::update();
    }

    // Deferred config save (SPIFFS wear minimization)
    oms::config::tick();
}