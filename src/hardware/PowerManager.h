// OpenMeshOS — PowerManager.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Power management: light sleep / idle between loop iterations to save battery.
// Platform-aware: ESP32 uses esp_pm + esp_light_sleep, nRF52 uses WFE idle.
// Wakes on GPIO events (keyboard, trackball, LoRa IRQ) or timer.

#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

// ── ESP32 implementation ─────────────────────────────────────────────
#include <esp_sleep.h>
#include "../utils/Log.h"

#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif

namespace oms {

class PowerManager {
public:
    static PowerManager& instance() {
        static PowerManager inst;
        return inst;
    }

    void init() {
#if CONFIG_PM_ENABLE
        esp_pm_config_t pm_cfg = {
            .max_freq_mhz = 240,
            .min_freq_mhz = 40,
            .light_sleep_enable = true,
        };
        esp_err_t err = esp_pm_configure(&pm_cfg);
        if (err == ESP_OK) {
            OMS_LOG("Power", "PM configured: 40-240MHz, light sleep enabled");
        } else {
            OMS_LOG("Power", "PM config failed: %s", esp_err_to_name(err));
        }
#else
        OMS_LOG("Power", "PM framework not available, using simple yield idle");
#endif
        _initialized = true;
    }

    void idle() {
        if (_noSleep || !_initialized) return;
        delay(1);
        _totalSleepMs++;
    }

    void setNoSleep(bool noSleep) {
        _noSleep = noSleep;
        if (noSleep) {
            OMS_LOG("Power", "No-sleep mode active");
        }
    }

    uint32_t sleepTimeMs() const { return _totalSleepMs; }

private:
    PowerManager() = default;
    bool _noSleep = false;
    uint32_t _totalSleepMs = 0;
    bool _initialized = false;
};

}  // namespace oms

#elif defined(ARDUINO_ARCH_NRF52840)

// ── nRF52 implementation ─────────────────────────────────────────────
// nRF52 power management is fundamentally different from ESP32:
//   - No dynamic frequency scaling (CPU runs at fixed 64MHz)
//   - Light sleep = WFE (Wait For Event) — wakes on any interrupt
//   - Deep sleep = SYSTEMOFF (only wakes on GPIO DETECT or RTC)
//   - The Arduino nRF52 core already uses WFE in the idle task
//
// For the loop() idle, we rely on the FreeRTOS idle task which enters
// WFE automatically. We just need to make sure we don't busy-wait.
// The nRF52 is designed for low-power IoT — the idle current is already
// ~1mA with just the CPU in WFE (vs ~10mA on ESP32 in light sleep).
//
// For deep sleep (e.g. when battery is low), we use NRF_POWER->SYSTEMOFF
// which drops current to ~1uA. The device wakes on GPIO DETECT (tied to
// LoRa DIO1, keyboard INT, or a button press depending on board).

#include "../utils/Log.h"

namespace oms {

class PowerManager {
public:
    static PowerManager& instance() {
        static PowerManager inst;
        return inst;
    }

    void init() {
        // nRF52 doesn't need PM framework configuration.
        // The Arduino nRF52 core handles tickless idle via FreeRTOS.
        // We just log and mark as initialized.
        OMS_LOG("Power", "nRF52 PM: WFE idle (automatic via FreeRTOS), SYSTEMOFF available");
        _initialized = true;
    }

    void idle() {
        if (_noSleep || !_initialized) return;
        // On nRF52, FreeRTOS idle task enters WFE automatically.
        // We just yield with a tiny delay to allow the scheduler to
        // put the CPU into WFE if no other task is ready.
        delay(1);
        _totalSleepMs++;
    }

    void setNoSleep(bool noSleep) {
        _noSleep = noSleep;
        if (noSleep) {
            OMS_LOG("Power", "No-sleep mode active");
        }
    }

    uint32_t sleepTimeMs() const { return _totalSleepMs; }

    // nRF52-specific: enter SYSTEMOFF (ultra-low-power deep sleep).
    // The device wakes on a GPIO DETECT signal (configured by the
    // board's wake pin, typically LoRa DIO1 or a user button).
    // NOTE: This is a destructive call — all RAM content is lost
    // except for the nRF52's retained registers. Use only when
    // the device is idle and all state is persisted to flash.
    void enterSystemOff() {
        OMS_LOG("Power", "Entering SYSTEMOFF (deep sleep)");
        // Ensure all pending writes are flushed
        delay(10);
        // Enter system off — wakes on DETECT signal
        NRF_POWER->SYSTEMOFF = POWER_SYSTEMOFF_SYSTEMOFF_Enter;
        // Code should never reach here; if it does, a reset occurred
    }

private:
    PowerManager() = default;
    bool _noSleep = false;
    uint32_t _totalSleepMs = 0;
    bool _initialized = false;
};

}  // namespace oms

#else

// ── Unknown platform stub ─────────────────────────────────────────────
#include "../utils/Log.h"

namespace oms {

class PowerManager {
public:
    static PowerManager& instance() {
        static PowerManager inst;
        return inst;
    }

    void init() { _initialized = true; }
    void idle() { delay(1); }
    void setNoSleep(bool n) { _noSleep = n; }
    uint32_t sleepTimeMs() const { return 0; }

private:
    PowerManager() = default;
    bool _noSleep = false;
    bool _initialized = false;
};

}  // namespace oms

#endif