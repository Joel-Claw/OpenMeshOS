// OpenMeshOS — Watchdog.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Hardware watchdog timer. Auto-reboots if loop() stalls >30s.

#pragma once

#include <esp_task_wdt.h>

namespace oms {

class Watchdog {
public:
    static void init(uint32_t timeoutSec = 30) {
        OMS_LOG("Watchdog", "Starting, timeout=%lus", (unsigned long)timeoutSec);
        // Old ESP-IDF API: esp_task_wdt_init(timeout, panic)
        // New ESP-IDF 5.x API: esp_task_wdt_init(&config)
        // The Arduino framework currently ships ESP-IDF 4.x style.
        esp_task_wdt_init(timeoutSec, true);  // panic on timeout
        esp_task_wdt_add(nullptr);  // add current task
    }

    static void feed() {
        esp_task_wdt_reset();
    }

    static void deinit() {
        esp_task_wdt_delete(nullptr);
        esp_task_wdt_deinit();
    }
};

}  // namespace oms