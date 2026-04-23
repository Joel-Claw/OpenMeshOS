// OpenMeshOS — MapEngine.h
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Offline tile-based map renderer for ESP32.
// Tiles stored on SD card in /map/ directory structure:
//   /map/{z}/{x}/{y}.png
// Using OpenStreetMap tile coordinates (slippy map).

#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace oms {

struct MapNode {
    float lat;
    float lng;
    char  name[16];
    int   rssi;           // signal strength
    bool  isRepeater;
};

class MapEngine {
public:
    void init();
    void tick();

    // Pan/zoom
    void setCenter(float lat, float lng);
    void setZoom(int z);         // z = 0..18
    void pan(int dxPx, int dyPx);
    void zoomIn();
    void zoomOut();

    // Nodes
    void addNode(const MapNode& node);
    void clearNodes();
    uint16_t nodeCount() const;

    // Tile management
    bool hasTileDir() const;      // SD card present with /map/
    bool tileExists(int z, int x, int y) const;

    // Rendering — called from LVGL tick
    // Returns true if a new tile was loaded this frame
    bool renderFrame();

    // Coordinate conversion
    static void latLngToTile(float lat, float lng, int z, int& tx, int& ty);
    static void tileToLatLng(int tx, int ty, int z, float& lat, float& lng);

private:
    float  _centerLat = 49.6117f;   // Luxembourg :)
    float  _centerLng = 6.1300f;
    int    _zoom = 10;                // country-level default
    MapNode _nodes[64];
    uint16_t _nodeCount = 0;
    bool   _sdPresent = false;
};

}  // namespace oms