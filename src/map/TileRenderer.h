// OpenMeshOS — TileRenderer.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Renders map tiles from SD card onto an LVGL canvas.
// Uses lodepng for PNG decoding (lightweight, no deps).
// Implements progressive loading: center tile first, then edges.
// PSRAM LRU cache for recently used tiles.

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <SD.h>

namespace oms {

/// A decoded tile ready for blitting to canvas.
struct DecodedTile {
    int    tx, ty;      // tile coordinates (slippy map)
    int    zoom;
    bool   valid;       // true if decoded data is present
    uint8_t* pixels;    // RGB565 pixel data (256*256*2 = 128KB)
};

/// LRU cache slot for decoded tiles.
struct CacheSlot {
    int    tx = -1, ty = -1, zoom = -1;
    bool   valid = false;
    uint32_t lastUsed = 0;
    uint8_t* pixels = nullptr;   // allocated in PSRAM
};

/// TileRenderer: loads PNG tiles from SD card, decodes them,
/// and blits them onto the LVGL canvas.
///
/// Usage:
///   TileRenderer renderer;
///   renderer.init();
///   // In render loop:
///   renderer.renderFrame(canvas, centerLat, centerLng, zoom);
class TileRenderer {
public:
    TileRenderer() = default;

    /// Initialise SD card and allocate cache.
    /// Returns true if SD card is available.
    bool init();

    /// Render the current view onto the given LVGL canvas.
    /// Loads tiles from SD card, decodes PNG, blits to canvas buffer.
    /// Returns true if any new tile was loaded this frame.
    bool renderFrame(lv_obj_t* canvas, float centerLat, float centerLng, int zoom);

    /// Draw node markers on top of tiles.
    void drawNodes(lv_obj_t* canvas, const MapNode* nodes, uint16_t count,
                   float centerLat, float centerLng, int zoom,
                   int highlightIdx = -1);

    /// Whether SD card with /map/ directory is available.
    bool hasTiles() const { return _sdPresent; }

    /// Get the number of cache hits/misses for debugging.
    uint32_t cacheHits() const { return _cacheHits; }
    uint32_t cacheMisses() const { return _cacheMisses; }

private:
    /// Load and decode a tile from SD card.
    /// Returns pointer to decoded RGB565 data (from cache or newly decoded).
    /// Returns nullptr if tile not found or decode failed.
    uint8_t* getTile(int tx, int ty, int zoom);

    /// Find or allocate a cache slot for the given tile.
    /// Evicts least-recently-used if all slots are occupied.
    CacheSlot* findCacheSlot(int tx, int ty, int zoom);

    /// Decode a PNG file from SD card into RGB565 format.
    /// Output buffer must be at least 256*256*2 = 131072 bytes.
    /// Returns true on success.
    bool decodePNG(const char* path, uint8_t* rgb565Out);

    /// Calculate which tiles are visible for the current view.
    /// Fills outTiles with {tx, ty} pairs. Returns count.
    struct VisibleTile { int tx, ty; };
    int calcVisibleTiles(float centerLat, float centerLng, int zoom,
                         int canvasW, int canvasH,
                         VisibleTile* out, int maxOut);

    /// Blit a 256x256 RGB565 tile onto the canvas at pixel offset.
    void blitTile(uint8_t* canvasBuf, int canvasW, int canvasH,
                  uint8_t* tileData, int offsetX, int offsetY);

    /// Draw a placeholder grid line for missing tiles.
    void drawTilePlaceholder(uint8_t* canvasBuf, int canvasW, int canvasH,
                              int px, int py);

    /// Convert RGBA8888 pixel to RGB565.
    static inline uint16_t rgbaToRgb565(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        // Simple alpha blend with black background
        if (a == 0) return 0;  // transparent → black
        // RGB565: R=5bit, G=6bit, B=5bit
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    // SD card state
    bool _sdPresent = false;
    static constexpr uint8_t SD_CS_PIN = 11;   // T-Deck SD card CS

    // Tile cache (PSRAM-backed LRU)
    static constexpr int CACHE_SIZE = 9;  // 3x3 grid (most common view)
    CacheSlot _cache[CACHE_SIZE];

    // Temporary PNG decode buffer (allocated once in PSRAM)
    // lodepng decodes to RGBA8888 first, then we convert to RGB565
    uint8_t* _decodeBuf = nullptr;  // 256*256*4 = 262144 bytes (RGBA)
    static constexpr size_t DECODE_BUF_SIZE = 256 * 256 * 4;

    // Stats
    uint32_t _cacheHits = 0;
    uint32_t _cacheMisses = 0;
};

}  // namespace oms