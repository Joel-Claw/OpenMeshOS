// OpenMeshOS — CrashLog.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Saves crash info to SPIFFS on panic, shows on next boot.
// Uses ESP32 core panic handler hook.
// No Arduino String usage — fixed-size char buffer only.

#pragma once

#include <Arduino.h>

namespace oms {

// Maximum crash log size to read (SPIFFS files are small)
static constexpr size_t CRASH_LOG_MAX = 512;

class CrashLog {
public:
    // Check for previous crash on boot, return true if one exists
    static bool hasCrash();

    // Get crash info into a caller-provided buffer.
    // Returns the length of the info string (0 if no crash).
    // Buffer is always null-terminated.
    static size_t getCrashInfo(char* buf, size_t bufLen);

    // Clear stored crash log
    static void clear();

    // Install panic handler (call once at startup)
    static void installHandler();

    // Show crash info to user (call from setup if hasCrash())
    static void showCrashReport();
};

}  // namespace oms