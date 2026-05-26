// OpenMeshOS — MapTypes.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Shared types for map rendering (node markers, coordinates).
// Separated from MapEngine.h and TileRenderer.h to avoid circular includes.

#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace oms {

/// A map node marker (our position, contacts, repeaters).
struct MapNode {
    float lat;
    float lng;
    char  name[16];
    int   rssi;           // signal strength
    bool  isRepeater;
};

}  // namespace oms