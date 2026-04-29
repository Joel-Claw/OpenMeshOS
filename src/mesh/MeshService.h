// OpenMeshOS — MeshService.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

// Forward declarations
class TDeckBoard;
class TDeckClock;

class MeshService {
public:
    MeshService() = default;
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

    // Access to board/clock for other subsystems
    TDeckBoard& board() { return *_board; }
    TDeckClock& clock() { return *_clock; }

private:
    bool _initialized = false;
    TDeckBoard* _board = nullptr;
    TDeckClock* _clock = nullptr;
};

}  // namespace oms