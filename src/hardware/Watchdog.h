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
        esp_task_wdt_config_t cfg = {
            .timeout_ms = timeoutSec * 1000,
            .idle_core_mask = (1 << 0),  // monitor core 0 (Arduino loop)
            .trigger_panic = true,
        };
        esp_task_wdt_init(&cfg);
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