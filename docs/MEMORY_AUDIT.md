# Memory Audit — OpenMeshOS

**Date**: 2026-06-16  
**Firmware version**: 0.1.0-alpha.1  
**Target**: ESP32-S3 (320KB DRAM, 8MB PSRAM)

## Summary

OpenMeshOS runs on an ESP32-S3 with 320KB internal DRAM and 8MB OPI PSRAM. The codebase is generally well-structured for embedded use, but there are several areas where heap allocation happens outside of `setup()` and some where `String` (Arduino) is used in ways that could fragment the heap over time.

**Overall risk level**: MEDIUM — the device should run for many hours without issues, but 72-hour continuous operation needs validation. The main concerns are BLE callback allocations and the Config JSON parser's `String` usage.

---

## 1. Allocation Inventory

### 1.1 Setup-Time Allocations (SAFE — no fragmentation)

These are allocated once during `setup()` or `MeshService::init()` and never freed:

| Allocation | Location | Size | Pool |
|---|---|---|---|
| Display buffer (partial) | `UIScreen.cpp:54` | 320×40×2 = 25,600 bytes | PSRAM |
| MeshService objects | `MeshService.cpp:116-186` | ~12 objects (board, clock, SPI, radio, etc.) | DRAM |
| MsgRingBuffer | `MsgRingBuffer.h:164` | 1000 × ~340 bytes ≈ 340KB | PSRAM (fallback: DRAM) |
| TileRenderer decode buffer | `TileRenderer.cpp:44` | DECODE_BUF_SIZE (defined elsewhere) | PSRAM (fallback: DRAM) |
| TileRenderer cache entries | `TileRenderer.cpp:58` | 9 tiles × TILE_PX² × 2 | PSRAM (fallback: DRAM) |
| ScreenMap canvas | `ScreenMap.cpp:106` | canvasW × canvasH × 2 | PSRAM |
| BLE server/characteristics | `BLECompanion.cpp:110-193` | ~10 heap objects | DRAM |
| Keyboard ring buffer | `Keyboard.h` | 16 entries stack-allocated | Stack |
| MessageBus ring | `MessageBus.h` | 32 × ~280 bytes ≈ 9KB | Stack/BSS |

### 1.2 Runtime Allocations (POTENTIAL FRAGMENTATION)

These allocations happen during normal operation:

| Allocation | Location | Risk | Mitigation |
|---|---|---|---|
| **Notification tone buffer** | `Notification.cpp:156,191` | LOW — allocated per beep, freed immediately. `heap_caps_malloc(MALLOC_CAP_8BIT)` ≈ 1-2KB. Short-lived. | None needed — allocates and frees in same function |
| **BLE `new` callbacks** | `BLECompanion.cpp:110,141,150,159,167,192,193` | LOW — allocated once during `init()`, never freed | None needed — one-time setup |
| **BLE2902 descriptors** | `BLECompanion.cpp:150,167,193` | LOW — allocated once during `init()` | None needed |
| **Config JSON read** | `Config.cpp:58` | **MEDIUM** — `String json = f.readString()` allocates on DRAM. Config file is small (~200 bytes) but this uses Arduino `String` which can fragment | **Recommend**: Replace with fixed-size `char` buffer + `f.readBytes()` |
| **Config JSON parse** | `Config.cpp:65,97,123` | **MEDIUM** — `String searchKey = String("\"") + key + "\":\""` creates temporary `String` objects each call. 3 lambdas in `init()`, each creating short-lived `String` objects | **Recommend**: Replace with `char[]` + `snprintf` |
| **CrashLog::getCrashInfo()** | `CrashLog.cpp:18-24` | LOW — returns `String`, but only called on boot/crash, not in normal loop | Acceptable for debug path |
| **OTA firmware update** | `BLECompanion.cpp:480+` | LOW — `Update.begin()` allocates flash partition, not heap | None needed |

---

## 2. String Usage Audit

Arduino `String` heap-allocates and can fragment DRAM over time. Findings:

### 2.1 Hot Path (loop) — CRITICAL

**None found.** The main `loop()` does not use `String`. Config save/load only happens at init or on explicit user action.

### 2.2 Warm Path (callbacks) — MODERATE

| Location | Usage | Risk |
|---|---|---|
| `Config::init()` | `String json = f.readString()` | MEDIUM — allocates DRAM for entire config file |
| `Config::init()` lambdas | `String searchKey = String("\"") + key + "\":\""` | MEDIUM — temporary String objects |
| `BLECompanion::handleConfigWrite()` | `std::string value = pChar->getValue()` | LOW — std::string from BLE stack, short-lived |
| `CrashLog::getCrashInfo()` | `String info = f.readString()` | LOW — only on crash boot |

### 2.3 Recommendations

1. **Config::init()**: Replace `String json = f.readString()` with a fixed-size `char` buffer:
   ```cpp
   char json[512];  // config file is ~200 bytes
   size_t len = f.readBytes(json, sizeof(json) - 1);
   json[len] = '\0';
   ```
   Then use `strstr()` / `strncmp()` for key lookup instead of `String::indexOf()`.

2. **Config parsing lambdas**: Replace `String searchKey` with `char searchKey[64]; snprintf(searchKey, sizeof(searchKey), "\"%s\":\"", key);` and use `strstr()`.

3. **CrashLog**: Leave as-is — only runs on boot after a crash, not in normal operation.

---

## 3. Stack Usage

### 3.1 FreeRTOS Task Stack

The ESP32-S3 Arduino framework runs `loop()` in the default FreeRTOS task with an 8KB stack. LVGL and display operations can be stack-heavy.

**Concern**: The `ScreenMap` canvas operations and tile rendering may push stack close to limits. No `xTaskCreate` calls found — everything runs in the main task.

**Recommendation**: Monitor stack high-water mark with `uxTaskGetStackHighWaterMark(NULL)` during development to confirm adequate margin.

### 3.2 Large Stack Objects

No large arrays found on the stack in `loop()`. The `Keyboard` ring buffer is 16 entries of small structs — safe.

---

## 4. PSRAM Usage

PSRAM is correctly used for large, long-lived buffers:

| Buffer | Estimated Size | Notes |
|---|---|---|
| Display partial buffer | 25.6 KB | `ps_malloc()` ✓ |
| MsgRingBuffer | ~340 KB | `heap_caps_malloc(MALLOC_CAP_SPIRAM)` ✓ |
| TileRenderer decode buffer | ~32 KB | `heap_caps_malloc(MALLOC_CAP_SPIRAM)` ✓ |
| TileRenderer LRU cache (9 tiles) | ~115 KB | `heap_caps_malloc(MALLOC_CAP_SPIRAM)` ✓ |
| ScreenMap canvas | ~153 KB (320×240×2) | `ps_malloc()` ✓ |
| **Total PSRAM** | **~666 KB** | Well within 8MB |

**Good**: All large buffers correctly use `ps_malloc()` or `heap_caps_malloc(MALLOC_CAP_SPIRAM)`, keeping DRAM free for LVGL and the stack.

**Fallback pattern**: `TileRenderer` and `MsgRingBuffer` both fall back to DRAM if PSRAM is unavailable. This is correct but should be logged prominently — running out of DRAM would be catastrophic.

---

## 5. Memory Leak Vectors

### 5.1 BLE Callback Objects

`BLECompanion::init()` allocates several `new` objects (callbacks, descriptors) that are never freed. This is **intentional** — BLE runs for the lifetime of the app and the ESP-IDF BLE stack manages these objects.

**Risk**: NONE — these live for the app lifetime.

### 5.2 MeshService Allocations

`MeshService::init()` allocates ~12 objects with `new` that are never freed. Same as BLE — intentional lifetime allocations.

**Risk**: NONE.

### 5.3 Notification Tone Buffer

`Notification::playToneCustom()` allocates `heap_caps_malloc(MALLOC_CAP_8BIT)` for the tone buffer and frees it in the same function. No leak possible.

**Risk**: NONE.

### 5.4 Config Save

`Config::save()` writes to SPIFFS using `File` which self-closes. No leak.

**Risk**: NONE.

---

## 6. Fragmentation Risk Assessment

### 6.1 Long-Running Operation (24h+)

The main concern for 72-hour continuous operation:

1. **Config String allocation** — `Config::init()` creates temporary `String` objects on DRAM. This only runs at boot and on explicit config reload, so it fragments once and the freed blocks are reused. **Low risk** for long-running operation.

2. **BLE `std::string`** — `BLECompanion::handleConfigWrite()` creates a `std::string` from `pChar->getValue()`. This allocates on DRAM but is short-lived. **Low risk** if config writes are infrequent.

3. **No heap allocation in main loop** — The `loop()` function and LVGL tick path do not allocate heap memory. This is the most important finding for long-term stability.

4. **SPIFFS wear** — Config writes are debounced (5s minimum). The `markDirty()` / `tick()` pattern ensures no write storms. **Low risk** for flash wear.

### 6.2 Worst-Case Scenario

If a user rapidly changes settings via BLE (e.g., a companion app sending config writes every 100ms), the `std::string` allocations in BLE callbacks could fragment DRAM. However, since config saves are debounced and BLE writes are small, this is unlikely to cause issues in practice.

**Recommendation**: Add a rate limiter to BLE config writes (min 1s between writes) as a defense-in-depth measure.

---

## 7. Dynamic Allocation After Setup — Violations

The project convention is "no dynamic allocation after `setup()`". Current violations:

| Location | Allocation | Severity |
|---|---|---|
| `Notification.cpp:156,191` | `heap_caps_malloc` for tone buffer | **LOW** — freed immediately, <2KB |
| `BLECompanion.cpp:handleConfigWrite()` | `std::string` from BLE stack | **LOW** — freed at end of callback |
| `BLECompanion.cpp:handleFirmwareWrite()` | `Update.write()` internal allocations | **LOW** — ESP-IDF manages, one-time OTA |

**No critical violations found.** All runtime allocations are short-lived and small.

---

## 8. Recommendations

### Must Fix (before 1.0)

1. **Replace `String` in Config::init()** — Use `char[]` + `f.readBytes()` + `strstr()`. This eliminates the only DRAM fragmentation source in normal operation.

2. **Add BLE config write rate limiter** — Minimum 1 second between config writes. Prevents rapid BLE config change attacks from fragmenting DRAM.

3. **Add stack high-water mark monitoring** — Log `uxTaskGetStackHighWaterMark(NULL)` every 60s in the first 24h of operation. Confirms 8KB stack is sufficient.

### Should Fix (before production)

4. **Replace `CrashLog::getCrashInfo()` String return** — Use a static `char[]` buffer instead of returning `String`. This only runs on crash boot, but eliminating `String` entirely makes the codebase cleaner.

5. **Add heap/PSRAM monitoring** — Log `ESP.getFreeHeap()` and `ESP.getFreePsram()` every 60s. Detect slow leaks before they crash.

6. **Consider pre-allocating the tone buffer** — Allocate the 1-2KB tone buffer once in `Notification::init()` and reuse it. Eliminates the only per-event heap allocation.

### Nice to Have

7. **Add 72-hour soak test** — Run the device for 72h continuously, logging heap/PSRAM every 60s. Verify no leaks or fragmentation growth.

8. **Add watchdog for heap** — If `ESP.getFreeHeap()` drops below 20KB, force a reboot. This is a safety net for any undiscovered leaks.

---

## 9. Memory Map (Estimated)

| Region | Size | Usage |
|---|---|---|
| Flash (app) | ~593 KB | 9.0% of 6.6MB app partition |
| DRAM (static/BSS) | ~50 KB | Global objects, stacks |
| DRAM (heap) | ~270 KB | LVGL, BLE, Arduino core, MeshCore |
| PSRAM (display) | ~25.6 KB | LVGL partial buffer |
| PSRAM (messages) | ~340 KB | MsgRingBuffer |
| PSRAM (tiles) | ~147 KB | TileRenderer decode + cache |
| PSRAM (canvas) | ~153 KB | ScreenMap canvas |
| PSRAM (total) | ~666 KB | ~8% of 8MB PSRAM |
| DRAM (free, estimated) | ~220 KB | Available for LVGL, BLE, runtime |
| PSRAM (free, estimated) | ~7.3 MB | Vast headroom |

---

## 10. Conclusions

The OpenMeshOS firmware is well-structured for embedded use. Key strengths:

- **All large buffers in PSRAM** — Display, message, and tile buffers correctly avoid DRAM
- **No heap allocation in main loop** — The hot path is clean
- **Config writes debounced** — SPIFFS wear is minimized
- **Ring buffers for messages** — No growing queues, bounded memory usage

The main areas for improvement are:

1. **Replace Arduino `String` in Config parser** — Single source of DRAM fragmentation
2. **BLE config rate limiter** — Defense against rapid config change attacks
3. **Stack monitoring** — Confirm 8KB stack is sufficient for LVGL + tile rendering
4. **Heap monitoring** — Detect slow leaks before they crash

After these fixes, the firmware should be stable for continuous operation well beyond 72 hours.