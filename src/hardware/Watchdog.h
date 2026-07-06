// OpenMeshOS — Watchdog.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Hardware watchdog timer. Auto-reboots if loop() stalls >30s.
// Platform-aware: ESP32 uses esp_task_wdt, nRF52 uses Nordic WDT peripheral.

#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

// ── ESP32 implementation ─────────────────────────────────────────────
#include <esp_task_wdt.h>

namespace oms {

class Watchdog {
public:
    static void init(uint32_t timeoutSec = 30) {
        OMS_LOG("Watchdog", "Starting, timeout=%lus", (unsigned long)timeoutSec);
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

#elif defined(ARDUINO_ARCH_NRF52840)

// ── nRF52 implementation ─────────────────────────────────────────────
// nRF52 WDT peripheral: 8-bit counter with configurable prescaler.
// CRV (counter reload value) = timeout_seconds * 32768 (LFCLK frequency).
// The WDT must be fed (RR[0] = 0x16E924D3) within the timeout period,
// otherwise the system resets.
//
// Unlike ESP32's task watchdog, the nRF52 WDT is a single global watchdog.
// We feed it from loop() directly.

#include <nrf.h>

namespace oms {

class Watchdog {
public:
    static void init(uint32_t timeoutSec = 30) {
        OMS_LOG("Watchdog", "Starting nRF52 WDT, timeout=%lus", (unsigned long)timeoutSec);
        // Calculate CRV: timeout in seconds * LFCLK frequency (32768 Hz)
        uint32_t crv = timeoutSec * 32768UL;
        // HALT=0 (Pause): pause WDT when CPU is halted by debugger
        NRF_WDT->CONFIG = 0;  // all fields zero = pause on halt, run otherwise
        NRF_WDT->CRV = crv;
        NRF_WDT->RREN = WDT_RREN_RR0_Msk;  // enable reload register 0
        NRF_WDT->TASKS_START = 1;
        _active = true;
    }

    static void feed() {
        if (_active) {
            NRF_WDT->RR[0] = 0x16E924D3UL;  // magic value to reload WDT
        }
    }

    static void deinit() {
        // nRF52 WDT cannot be stopped once started (hardware safety feature).
        // The WDT runs as long as the WDT peripheral is powered, regardless of
        // the HALT bit. Setting HALT only pauses the WDT when the CPU is halted
        // (debug halt). There is no way to stop it in software.
        // We just mark as inactive so feed() stops reloading the counter,
        // which will cause a reset if the watchdog is not fed by other means.
        _active = false;
    }

private:
    inline static bool _active = false;
};

}  // namespace oms

#else

// ── Unknown platform stub ────────────────────────────────────────────
namespace oms {

class Watchdog {
public:
    static void init(uint32_t timeoutSec = 30) { (void)timeoutSec; }
    static void feed() {}
    static void deinit() {}
};

}  // namespace oms

#endif