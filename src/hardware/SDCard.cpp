// OpenMeshOS — SDCard.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// SD card manager. Prevents corruption by:
// - Tracking mount state (no double-init)
// - Providing safe unmount before SD removal
// - Wrapping file reads with proper open/close

#include "SDCard.h"
#include "../utils/Log.h"

namespace oms {

SDCard& SDCard::instance() {
    static SDCard inst;
    return inst;
}

bool SDCard::init(uint8_t csPin) {
    _csPin = csPin;
    return mount();
}

bool SDCard::mount() {
    if (_mounted) {
        OMS_LOG("SD", "Already mounted, skipping");
        return true;
    }

    if (!SD.begin(_csPin)) {
        OMS_LOG("SD", "Mount failed (no card?)");
        return false;
    }

    _mounted = true;
    uint64_t size = SD.cardSize() / (1024 * 1024);
    OMS_LOG("SD", "Mounted, size: %llu MB", size);
    return true;
}

void SDCard::unmount() {
    if (!_mounted) return;

    // Flush any pending writes by closing all open files
    // (Arduino SD library doesn't have explicit flush-all, but SD.end() handles it)
    SD.end();
    _mounted = false;
    OMS_LOG("SD", "Unmounted safely");
}

size_t SDCard::readFile(const char* path, uint8_t* buf, size_t maxLen) {
    if (!_mounted) {
        OMS_LOG("SD", "Not mounted, can't read %s", path);
        return 0;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
        OMS_LOG("SD", "Can't open %s", path);
        return 0;
    }

    size_t bytesRead = f.read(buf, maxLen);
    f.close();
    return bytesRead;
}

bool SDCard::exists(const char* path) {
    if (!_mounted) return false;
    return SD.exists(path);
}

uint64_t SDCard::cardSizeMB() const {
    if (!_mounted) return 0;
    return SD.cardSize() / (1024 * 1024);
}

}  // namespace oms