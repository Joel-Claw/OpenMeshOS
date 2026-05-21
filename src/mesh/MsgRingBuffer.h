// OpenMeshOS — MsgRingBuffer.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// PSRAM-backed ring buffer for mesh message history.
// Stores up to RING_CAPACITY messages, dropping the oldest
// when full. Messages are allocated in PSRAM to preserve DRAM
// for LVGL and the display stack.
//
// Thread safety: single-producer (MeshService::tick from main loop),
// single-consumer (UI update from main loop). Since both run in
// the same Arduino thread, no locking is needed.

#pragma once

#include <Arduino.h>
#include "../utils/Log.h"
#include <cstddef>
#include <cstdint>
#include "MessageBus.h"

namespace oms {

/// Maximum stored messages across all channels
static constexpr size_t RING_CAPACITY = 1000;

/// Ring buffer entry — extends InboxMessage with a display-ready
/// formatted string and a sequence number for ordering.
struct RingEntry {
    InboxMessage msg;           // original message
    char    display[MSG_MAX_LEN + 32]; // pre-formatted for UI
    uint32_t seq;               // monotonic sequence number
    bool    valid;              // slot occupied
};

/// PSRAM-backed ring buffer for message history.
///
/// Usage:
///   MsgRingBuffer::instance().push(msg);
///   MsgRingBuffer::instance().messagesForChannel(0, offset, count, out, outMax);
class MsgRingBuffer {
public:
    static MsgRingBuffer& instance() {
        static MsgRingBuffer s_ring;
        return s_ring;
    }

    /// Push a new message. Formats the display string and assigns
    /// a sequence number. If full, the oldest message is evicted.
    void push(const InboxMessage& msg) {
        // Format display string: "HH:MM [CH|DM] sender: text"
        uint32_t secs = msg.timestamp / 1000;
        uint32_t mins = (secs / 60) % 60;
        uint32_t hrs  = (secs / 3600) % 24;
        char timeBuf[8];
        snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", hrs, mins);

        const char* prefix = (msg.kind == MsgKind::DirectMessage) ? "[DM]" : "[CH]";

        _seq++;
        size_t slot = _seq % RING_CAPACITY;
        _buf[slot].msg = msg;
        _buf[slot].seq = _seq;
        _buf[slot].valid = true;
        snprintf(_buf[slot].display, sizeof(RingEntry::display),
                 "%s %s %s: %s", timeBuf, prefix, msg.sender, msg.text);

        if (_count < RING_CAPACITY) {
            _count++;
        }
        // else: we overwrote the oldest (ring wrap), count stays at max
    }

    /// Query messages for a specific channel_id.
    /// Returns up to `outMax` messages in `out`, starting from the
    /// most recent and going backwards (newest first).
    /// Returns the number of messages actually written.
    ///
    /// channel_id: 0 = public, 1 = CH1, 255 = DM
    size_t queryByChannel(uint8_t channel_id, size_t offset, size_t limit,
                          const char** outTexts, size_t outMax) const {
        if (_count == 0 || outMax == 0) return 0;

        size_t written = 0;
        size_t skipped = 0;

        // Walk backwards from most recent
        for (size_t i = 0; i < _count && written < outMax; i++) {
            size_t slot;
            if (_seq >= RING_CAPACITY) {
                // Ring has wrapped: compute slot relative to current seq
                slot = (_seq - i) % RING_CAPACITY;
            } else {
                // Ring hasn't wrapped yet
                if (i > _seq) break;
                slot = (_seq - i) % RING_CAPACITY;
            }

            if (!_buf[slot].valid) continue;

            // Filter by channel
            bool match = false;
            if (channel_id == 255) {
                // DM channel: match direct messages
                match = (_buf[slot].msg.kind == MsgKind::DirectMessage);
            } else if (channel_id == 0) {
                // Public channel: match group channel 0 or system info
                match = (_buf[slot].msg.kind == MsgKind::GroupChannel &&
                         _buf[slot].msg.channel_id == 0) ||
                        (_buf[slot].msg.kind == MsgKind::SystemInfo);
            } else {
                match = (_buf[slot].msg.kind == MsgKind::GroupChannel &&
                         _buf[slot].msg.channel_id == channel_id);
            }

            if (!match) continue;

            if (skipped < offset) {
                skipped++;
                continue;
            }

            outTexts[written] = _buf[slot].display;
            written++;
            if (written >= limit) break;
        }

        return written;
    }

    /// Count messages for a specific channel.
    size_t countByChannel(uint8_t channel_id) const {
        if (_count == 0) return 0;
        size_t n = 0;
        for (size_t i = 0; i < _count; i++) {
            size_t slot;
            if (_seq >= RING_CAPACITY) {
                slot = (_seq - i) % RING_CAPACITY;
            } else {
                if (i > _seq) break;
                slot = (_seq - i) % RING_CAPACITY;
            }
            if (!_buf[slot].valid) continue;

            if (channel_id == 255) {
                if (_buf[slot].msg.kind == MsgKind::DirectMessage) n++;
            } else if (channel_id == 0) {
                if ((_buf[slot].msg.kind == MsgKind::GroupChannel &&
                     _buf[slot].msg.channel_id == 0) ||
                    (_buf[slot].msg.kind == MsgKind::SystemInfo)) n++;
            } else {
                if (_buf[slot].msg.kind == MsgKind::GroupChannel &&
                    _buf[slot].msg.channel_id == channel_id) n++;
            }
        }
        return n;
    }

    size_t total()     const { return _count; }
    uint32_t seqNum()  const { return _seq; }

private:
    MsgRingBuffer() : _buf(nullptr), _count(0), _seq(0) {
        // Allocate ring buffer in PSRAM
        _buf = (RingEntry*)heap_caps_malloc(RING_CAPACITY * sizeof(RingEntry), MALLOC_CAP_SPIRAM);
        if (_buf) {
            memset(_buf, 0, RING_CAPACITY * sizeof(RingEntry));
            OMS_LOG("MEM", "MsgRingBuffer: allocated %u KB in PSRAM",
                    (unsigned)(RING_CAPACITY * sizeof(RingEntry) / 1024));
        } else {
            OMS_LOG("MEM", "MsgRingBuffer: PSRAM alloc FAILED, falling back to DRAM");
            _buf = (RingEntry*)calloc(RING_CAPACITY, sizeof(RingEntry));
        }
    }

    RingEntry*  _buf;
    size_t      _count;     // current number of valid entries (up to RING_CAPACITY)
    uint32_t    _seq;       // monotonically increasing sequence number
};

}  // namespace oms