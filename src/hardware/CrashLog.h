// OpenMeshOS — CrashLog.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Saves crash info to SPIFFS on panic, shows on next boot.
// Uses ESP32 core panic handler hook.

#pragma once

#include <Arduino.h>

namespace oms {

class CrashLog {
public:
    // Check for previous crash on boot, return true if one exists
    static bool hasCrash();

    // Get the last crash info string
    static String getCrashInfo();

    // Clear stored crash log
    static void clear();

    // Install panic handler (call once at startup)
    static void installHandler();

    // Show crash info to user (call from setup if hasCrash())
    static void showCrashReport();
};

}  // namespace oms