// OpenMeshOS — MeshService.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

// Forward declarations
class IBoard;
class MeshBoard;
class MeshClock;
class OpenMeshChat;

/// MeshService is the central bridge between MeshCore's protocol stack
/// and the OpenMeshOS UI. It owns the radio, identity, and message loop.
///
/// Uses OpenMeshChat (BaseChatMesh subclass) for chat-level features:
/// contact discovery, group channels, DM ACKs, identity management.
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
    uint16_t nodeCount() const;
    int rssi() const;

    bool initialized() const { return _initialized; }

    // Access to MeshCore board/clock for other subsystems
    MeshBoard& board() { return *_meshBoard; }
    MeshClock& clock() { return *_clock; }

    // Access to the chat layer (for contact list, adverts, etc.)
    OpenMeshChat* chat() { return _chat; }

private:
    bool _initialized = false;
    MeshBoard* _meshBoard = nullptr;
    MeshClock* _clock = nullptr;
    OpenMeshChat* _chat = nullptr;

    // Periodic advert timer (lets other nodes discover us)
    static constexpr uint32_t ADVERT_INTERVAL_MS = 300000;  // 5 minutes base
    static constexpr uint32_t ADVERT_JITTER_MS   = 60000;   // +/- 1 minute random jitter
    uint32_t _lastAdvertMs = 0;
    uint32_t _advertDeadline = 300000;  // first advert 5 min after boot
};

}  // namespace oms