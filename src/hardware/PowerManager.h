// OpenMeshOS — PowerManager.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Power management: light sleep between loop iterations to save battery.
// Wakes on GPIO events (keyboard, trackball, LoRa IRQ).

#pragma once

#include <Arduino.h>

namespace oms {

class PowerManager {
public:
    static PowerManager& instance();

    // Initialize power management (call once after hardware init)
    void init();

    // Call at end of each loop() iteration.
    // If no events are pending, enters light sleep for a few ms.
    void idle();

    // Force no-sleep mode (e.g. during OTA update or active LoRa TX)
    void setNoSleep(bool noSleep);

    // Get approximate time spent in sleep (for diagnostics)
    uint32_t sleepTimeMs() const { return _totalSleepMs; }

private:
    PowerManager() = default;
    bool _noSleep = false;
    uint32_t _totalSleepMs = 0;
    bool _initialized = false;
};

}  // namespace oms