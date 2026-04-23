// OpenMeshOS — MeshService.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

class MeshService {
public:
    static MeshService& instance();

    void init();
    void tick();

    // Messaging
    bool sendChannel(const char* channel, const char* text);
    bool sendDirect(const uint8_t* pubkey, const char* text);

    // Status
    uint16_t hopCount() const;
    int rssi() const;

    bool initialized() const { return _initialized; }

private:
    bool _initialized = false;
};

}  // namespace oms