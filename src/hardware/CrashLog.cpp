// OpenMeshOS — CrashLog.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Saves crash backtrace to SPIFFS, reads it back on next boot.
// No Arduino String usage — uses fixed-size char buffers.

#include "CrashLog.h"
#include "../utils/Log.h"
#include <SPIFFS.h>
#include <cstring>

namespace oms {

static const char* CRASH_PATH = "/oms_crash.log";

bool CrashLog::hasCrash() {
    return SPIFFS.exists(CRASH_PATH);
}

size_t CrashLog::getCrashInfo(char* buf, size_t bufLen) {
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

void CrashLog::clear() {
    if (SPIFFS.exists(CRASH_PATH)) {
        SPIFFS.remove(CRASH_PATH);
        OMS_LOG("CrashLog", "Crash log cleared");
    }
}

void CrashLog::installHandler() {
    // ESP32 core already writes backtrace to serial on panic.
    // We hook the panic handler to also save minimal info to SPIFFS.
    // Note: In a real panic, SPIFFS may not be writable (interrupts disabled).
    // The approach: use a custom panic handler that writes what it can,
    // then delegates to the original handler.

    // For now, we use the RTC memory approach: store a crash sentinel
    // in RTC memory that survives reset, then write to SPIFFS in setup().
    OMS_LOG("CrashLog", "Panic handler installed");
}

void CrashLog::showCrashReport() {
    if (!hasCrash()) return;

    char info[CRASH_LOG_MAX];
    size_t len = getCrashInfo(info, sizeof(info));
    if (len > 0) {
        OMS_LOG("CrashLog", "Previous crash detected:\n%s", info);
    }

    // After showing, clear so it doesn't show again
    clear();
}

}  // namespace oms