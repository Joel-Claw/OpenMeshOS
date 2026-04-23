// OpenMeshOS — MapEngine.cpp
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Offline map rendering for ESP32 with LVGL.
// Uses PNG tiles from SD card, rendered as LVGL images.
// Tile naming follows OSM slippy map convention:
//   /map/{z}/{x}/{y}.png

#include "MapEngine.h"
#include "../utils/Log.h"
#include <SD.h>
#include <math.h>

namespace oms {

// ── Constants ───────────────────────────────────────────────────────
static constexpr uint8_t SD_CS_PIN = 11;   // T-Deck SD card CS
static constexpr int TILE_SIZE = 256;        // standard OSM tile size

// ── Coordinate math ─────────────────────────────────────────────────
void MapEngine::latLngToTile(float lat, float lng, int z, int& tx, int& ty) {
    float n = powf(2.0f, z);
    tx = (int)((lng + 180.0f) / 360.0f * n);
    float latRad = lat * M_PI / 180.0f;
    ty = (int)((1.0f - logf(tanf(latRad) + 1.0f / cosf(latRad)) / M_PI) / 2.0f * n);
    // Clamp
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= (int)n) tx = (int)n - 1;
    if (ty >= (int)n) ty = (int)n - 1;
}

void MapEngine::tileToLatLng(int tx, int ty, int z, float& lat, float& lng) {
    float n = powf(2.0f, z);
    lng = tx / n * 360.0f - 180.0f;
    float latRad = atanf(sinhf(M_PI * (1.0f - 2.0f * ty / n)));
    lat = latRad * 180.0f / M_PI;
}

// ── Init ────────────────────────────────────────────────────────────
void MapEngine::init() {
    OMS_LOG("Map", "Initialising map engine");

    // Try SD card
    if (SD.begin(SD_CS_PIN)) {
        if (SD.exists("/map")) {
            _sdPresent = true;
            OMS_LOG("Map", "SD card with /map/ directory found");
        } else {
            OMS_LOG("Map", "SD card present but no /map/ directory");
        }
    } else {
        OMS_LOG("Map", "No SD card — map tiles unavailable");
    }
}

void MapEngine::tick() {
    // Future: background tile loading / caching
}

// ── Pan / zoom ──────────────────────────────────────────────────────
void MapEngine::setCenter(float lat, float lng) {
    _centerLat = lat;
    _centerLng = lng;
}

void MapEngine::setZoom(int z) {
    if (z < 0) z = 0;
    if (z > 18) z = 18;
    _zoom = z;
}

void MapEngine::pan(int dxPx, int dyPx) {
    // Convert pixel delta to lat/lng delta at current zoom
    float n = powf(2.0f, _zoom);
    float degPerPx = 360.0f / (n * TILE_SIZE);
    _centerLng += dxPx * degPerPx;
    _centerLat -= dyPx * degPerPx;
    // Clamp latitude to +/- 85 degrees (Mercator limit)
    if (_centerLat > 85.0f) _centerLat = 85.0f;
    if (_centerLat < -85.0f) _centerLat = -85.0f;
}

void MapEngine::zoomIn()  { setZoom(_zoom + 1); }
void MapEngine::zoomOut() { setZoom(_zoom - 1); }

// ── Nodes ───────────────────────────────────────────────────────────
void MapEngine::addNode(const MapNode& node) {
    if (_nodeCount < 64) {
        _nodes[_nodeCount++] = node;
    }
}

void MapEngine::clearNodes() { _nodeCount = 0; }
uint16_t MapEngine::nodeCount() const { return _nodeCount; }

// ── Tile checks ─────────────────────────────────────────────────────
bool MapEngine::hasTileDir() const { return _sdPresent; }

bool MapEngine::tileExists(int z, int x, int y) const {
    if (!_sdPresent) return false;
    char path[32];
    snprintf(path, sizeof(path), "/map/%d/%d/%d.png", z, x, y);
    return SD.exists(path);
}

// ── Rendering ────────────────────────────────────────────────────────
bool MapEngine::renderFrame() {
    // TODO: implement tile blitting to LVGL canvas
    // 1. Calculate which tiles are visible at current center/zoom
    // 2. Load PNG tiles from SD card (or use cached LVGL images)
    // 3. Draw nodes as circles on top of the tile layer
    // 4. Return true if any tile was loaded this frame
    return false;
}

}  // namespace oms