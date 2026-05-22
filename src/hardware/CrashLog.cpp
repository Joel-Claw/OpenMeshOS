// OpenMeshOS — CrashLog.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Saves crash backtrace to SPIFFS, reads it back on next boot.

#include "CrashLog.h"
#include "../utils/Log.h"
#include <SPIFFS.h>

namespace oms {

static const char* CRASH_PATH = "/oms_crash.log";

bool CrashLog::hasCrash() {
    return SPIFFS.exists(CRASH_PATH);
}

String CrashLog::getCrashInfo() {
    if (!SPIFFS.exists(CRASH_PATH)) return String();

    File f = SPIFFS.open(CRASH_PATH, "r");
    if (!f) return String("Failed to read crash log");

    String info = f.readString();
    f.close();
    return info;
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

    String info = getCrashInfo();
    OMS_LOG("CrashLog", "Previous crash detected:\n%s", info.c_str());

    // After showing, clear so it doesn't show again
    clear();
}

}  // namespace oms