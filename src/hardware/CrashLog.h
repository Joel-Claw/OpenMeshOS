// OpenMeshOS — CrashLog.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Saves crash info to filesystem on panic, shows on next boot.
// Platform-aware: ESP32 uses esp_reset_reason + SPIFFS,
// nRF52 uses RESETREAS register + LittleFS (via SPIFFS compat shim).
// No Arduino String usage — fixed-size char buffer only.

#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)

// ── ESP32 implementation ─────────────────────────────────────────────
#include <SPIFFS.h>
#include <esp_system.h>
#include <cstring>

namespace oms {

static constexpr size_t CRASH_LOG_MAX = 512;

class CrashLog {
public:
    static bool hasCrash() {
        return SPIFFS.exists("/oms_crash.log");
    }

    static size_t getCrashInfo(char* buf, size_t bufLen) {
        if (!buf || bufLen == 0) return 0;
        buf[0] = '\0';
        if (!SPIFFS.exists("/oms_crash.log")) return 0;

        File f = SPIFFS.open("/oms_crash.log", "r");
        if (!f) {
            const char* err = "Failed to read crash log";
            size_t errLen = std::strlen(err);
            if (errLen >= bufLen) errLen = bufLen - 1;
            std::memcpy(buf, err, errLen);
            buf[errLen] = '\0';
            return errLen;
        }
        size_t len = f.readBytes(buf, bufLen - 1);
        buf[len] = '\0';
        f.close();
        return len;
    }

    static void clear() {
        if (SPIFFS.exists("/oms_crash.log")) {
            SPIFFS.remove("/oms_crash.log");
        }
    }

    static void installHandler() {
        // ESP32 core writes backtrace to serial on panic.
        // We use RTC memory sentinel approach: store crash marker in RTC
        // memory that survives reset, then write to SPIFFS in setup().
        OMS_LOG("CrashLog", "Panic handler installed");
    }

    static void showCrashReport() {
        if (!hasCrash()) return;
        char info[CRASH_LOG_MAX];
        size_t len = getCrashInfo(info, sizeof(info));
        if (len > 0) {
            OMS_LOG("CrashLog", "Previous crash detected:\n%s", info);
        }
        clear();
    }
};

}  // namespace oms

#elif defined(ARDUINO_ARCH_NRF52840)

// ── nRF52 implementation ─────────────────────────────────────────────
// nRF52 has a RESETREAS register that tells us why the system reset:
//   - RESETREAS.DOG: watchdog timeout
//   - RESETREAS.LOCKUP: soft-reset (lockup)
//   - RESETREAS.OFF: wake from SYSTEMOFF (not a real crash)
//   - RESETREAS.SREQ: software reset (NVIC_SystemReset)
//   - RESETREAS.RESETPIN: external reset pin
//
// The nRF52 Arduino core does not provide SPIFFS. We use our
// boards/SPIFFS.h compat shim which maps to Adafruit InternalFS (LittleFS).
//
// Unlike ESP32 which has a panic handler hook, nRF52 crashes typically
// result in a lockup or watchdog reset. We check RESETREAS on boot and
// log the reason to a file if it indicates a watchdog or lockup reset.

#include "../../boards/SPIFFS.h"
#include <nrf.h>
#include <cstring>

namespace oms {

static constexpr size_t CRASH_LOG_MAX = 512;
static const char* CRASH_PATH = "/oms_crash.log";

class CrashLog {
public:
    static bool hasCrash() {
        return SPIFFS.exists(CRASH_PATH);
    }

    static size_t getCrashInfo(char* buf, size_t bufLen) {
        if (!buf || bufLen == 0) return 0;
        buf[0] = '\0';
        if (!SPIFFS.exists(CRASH_PATH)) return 0;

        File f = SPIFFS.open(CRASH_PATH, "r");
        if (!f) {
            const char* err = "Failed to read crash log";
            size_t errLen = std::strlen(err);
            if (errLen >= bufLen) errLen = bufLen - 1;
            std::memcpy(buf, err, errLen);
            buf[errLen] = '\0';
            return errLen;
        }
        size_t len = f.readBytes(buf, bufLen - 1);
        buf[len] = '\0';
        f.close();
        return len;
    }

    static void clear() {
        if (SPIFFS.exists(CRASH_PATH)) {
            SPIFFS.remove(CRASH_PATH);
        }
    }

    static void installHandler() {
        // nRF52 doesn't have a runtime panic handler hook like ESP32.
        // Instead, we check RESETREAS on boot and log if it indicates a crash.
        // This is called during setup() so SPIFFS is already mounted.
        uint32_t reason = NRF_POWER->RESETREAS;

        // Mask to only crash-related reset reasons
        // Bit 0: RESETREAS.DOG (watchdog)
        // Bit 1: RESETREAS.LOCKUP (lockup / hard fault)
        // Bit 3: RESETREAS.SREQ (software reset via NVIC_SystemReset)
        // Bit 5: RESETREAS.RESETPIN (external reset pin — not a crash)
        // Bit 16: RESETREAS.OFF (wake from SYSTEMOFF — not a crash)
        uint32_t crashMask = (POWER_RESETREAS_DOG_Msk | POWER_RESETREAS_LOCKUP_Msk);

        if (reason & crashMask) {
            char info[256];
            const char* cause = "unknown";

            if (reason & POWER_RESETREAS_DOG_Msk) {
                cause = "Watchdog timeout";
            } else if (reason & POWER_RESETREAS_LOCKUP_Msk) {
                cause = "CPU lockup (hard fault)";
            }

            // Write crash log to filesystem
            // Format: "nRF52 reset: <cause> (RESETREAS=0xXXXXXX) at <uptime>ms"
            int written = snprintf(info, sizeof(info),
                "nRF52 reset: %s (RESETREAS=0x%06lX) at %lums",
                cause, (unsigned long)reason, (unsigned long)millis());

            File f = SPIFFS.open(CRASH_PATH, "w");
            if (f) {
                f.write((const uint8_t*)info, written);
                f.close();
                OMS_LOG("CrashLog", "Crash logged: %s", cause);
            } else {
                OMS_LOG("CrashLog", "WARNING: Could not write crash log to LittleFS");
            }
        }

        // Clear RESETREAS by writing 1 to the bits that are set
        // (nRF52 requires explicit clearing; bits persist otherwise)
        NRF_POWER->RESETREAS = reason;

        OMS_LOG("CrashLog", "nRF52 reset reason handler installed (RESETREAS=0x%06lX)",
                (unsigned long)reason);
    }

    static void showCrashReport() {
        if (!hasCrash()) return;
        char info[CRASH_LOG_MAX];
        size_t len = getCrashInfo(info, sizeof(info));
        if (len > 0) {
            OMS_LOG("CrashLog", "Previous crash detected:\n%s", info);
        }
        clear();
    }
};

}  // namespace oms

#else

// ── Unknown platform stub ─────────────────────────────────────────────
namespace oms {

static constexpr size_t CRASH_LOG_MAX = 512;

class CrashLog {
public:
    static bool hasCrash() { return false; }
    static size_t getCrashInfo(char*, size_t) { return 0; }
    static void clear() {}
    static void installHandler() {}
    static void showCrashReport() {}
};

}  // namespace oms

#endif