// OpenMeshOS — NodeTracker.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Implementation of mesh node tracker. Called from MeshCore advert callback.

#include "NodeTracker.h"
#include "../utils/Log.h"

namespace oms {

void NodeTracker::onAdvert(const uint8_t pub_key[32],
                            uint8_t adv_type,
                            const char* name,
                            int32_t lat, int32_t lon,
                            int rssi) {
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
        node.whitelisted = false;
        _count++;

        OMS_LOG("Mesh", "New node: %s (type=%d, rssi=%d)",
                 node.name, adv_type, rssi);
    }
}

}  // namespace oms