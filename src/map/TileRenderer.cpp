// OpenMeshOS — TileRenderer.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tile rendering pipeline: loads PNG tiles from SD card,
// decodes them, and blits onto the LVGL canvas.
//
// Uses a simple LRU cache in PSRAM for decoded tiles.
// PNG decoding uses lodepng (single-file, no deps).
// Progressive loading: center tile first, then expanding outward.

#include "TileRenderer.h"
#include "MapEngine.h"
#include "../utils/Log.h"

#include <SD.h>
#include <math.h>

// lodepng — single-file PNG decoder
// We include it directly; it's small enough for embedded use.
#include "lodepng/lodepng.h"

namespace oms {

// ── Tile size constant (OSM standard) ───────────────────────────────
static constexpr int TILE_PX = 256;

// ── init ────────────────────────────────────────────────────────────
bool TileRenderer::init() {
    OMS_LOG("Map", "TileRenderer: init");

    // Try SD card
    if (SD.begin(SD_CS_PIN)) {
        if (SD.exists("/map")) {
            _sdPresent = true;
            OMS_LOG("Map", "SD card with /map/ directory found");
        } else {
            OMS_LOG("Map", "SD card present but no /map/ directory");
        }
    } else {
        OMS_LOG("Map", "No SD card — tile rendering unavailable");
    }

    // Allocate decode buffer in PSRAM
    _decodeBuf = (uint8_t*)heap_caps_malloc(DECODE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!_decodeBuf) {
        _decodeBuf = (uint8_t*)malloc(DECODE_BUF_SIZE);
        if (_decodeBuf) {
            OMS_LOG("Map", "Decode buffer in DRAM (PSRAM alloc failed)");
        } else {
            OMS_LOG("Map", "FATAL: Cannot allocate PNG decode buffer");
            _sdPresent = false;
            return false;
        }
    }

    // Allocate cache slots in PSRAM
    for (int i = 0; i < CACHE_SIZE; i++) {
        _cache[i].pixels = (uint8_t*)heap_caps_malloc(TILE_PX * TILE_PX * 2, MALLOC_CAP_SPIRAM);
        if (!_cache[i].pixels) {
            _cache[i].pixels = (uint8_t*)malloc(TILE_PX * TILE_PX * 2);
        }
        _cache[i].valid = false;
        _cache[i].tx = -1;
        _cache[i].ty = -1;
        _cache[i].zoom = -1;
        _cache[i].lastUsed = 0;
    }

    OMS_LOG("Map", "Tile cache: %d slots, decode buffer: %uKB",
            CACHE_SIZE, (unsigned)(DECODE_BUF_SIZE / 1024));

    return _sdPresent;
}

// ── calcVisibleTiles ────────────────────────────────────────────────
int TileRenderer::calcVisibleTiles(float centerLat, float centerLng, int zoom,
                                    int canvasW, int canvasH,
                                    VisibleTile* out, int maxOut) {
    // Find the center tile
    int centerTx, centerTy;
    MapEngine::latLngToTile(centerLat, centerLng, zoom, centerTx, centerTy);

    // Calculate how many tiles we need to fill the canvas
    int tilesX = (canvasW / TILE_PX) + 2;  // +2 for partial tiles at edges
    int tilesY = (canvasH / TILE_PX) + 2;

    // Sub-tile pixel offset (where within the center tile are we centered?)
    float n = powf(2.0f, zoom);
    float fTileX = (centerLng + 180.0f) / 360.0f * n;
    float latRad = centerLat * (float)M_PI / 180.0f;
    float fTileY = (1.0f - logf(tanf(latRad) + 1.0f / cosf(latRad)) / (float)M_PI) / 2.0f * n;

    int subX = (int)((fTileX - floorf(fTileX)) * TILE_PX);
    int subY = (int)((fTileY - floorf(fTileY)) * TILE_PX);

    // Center the view: start from the tile that puts the center point
    // in the middle of the canvas
    int startTx = centerTx - (canvasW / 2 - subX) / TILE_PX - 1;
    int startTy = centerTy - (canvasH / 2 - subY) / TILE_PX - 1;

    int count = 0;
    // Generate all visible tiles, then sort by distance from center
    // for progressive loading (center tiles first)
    struct TileDist {
        int tx, ty;
        int dist;  // Manhattan distance from center tile
    };
    TileDist tiles[CACHE_SIZE + 4];
    int tileCount = 0;

    for (int dy = 0; dy < tilesY; dy++) {
        for (int dx = 0; dx < tilesX; dx++) {
            int ttx = startTx + dx;
            int tty = startTy + dy;
            int dist = abs(ttx - centerTx) + abs(tty - centerTy);
            tiles[tileCount].tx = ttx;
            tiles[tileCount].ty = tty;
            tiles[tileCount].dist = dist;
            tileCount++;
        }
    }

    // Sort by Manhattan distance from center (closest first)
    for (int i = 0; i < tileCount - 1; i++) {
        for (int j = i + 1; j < tileCount; j++) {
            if (tiles[j].dist < tiles[i].dist) {
                TileDist tmp = tiles[i];
                tiles[i] = tiles[j];
                tiles[j] = tmp;
            }
        }
    }

    // Copy sorted tiles to output
    for (int i = 0; i < tileCount && count < maxOut; i++) {
        out[count].tx = tiles[i].tx;
        out[count].ty = tiles[i].ty;
        count++;
    }

    return count;
}

// ── renderFrame ────────────────────────────────────────────────────
bool TileRenderer::renderFrame(lv_obj_t* canvas, float centerLat, float centerLng, int zoom) {
    if (!_sdPresent || !canvas) return false;

    // Get canvas buffer info
    lv_draw_buf_t* drawBuf = lv_canvas_get_draw_buf(canvas);
    if (!drawBuf) return false;

    int canvasW = drawBuf->header.w;
    int canvasH = drawBuf->header.h;
    uint8_t* canvasBuf = (uint8_t*)drawBuf->data;

    // Calculate visible tiles
    VisibleTile visible[CACHE_SIZE + 4];  // a few extra for safety
    int numVisible = calcVisibleTiles(centerLat, centerLng, zoom,
                                       canvasW, canvasH,
                                       visible, CACHE_SIZE + 4);

    // Calculate sub-tile pixel offset for panning
    float n = powf(2.0f, zoom);
    float fTileX = (centerLng + 180.0f) / 360.0f * n;
    float latRad = centerLat * (float)M_PI / 180.0f;
    float fTileY = (1.0f - logf(tanf(latRad) + 1.0f / cosf(latRad)) / (float)M_PI) / 2.0f * n;

    int subX = (int)((fTileX - floorf(fTileX)) * TILE_PX);
    int subY = (int)((fTileY - floorf(fTileY)) * TILE_PX);

    // Center tile
    int centerTx, centerTy;
    MapEngine::latLngToTile(centerLat, centerLng, zoom, centerTx, centerTy);

    // Offset from center of canvas to top-left of center tile
    int offsetX = canvasW / 2 - subX - TILE_PX / 2;
    int offsetY = canvasH / 2 - subY - TILE_PX / 2;

    bool newTileLoaded = false;

    // Clear canvas to background colour (dark map background)
    // RGB565: dark blue-grey matching theme
    uint16_t bg565 = ((13 >> 3) << 11) | ((17 >> 2) << 5) | (23 >> 3);
    uint16_t* buf16 = (uint16_t*)canvasBuf;
    for (int i = 0; i < canvasW * canvasH; i++) {
        buf16[i] = bg565;
    }

    // Load and blit each visible tile
    for (int i = 0; i < numVisible; i++) {
        int tx = visible[i].tx;
        int ty = visible[i].ty;

        // Get decoded tile data (from cache or SD card)
        uint8_t* tileData = getTile(tx, ty, zoom);
        if (!tileData) {
            // Tile not available — draw a subtle grid line to show tile boundary
            // Calculate where this tile would be on canvas
            int px = offsetX + (tx - centerTx) * TILE_PX;
            int py = offsetY + (ty - centerTy) * TILE_PX;
            // Draw a thin border rectangle for missing tiles
            drawTilePlaceholder(canvasBuf, canvasW, canvasH, px, py);
            continue;
        }

        // Calculate pixel position on canvas
        int px = offsetX + (tx - centerTx) * TILE_PX;
        int py = offsetY + (ty - centerTy) * TILE_PX;

        // Blit tile onto canvas
        blitTile(canvasBuf, canvasW, canvasH, tileData, px, py);
        newTileLoaded = true;
    }

    return newTileLoaded;
}

// ── drawNodes ──────────────────────────────────────────────────────
void TileRenderer::drawNodes(lv_obj_t* canvas, const MapNode* nodes, uint16_t count,
                              float centerLat, float centerLng, int zoom) {
    if (!canvas || count == 0) return;

    lv_draw_buf_t* drawBuf = lv_canvas_get_draw_buf(canvas);
    if (!drawBuf) return;

    int canvasW = drawBuf->header.w;
    int canvasH = drawBuf->header.h;

    float n = powf(2.0f, zoom);
    float fCenterX = (centerLng + 180.0f) / 360.0f * n;
    float latRad = centerLat * (float)M_PI / 180.0f;
    float fCenterY = (1.0f - logf(tanf(latRad) + 1.0f / cosf(latRad)) / (float)M_PI) / 2.0f * n;

    for (uint16_t i = 0; i < count; i++) {
        // Convert node lat/lng to pixel position relative to canvas center
        float fNodeX = (nodes[i].lng + 180.0f) / 360.0f * n;
        float nodeLatRad = nodes[i].lat * (float)M_PI / 180.0f;
        float fNodeY = (1.0f - logf(tanf(nodeLatRad) + 1.0f / cosf(nodeLatRad)) / (float)M_PI) / 2.0f * n;

        int px = canvasW / 2 + (int)((fNodeX - fCenterX) * TILE_PX);
        int py = canvasH / 2 + (int)((fNodeY - fCenterY) * TILE_PX);

        // Draw node marker on canvas (8x8 filled circle in accent colour)
        uint16_t marker565 = ((88 >> 3) << 11) | ((166 >> 2) << 5) | (255 >> 3);  // accent blue

        uint16_t* buf16 = (uint16_t*)(drawBuf->data);
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                // Circle check
                if (dx * dx + dy * dy > 16) continue;
                int sx = px + dx;
                int sy = py + dy;
                if (sx >= 0 && sx < canvasW && sy >= 0 && sy < canvasH) {
                    buf16[sy * canvasW + sx] = marker565;
                }
            }
        }

        // Draw white border around marker
        uint16_t white565 = 0xFFFF;
        for (int a = 0; a < 16; a++) {
            // Simple circle perimeter approximation
            int bx = px + (int)(4.0f * cosf(a * (float)M_PI / 8.0f));
            int by = py + (int)(4.0f * sinf(a * (float)M_PI / 8.0f));
            if (bx >= 0 && bx < canvasW && by >= 0 && by < canvasH) {
                buf16[by * canvasW + bx] = white565;
            }
        }
    }
}

// ── getTile ────────────────────────────────────────────────────────
uint8_t* TileRenderer::getTile(int tx, int ty, int zoom) {
    // Check cache first
    CacheSlot* slot = findCacheSlot(tx, ty, zoom);
    if (slot && slot->valid && slot->tx == tx && slot->ty == ty && slot->zoom == zoom) {
        slot->lastUsed = millis();
        _cacheHits++;
        return slot->pixels;
    }

    // Cache miss: load from SD card
    _cacheMisses++;

    if (!_sdPresent) return nullptr;

    // Check if tile file exists
    char path[48];
    snprintf(path, sizeof(path), "/map/%d/%d/%d.png", zoom, tx, ty);

    if (!SD.exists(path)) {
        return nullptr;  // no tile for this area
    }

    // Decode the PNG into the cache slot
    if (!slot) {
        OMS_LOG("Map", "No cache slot available");
        return nullptr;
    }

    if (!decodePNG(path, slot->pixels)) {
        OMS_LOG("Map", "PNG decode failed: %s", path);
        return nullptr;
    }

    slot->tx = tx;
    slot->ty = ty;
    slot->zoom = zoom;
    slot->valid = true;
    slot->lastUsed = millis();

    OMS_LOG("Map", "Tile loaded: z%d/%d/%d (cache %u/%u)",
            zoom, tx, ty, (unsigned)_cacheHits, (unsigned)(_cacheHits + _cacheMisses));

    return slot->pixels;
}

// ── findCacheSlot ──────────────────────────────────────────────────
CacheSlot* TileRenderer::findCacheSlot(int tx, int ty, int zoom) {
    // First check for exact match
    uint32_t oldest = UINT32_MAX;
    CacheSlot* lru = nullptr;

    for (int i = 0; i < CACHE_SIZE; i++) {
        if (_cache[i].valid && _cache[i].tx == tx && _cache[i].ty == ty && _cache[i].zoom == zoom) {
            return &_cache[i];  // cache hit
        }
        // Track LRU for eviction
        if (!_cache[i].valid) {
            // Empty slot — use it immediately
            return &_cache[i];
        }
        if (_cache[i].lastUsed < oldest) {
            oldest = _cache[i].lastUsed;
            lru = &_cache[i];
        }
    }

    // No exact match and no empty slot — evict LRU
    return lru;
}

// ── decodePNG ──────────────────────────────────────────────────────
bool TileRenderer::decodePNG(const char* path, uint8_t* rgb565Out) {
    // Load entire PNG file from SD card into memory
    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    size_t fileSize = f.size();
    if (fileSize == 0 || fileSize > 512 * 1024) {  // max 512KB per tile PNG
        f.close();
        return false;
    }

    // Read file into decode buffer
    uint8_t* pngData = _decodeBuf;
    size_t bytesRead = f.read(pngData, fileSize);
    f.close();

    if (bytesRead != fileSize) {
        OMS_LOG("Map", "SD read error: got %u, expected %u",
                (unsigned)bytesRead, (unsigned)fileSize);
        return false;
    }

    // Decode PNG to RGBA8888
    unsigned int width = 0, height = 0;
    uint8_t* rgbaData = nullptr;
    unsigned int error = lodepng_decode32(&rgbaData, &width, &height, pngData, fileSize);

    if (error != 0 || rgbaData == nullptr) {
        OMS_LOG("Map", "lodepng error %u: %s", error, lodepng_error_text(error));
        return false;
    }

    // Check dimensions (OSM tiles are 256x256)
    if (width != TILE_PX || height != TILE_PX) {
        OMS_LOG("Map", "Unexpected tile size: %ux%u (expected 256x256)", width, height);
        free(rgbaData);
        return false;
    }

    // Convert RGBA8888 to RGB565
    for (unsigned int i = 0; i < width * height; i++) {
        uint8_t r = rgbaData[i * 4 + 0];
        uint8_t g = rgbaData[i * 4 + 1];
        uint8_t b = rgbaData[i * 4 + 2];
        uint8_t a = rgbaData[i * 4 + 3];

        uint16_t rgb565 = rgbaToRgb565(r, g, b, a);
        rgb565Out[i * 2 + 0] = rgb565 & 0xFF;       // low byte
        rgb565Out[i * 2 + 1] = (rgb565 >> 8) & 0xFF; // high byte
    }

    free(rgbaData);  // lodepng allocates via malloc
    return true;
}

// ── blitTile ───────────────────────────────────────────────────────
void TileRenderer::blitTile(uint8_t* canvasBuf, int canvasW, int canvasH,
                             uint8_t* tileData, int offsetX, int offsetY) {
    uint16_t* canvas = (uint16_t*)canvasBuf;
    uint16_t* tile = (uint16_t*)tileData;

    // Calculate visible region (clip to canvas bounds)
    int srcX0 = (offsetX < 0) ? -offsetX : 0;
    int srcY0 = (offsetY < 0) ? -offsetY : 0;
    int srcX1 = (offsetX + TILE_PX > canvasW) ? canvasW - offsetX : TILE_PX;
    int srcY1 = (offsetY + TILE_PX > canvasH) ? canvasH - offsetY : TILE_PX;

    for (int sy = srcY0; sy < srcY1; sy++) {
        int dstY = offsetY + sy;
        if (dstY < 0 || dstY >= canvasH) continue;

        for (int sx = srcX0; sx < srcX1; sx++) {
            int dstX = offsetX + sx;
            if (dstX < 0 || dstX >= canvasW) continue;

            uint16_t pixel = tile[sy * TILE_PX + sx];
            // Skip pure-black pixels (transparent placeholder)
            if (pixel != 0) {
                canvas[dstY * canvasW + dstX] = pixel;
            }
        }
    }
}

// ── drawTilePlaceholder ────────────────────────────────────────────
void TileRenderer::drawTilePlaceholder(uint8_t* canvasBuf, int canvasW, int canvasH,
                                         int px, int py) {
    uint16_t* canvas = (uint16_t*)canvasBuf;
    uint16_t grid565 = ((30 >> 3) << 11) | ((36 >> 2) << 5) | (42 >> 3);  // subtle dark line

    // Draw thin border lines at tile edges
    for (int i = 0; i < TILE_PX; i++) {
        // Top edge
        int x = px + i, y = py;
        if (x >= 0 && x < canvasW && y >= 0 && y < canvasH)
            canvas[y * canvasW + x] = grid565;
        // Left edge
        x = px; y = py + i;
        if (x >= 0 && x < canvasW && y >= 0 && y < canvasH)
            canvas[y * canvasW + x] = grid565;
    }
}

}  // namespace oms