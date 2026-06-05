// OpenMeshOS — NodeTracker.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Implementation of mesh node tracker. Called from MeshCore advert callback.
// Whitelist is persisted to SPIFFS (/whitelist.bin) on toggle and
// loaded on boot.  File format: 32 bytes per entry (raw pub_key).

#include "NodeTracker.h"
#include "../utils/Log.h"
#include <SPIFFS.h>

namespace oms {

static const char* WHITELIST_PATH = "/whitelist.bin";

/// Sanitize a mesh node name: strip non-printable characters.
/// Returns static buffer (not thread-safe, but only called from main thread).
static const char* sanitizeMeshName(const char* name, char* buf, size_t bufLen) {
    size_t j = 0;
    for (size_t i = 0; name[i] && j < bufLen - 1; i++) {
        char c = name[i];
        if (c >= 0x20 && c < 0x7F) {
            buf[j++] = c;
        }
        // Skip control characters and non-ASCII
    }
    buf[j] = '\0';
    return buf;
}

void NodeTracker::onAdvert(const uint8_t pub_key[32],
                            uint8_t adv_type,
                            const char* name,
                            int32_t lat, int32_t lon,
                            int rssi) {
    // Sanitize name to strip non-printable characters
    char safeName[24];
    if (name && name[0]) {
        sanitizeMeshName(name, safeName, sizeof(safeName));
        name = safeName;
    }
    // Check if we already know this node
    int idx = findExisting(pub_key);

    if (idx >= 0) {
        // Update existing entry
        TrackedNode& node = _nodes[idx];
        if (name && name[0]) {
            strncpy(node.name, name, sizeof(node.name) - 1);
            node.name[sizeof(node.name) - 1] = '\0';
        }
        node.type = adv_type;
        if (lat != 0 || lon != 0) {
            node.lat = lat;
            node.lon = lon;
        }
        node.rssi = rssi;
        node.lastSeenMs = millis();
    } else {
        // Add new node
        if (_count >= MAX_TRACKED_NODES) {
            // Evict oldest (by firstSeenMs)
            uint32_t oldest = _nodes[0].firstSeenMs;
            size_t oldestIdx = 0;
            for (size_t i = 1; i < _count; i++) {
                if (_nodes[i].firstSeenMs < oldest) {
                    oldest = _nodes[i].firstSeenMs;
                    oldestIdx = i;
                }
            }
            // Shift remaining nodes down
            for (size_t i = oldestIdx; i < _count - 1; i++) {
                _nodes[i] = _nodes[i + 1];
            }
            _count--;
        }

        TrackedNode& node = _nodes[_count];
        memset(&node, 0, sizeof(node));
        memcpy(node.pub_key, pub_key, 32);
        if (name && name[0]) {
            strncpy(node.name, name, sizeof(node.name) - 1);
            node.name[sizeof(node.name) - 1] = '\0';
        } else {
            snprintf(node.name, sizeof(node.name), "%02X%02X%02X%02X",
                     pub_key[0], pub_key[1], pub_key[2], pub_key[3]);
        }
        node.type = adv_type;
        node.lat = lat;
        node.lon = lon;
        node.rssi = rssi;
        node.lastSeenMs = millis();
        node.firstSeenMs = millis();
        node.whitelisted = isWhitelistedKey(pub_key);  // check loaded whitelist
        _count++;

        OMS_LOG("Mesh", "New node: %s (type=%d, rssi=%d, wl=%s)",
                 node.name, adv_type, rssi, node.whitelisted ? "yes" : "no");
    }
}

// ── Whitelist persistence ───────────────────────────────────────────

bool NodeTracker::isWhitelistedKey(const uint8_t pub_key[32]) const {
    for (size_t i = 0; i < _whitelistKeyCount; i++) {
        if (memcmp(_whitelistKeys[i], pub_key, 32) == 0) {
            return true;
        }
    }
    return false;
}

void NodeTracker::loadWhitelist() {
    _whitelistKeyCount = 0;

    if (!SPIFFS.exists(WHITELIST_PATH)) {
        OMS_LOG("Mesh", "No whitelist file");
        return;
    }

    File f = SPIFFS.open(WHITELIST_PATH, "r");
    if (!f) {
        OMS_LOG("Mesh", "Failed to open whitelist");
        return;
    }

    size_t fileSize = f.size();
    size_t entryCount = fileSize / 32;

    if (entryCount > MAX_WHITELIST_KEYS) {
        entryCount = MAX_WHITELIST_KEYS;
    }

    for (size_t i = 0; i < entryCount; i++) {
        if (f.read(_whitelistKeys[i], 32) != 32) break;
        _whitelistKeyCount++;
    }

    f.close();
    OMS_LOG("Mesh", "Loaded %zu whitelisted keys", _whitelistKeyCount);
}

void NodeTracker::saveWhitelist() {
    // Collect all currently whitelisted keys from the tracker
    uint8_t keys[MAX_WHITELIST_KEYS][32];
    size_t count = 0;

    for (size_t i = 0; i < _count && count < MAX_WHITELIST_KEYS; i++) {
        if (_nodes[i].whitelisted) {
            memcpy(keys[count], _nodes[i].pub_key, 32);
            count++;
        }
    }

    // Also include any pre-loaded whitelist keys that aren't in the tracker yet
    for (size_t i = 0; i < _whitelistKeyCount && count < MAX_WHITELIST_KEYS; i++) {
        bool alreadyIncluded = false;
        for (size_t j = 0; j < count; j++) {
            if (memcmp(keys[j], _whitelistKeys[i], 32) == 0) {
                alreadyIncluded = true;
                break;
            }
        }
        if (!alreadyIncluded) {
            memcpy(keys[count], _whitelistKeys[i], 32);
            count++;
        }
    }

    File f = SPIFFS.open(WHITELIST_PATH, "w");
    if (!f) {
        OMS_LOG("Mesh", "Failed to save whitelist");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        f.write(keys[i], 32);
    }

    f.close();

    // Update the in-memory whitelist keys
    _whitelistKeyCount = count;
    for (size_t i = 0; i < count; i++) {
        memcpy(_whitelistKeys[i], keys[i], 32);
    }

    OMS_LOG("Mesh", "Saved %zu whitelisted keys", count);
}

}  // namespace oms