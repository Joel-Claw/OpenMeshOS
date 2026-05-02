// OpenMeshOS — MessageBus.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Thread-safe message queue between MeshCore callbacks and UI.
// MeshCore calls onGroupDataRecv/onPeerDataRecv from its loop(),
// which runs in the Arduino main thread — same as LVGL. But we
// still decouple to keep callbacks fast and UI updates batched.

#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace oms {

// Maximum message length (bytes) — matches MeshCore's typical payload
static constexpr size_t MSG_MAX_LEN = 251;

// Maximum queued messages before oldest are dropped
static constexpr size_t MSG_QUEUE_SIZE = 32;

// Payload type constants (from MeshCore)
static constexpr uint8_t PAYLOAD_TXT  = 1;   // text message
static constexpr uint8_t PAYLOAD_ACK  = 2;   // acknowledgement

/// MessageType: group channel message or direct message
enum class MsgKind : uint8_t {
    GroupChannel,   // broadcast on a group channel
    DirectMessage,  // encrypted DM to us
    SystemInfo,     // status/log message from the node itself
};

/// A single received message
struct InboxMessage {
    MsgKind  kind;
    uint8_t  channel_id;        // group channel index (0 = public)
    char     sender[9];         // short callsign or hex prefix
    char     text[MSG_MAX_LEN + 1];
    uint32_t timestamp;          // RTC or millis-based
    int      rssi;              // signal strength
};

/// Ring-buffer message queue (single producer, single consumer)
class MessageBus {
public:
    MessageBus() : _head(0), _tail(0), _count(0), _dropped(0) {}

    /// Push a message into the inbox (called from MeshCore callbacks).
    /// Returns false if the queue is full (oldest message is dropped).
    bool push(const InboxMessage& msg) {
        if (_count >= MSG_QUEUE_SIZE) {
            // Drop oldest
            _tail = (_tail + 1) % MSG_QUEUE_SIZE;
            _count--;
            _dropped++;
        }
        _buf[_head] = msg;
        _head = (_head + 1) % MSG_QUEUE_SIZE;
        _count++;
        return true;
    }

    /// Pop the oldest message from the inbox (called from UI tick).
    /// Returns false if empty.
    bool pop(InboxMessage& msg) {
        if (_count == 0) return false;
        msg = _buf[_tail];
        _tail = (_tail + 1) % MSG_QUEUE_SIZE;
        _count--;
        return true;
    }

    /// Peek at the oldest message without removing it.
    bool peek(InboxMessage& msg) const {
        if (_count == 0) return false;
        msg = _buf[_tail];
        return true;
    }

    size_t count() const { return _count; }
    size_t dropped() const { return _dropped; }

    /// Global inbox — single instance
    static MessageBus& inbox() {
        static MessageBus s_inbox;
        return s_inbox;
    }

private:
    InboxMessage _buf[MSG_QUEUE_SIZE];
    volatile size_t _head;
    volatile size_t _tail;
    volatile size_t _count;
    size_t _dropped;
};

}  // namespace oms