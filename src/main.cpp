// OpenMeshOS — main entry point
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Initialises hardware, MeshCore, and the UI task loop.
//
// Three build paths:
//   T-Deck builds    — Full UI (LVGL, keyboard, trackball, screens)
//   Heltec V3 builds — Headless (mesh radio + BLE companion only, no UI)
//   RAK4631 builds    — Headless (mesh radio + BLE, nRF52 low-power platform)

#include <Arduino.h>
#include <SPIFFS.h>
#include "version.h"
#include "hardware/IBoard.h"
#include "mesh/MeshService.h"
#include "mesh/NodeTracker.h"
#include "mesh/BLECompanion.h"
#include "utils/Log.h"
#include "utils/Config.h"
#include "hardware/Watchdog.h"
#include "hardware/CrashLog.h"
#include "hardware/PowerManager.h"
#include "hardware/HeapMonitor.h"

// ── T-Deck-only includes (not available on Heltec V3 build) ─────────
#ifndef OMS_PLATFORM_HELTEC_V3
  #include "hardware/KeyboardInput.h"
  #include "hardware/Notification.h"
  #include "ui/UIScreen.h"
  #include "ui/ScreenHome.h"
  #include "ui/ScreenMap.h"
  #include "ui/ScreenLock.h"
  #include "ui/ScreenScanner.h"

static oms::KeyboardInput s_kbInput;
#endif

// =============================================================================
//  T-Deck / T-Deck Plus — Full UI firmware
// =============================================================================
#ifndef OMS_PLATFORM_HELTEC_V3

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

    // 2a) Load whitelist from SPIFFS (before mesh starts receiving adverts)
    oms::NodeTracker::instance().loadWhitelist();

    // 2) Initialise board-level hardware (display, keyboard, trackball, LoRa, GPS)
    oms::theBoard()->init();

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

    // 7) Initialize power management (light sleep, dynamic frequency scaling)
    oms::PowerManager::instance().init();

    // 8) Initialize heap monitoring (logs diagnostics every 60s)
    oms::HeapMonitor::instance().init(60, 30000, 15000);

    OMS_LOG("main", "Ready");
}

void loop() {
    oms::Watchdog::feed();  // feed watchdog first thing
    oms::theBoard()->tick();
    s_kbInput.update(oms::theBoard()->keyboard());

    // Feed trackball input to active screen
    {
        int16_t tbDx = 0, tbDy = 0;
        oms::theBoard()->consumeTrackballDelta(tbDx, tbDy);
        bool tbPress = oms::theBoard()->consumeTrackballPress();
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

    // Update node scanner screen if active
    oms::ui::ScreenScanner::tick();

    // Deferred config save (SPIFFS wear minimization)
    oms::config::tick();

    // Heap monitoring: periodic diagnostics + immediate alerts
    oms::HeapMonitor::instance().tick();

    // Power: yield to idle task (may enter light sleep)
    oms::PowerManager::instance().idle();
}

// =============================================================================
//  RAK WisBlock RAK4631 — Headless mesh node / BLE companion (nRF52840)
// =============================================================================
#elif defined(OMS_PLATFORM_RAK4631)

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* wait up to 3s for serial */ }

    OMS_LOG("main", "OpenMeshOS v" OMS_VERSION_STRING " starting (RAK4631, headless nRF52)");

    // 1) Initialise LittleFS (nRF52 doesn't have SPIFFS, uses LittleFS)
    //    On nRF52 Arduino core, LittleFS is available via the built-in library.
    //    We use the same SPIFFS API for compatibility — the nRF52 Arduino core
    //    maps SPIFFS to LittleFS internally when on nRF52.
    if (!SPIFFS.begin(true)) {
        OMS_LOG("main", "WARNING: LittleFS mount failed, formatting");
        SPIFFS.format();
        if (!SPIFFS.begin(true)) {
            OMS_LOG("main", "ERROR: LittleFS unavailable, some features will fail");
        }
    }

    // 2) Load persistent config
    oms::config::init();

    // 3) Load whitelist from LittleFS
    oms::NodeTracker::instance().loadWhitelist();

    // 4) Initialise board hardware (LoRa SPI, LED, battery ADC)
    oms::theBoard()->init();

    // 5) Initialise MeshCore radio + protocol stack
    oms::MeshService::instance().init();

    // 6) Initialise BLE companion service
    oms::BLECompanion::instance().init();

    // 7) Start watchdog (30s timeout)
    Watchdog::init(30);

    // 8) Heap monitoring (nRF52 has 256KB RAM, no PSRAM)
    //    Shorter alert threshold since there's less RAM to work with.
    oms::HeapMonitor::instance().init(60, 8000, 4000);

    OMS_LOG("main", "Ready (RAK4631 headless mode: mesh + BLE)");
}

void loop() {
    oms::Watchdog::feed();
    oms::theBoard()->tick();

    // MeshCore protocol tick
    oms::MeshService::instance().tick();

    // BLE companion tick
    oms::BLECompanion::instance().tick();

    // Deferred config save
    oms::config::tick();

    // Heap monitoring
    oms::HeapMonitor::instance().tick();

    // nRF52 doesn't need explicit idle yield — the Arduino loop handles it.
    // The nRF52 Arduino core automatically enters WFE between loop iterations.
}

// =============================================================================
//  Heltec WiFi LoRa 32 V3 — Headless mesh node / BLE companion
// =============================================================================
#else  // OMS_PLATFORM_HELTEC_V3

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { /* wait up to 3s for serial */ }

    OMS_LOG("main", "OpenMeshOS v" OMS_VERSION_STRING " starting (Heltec V3, headless)");

    // 0) Check for previous crash
    if (oms::CrashLog::hasCrash()) {
        oms::CrashLog::showCrashReport();
    }
    oms::CrashLog::installHandler();

    // 1) Initialise SPIFFS
    if (!SPIFFS.begin(true)) {
        OMS_LOG("main", "WARNING: SPIFFS mount failed, formatting");
        SPIFFS.format();
        if (!SPIFFS.begin(true)) {
            OMS_LOG("main", "ERROR: SPIFFS unavailable, some features will fail");
        }
    }

    // 2) Load persistent config
    oms::config::init();

    // 3) Load whitelist from SPIFFS
    oms::NodeTracker::instance().loadWhitelist();

    // 4) Initialise board hardware (LoRa SPI, LED, battery ADC, OLED reset)
    oms::theBoard()->init();

    // 5) Initialise MeshCore radio + protocol stack
    oms::MeshService::instance().init();

    // 6) Initialise BLE companion service
    oms::BLECompanion::instance().init();

    // 7) Start watchdog (30s timeout)
    oms::Watchdog::init(30);

    // 8) Power management
    oms::PowerManager::instance().init();

    // 9) Heap monitoring
    oms::HeapMonitor::instance().init(60, 20000, 10000);

    OMS_LOG("main", "Ready (headless mode: mesh + BLE)");
}

void loop() {
    oms::Watchdog::feed();
    oms::theBoard()->tick();

    // MeshCore protocol tick
    oms::MeshService::instance().tick();

    // BLE companion tick
    oms::BLECompanion::instance().tick();

    // Deferred config save
    oms::config::tick();

    // Heap monitoring
    oms::HeapMonitor::instance().tick();

    // Power: yield to idle
    oms::PowerManager::instance().idle();
}

#endif  // OMS_PLATFORM_HELTEC_V3 / RAK4631