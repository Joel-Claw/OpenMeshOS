// OpenMeshOS — MeshService.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

// Forward declarations
class IBoard;
class TDeckBoard;
class TDeckClock;
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
    TDeckBoard& board() { return *_meshBoard; }
    TDeckClock& clock() { return *_clock; }

    // Access to the chat layer (for contact list, adverts, etc.)
    OpenMeshChat* chat() { return _chat; }

private:
    bool _initialized = false;
    TDeckBoard* _meshBoard = nullptr;
    TDeckClock* _clock = nullptr;
    OpenMeshChat* _chat = nullptr;
};

}  // namespace oms