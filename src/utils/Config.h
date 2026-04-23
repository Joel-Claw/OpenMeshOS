// OpenMeshOS — Config.h
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal

#pragma once

#include <Arduino.h>

namespace oms {

struct Config {
    char radioRegion[8];        // "EU868", "US915", etc
    char callsign[16];          // user-chosen name on the mesh
    int  channel;               // channel index (0 = public)
    int  brightness;            // 0-255 backlight
    int  screenTimeoutSec;      // seconds before screen sleeps
    bool notifySound;           // play sound on incoming message
    char mapTileDir[32];        // SD card path for map tiles
    int  theme;                 // 0 = dark, 1 = light
};

namespace config {
    void init();
    void save();
    const Config& get();

    void setCallsign(const char* cs);
    void setRegion(const char* reg);
}

}  // namespace oms