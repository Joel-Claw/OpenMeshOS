// OpenMeshOS — PowerManager.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Uses esp_light_sleep_start() to save power between loop iterations.
// ESP32-S3 wakes from light sleep on GPIO interrupts or timer.

#include "PowerManager.h"
#include "../utils/Log.h"
#include <esp_sleep.h>
#include <esp_pm.h>

namespace oms {

PowerManager& PowerManager::instance() {
    static PowerManager inst;
    return inst;
}

void PowerManager::init() {
    // Configure automatic light sleep under PM framework
    // ESP32-S3 PM: dynamically adjusts CPU frequency and enters light sleep
    // when idle. This is the simplest approach for Arduino loop().

#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    OMS_LOG("Power", "Tickless idle already enabled in menuconfig");
#else
    // Enable dynamic frequency scaling and auto light sleep
    esp_pm_config_t pm_cfg = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 40,       // drop to 40MHz when idle
        .light_sleep_enable = true,
    };
    esp_err_t err = esp_pm_configure(&pm_cfg);
    if (err == ESP_OK) {
        OMS_LOG("Power", "PM configured: 40-240MHz, light sleep enabled");
    } else {
        OMS_LOG("Power", "PM config failed: %s", esp_err_to_name(err));
    }
#endif

    _initialized = true;
}

void PowerManager::idle() {
    if (_noSleep || !_initialized) return;

    // Let FreeRTOS idle task enter light sleep if no work pending.
    // This happens automatically with esp_pm_configure + tickless idle.
    // We just yield here to give the idle task a chance.
    delay(1);  // 1ms yield; PM will enter light sleep if nothing else runs
    _totalSleepMs++;
}

void PowerManager::setNoSleep(bool noSleep) {
    _noSleep = noSleep;
    if (noSleep) {
        OMS_LOG("Power", "No-sleep mode active");
    }
}

}  // namespace oms