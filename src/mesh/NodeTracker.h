// OpenMeshOS — NodeTracker.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tracks mesh nodes discovered via advertisements.
// Populated by OpenMesh::onAdvertRecv(), consumed by ScreenScanner.
// Fixed-size array to avoid dynamic allocation on ESP32-S3.
//
// Whitelist is persisted to SPIFFS (/whitelist.bin) on toggle
// and loaded on boot.  File format: 32 bytes per entry (raw pub_key).

#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <helpers/AdvertDataHelpers.h>

namespace oms {

// Maximum tracked nodes (ring buffer — oldest evicted when full)
static constexpr size_t MAX_TRACKED_NODES = 32;

// Advert type constants (from MeshCore's AdvertDataHelpers)
static constexpr uint8_t NODE_TYPE_CHAT     = ADV_TYPE_CHAT;
static constexpr uint8_t NODE_TYPE_REPEATER = ADV_TYPE_REPEATER;
static constexpr uint8_t NODE_TYPE_ROOM     = ADV_TYPE_ROOM;
static constexpr uint8_t NODE_TYPE_SENSOR  = ADV_TYPE_SENSOR;

/// A single discovered mesh node
struct TrackedNode {
    uint8_t  pub_key[32];       // ed25519 public key (first 4 bytes for display)
    char     name[24];           // node callsign/name from advert
    uint8_t  type;               // ADV_TYPE_*
    int32_t  lat;                // latitude * 1E6 (0 = unknown)
    int32_t  lon;                // longitude * 1E6 (0 = unknown)
    int      rssi;               // last RSSI (dBm)
    uint32_t lastSeenMs;         // millis() when last advert received
    uint32_t firstSeenMs;        // millis() when first advert received
    bool     whitelisted;        // user has whitelisted this node
};

/// Fixed-size node tracker — single producer (MeshCore callbacks),
/// single consumer (UI). No mutex needed since both run on main thread.
class NodeTracker {
public:
    NodeTracker() : _count(0), _selfAdvertCount(0), _whitelistKeyCount(0) {}

    /// Called from OpenMesh::onAdvertRecv() when a node advertises.
    /// Updates existing entry or adds new one. Evicts oldest if full.
    void onAdvert(const uint8_t pub_key[32],
                  uint8_t adv_type,
                  const char* name,
                  int32_t lat, int32_t lon,
                  int rssi);

    /// Get node by index (0..count()-1). Returns nullptr if out of range.
    const TrackedNode* get(size_t idx) const {
        if (idx >= _count) return nullptr;
        return &_nodes[idx];
    }

    /// Total tracked nodes
    size_t count() const { return _count; }

    /// Number of adverts received from our own node (self-adverts are skipped)
    uint32_t selfAdvertCount() const { return _selfAdvertCount; }

    /// Toggle whitelist on a node by index and persist to SPIFFS
    void toggleWhitelist(size_t idx) {
        if (idx < _count) {
            _nodes[idx].whitelisted = !_nodes[idx].whitelisted;
            saveWhitelist();
        }
    }

    /// Find node by public key prefix (first 4 bytes). Returns index or -1.
    int findByPrefix(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) const {
        for (size_t i = 0; i < _count; i++) {
            if (_nodes[i].pub_key[0] == b0 &&
                _nodes[i].pub_key[1] == b1 &&
                _nodes[i].pub_key[2] == b2 &&
                _nodes[i].pub_key[3] == b3) {
                return (int)i;
            }
        }
        return -1;
    }

    /// Count nodes by type
    size_t countByType(uint8_t type) const {
        size_t n = 0;
        for (size_t i = 0; i < _count; i++) {
            if (_nodes[i].type == type) n++;
        }
        return n;
    }

    /// Count whitelisted nodes
    size_t countWhitelisted() const {
        size_t n = 0;
        for (size_t i = 0; i < _count; i++) {
            if (_nodes[i].whitelisted) n++;
        }
        return n;
    }

    /// Clear all tracked nodes
    void clear() { _count = 0; }

    /// Save whitelisted node keys to SPIFFS
    void saveWhitelist();

    /// Load whitelisted node keys from SPIFFS (call once at boot)
    void loadWhitelist();

    /// Singleton instance
    static NodeTracker& instance() {
        static NodeTracker s_tracker;
        return s_tracker;
    }

private:
    TrackedNode _nodes[MAX_TRACKED_NODES];
    size_t      _count;
    uint32_t    _selfAdvertCount;

    // Pre-loaded whitelist keys from SPIFFS (populated at boot before any adverts arrive)
    static constexpr size_t MAX_WHITELIST_KEYS = 32;
    uint8_t     _whitelistKeys[MAX_WHITELIST_KEYS][32];
    size_t      _whitelistKeyCount;

    /// Check if a public key is in the loaded whitelist
    bool isWhitelistedKey(const uint8_t pub_key[32]) const;

    /// Find existing node by full public key. Returns index or -1.
    int findExisting(const uint8_t pub_key[32]) const {
        for (size_t i = 0; i < _count; i++) {
            if (memcmp(_nodes[i].pub_key, pub_key, 32) == 0) {
                return (int)i;
            }
        }
        return -1;
    }
};

}  // namespace oms