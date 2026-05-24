// OpenMeshOS — PowerManager.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Uses esp_light_sleep_start() to save power between loop iterations.
// ESP32-S3 wakes from light sleep on GPIO interrupts or timer.
//
// Power management is configured at build time. If the ESP-IDF PM
// framework is enabled (CONFIG_PM_ENABLE), we use esp_pm_configure()
// for automatic DVFS and light sleep. Otherwise, we fall back to a
// simple yield-based idle that still saves some power.

#include "PowerManager.h"
#include "../utils/Log.h"
#include <esp_sleep.h>

#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif

namespace oms {

PowerManager& PowerManager::instance() {
    static PowerManager inst;
    return inst;
}

void PowerManager::init() {
#if CONFIG_PM_ENABLE
    // Enable dynamic frequency scaling and auto light sleep.
    // This requires CONFIG_PM_ENABLE=y and CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
    // in sdkconfig, which is set when using a custom partition with PM support.
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
#else
    // No ESP-IDF PM framework. Use simple yield-based idle.
    // The device will still work fine; it just won't auto-adjust CPU
    // frequency or enter light sleep. This is the default for Arduino
    // framework builds without custom sdkconfig.
    OMS_LOG("Power", "PM framework not available, using simple yield idle");
#endif

    _initialized = true;
}

void PowerManager::idle() {
    if (_noSleep || !_initialized) return;

    // Let FreeRTOS idle task enter light sleep if no work pending.
    // With PM configured + tickless idle, this happens automatically.
    // Without PM, we just yield to give other tasks a chance.
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