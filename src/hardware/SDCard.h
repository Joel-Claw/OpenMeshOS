// OpenMeshOS — SDCard.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// SD card manager with safe mount/unmount and corruption prevention.

#pragma once

#include <Arduino.h>
#include <SD.h>

namespace oms {

class SDCard {
public:
    static SDCard& instance();

    // Initialize SD card (call once at boot)
    bool init(uint8_t csPin = 42);

    // Safe mount/unmount (tracks state, prevents double-init)
    bool mount();
    void unmount();

    bool isMounted() const { return _mounted; }

    // Read a file entirely into a buffer (returns bytes read, 0 on error)
    size_t readFile(const char* path, uint8_t* buf, size_t maxLen);

    // Check if a file exists
    bool exists(const char* path);

    // Get card size in MB
    uint64_t cardSizeMB() const;

private:
    SDCard() = default;
    bool _mounted = false;
    uint8_t _csPin = 42;
};

}  // namespace oms