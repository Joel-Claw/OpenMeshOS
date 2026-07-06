# Memory Audit — OpenMeshOS

**Date**: 2026-07-06
**Auditor**: Automated (OpenClaw GLM-5.2)
**Scope**: Static analysis of all heap allocations in OpenMeshOS source code

## Summary

All heap allocations in OpenMeshOS are **intentional and safe** for the target use case
(embedded device, single firmware session, reboot = shutdown). No memory leaks were found
that would cause problems during normal operation.

## Allocation Inventory

### MeshService (src/mesh/MeshService.cpp)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `_meshBoard` | `new MeshBoard()` | Program lifetime | No (intentional) | None — lives until reboot |
| `_clock` | `new MeshClock()` | Program lifetime | No (intentional) | None |
| `s_loraSpi` | `new SPIClass(HSPI)` | Program lifetime | No (intentional) | None |
| `s_sx1262` module | `new Module(...)` | Program lifetime | No (intentional) | None |
| `s_sx1262` radio | `new CustomSX1262(...)` | Program lifetime | No (intentional) | None |
| `s_radio` | `new CustomSX1262Wrapper(...)` | Program lifetime | No (intentional) | None |
| `s_rng` | `new RadioNoiseListener(...)` | Program lifetime | No (intentional) | None |
| `s_millis` | `new ArduinoMillis()` | Program lifetime | No (intentional) | None |
| `s_pktMgr` | `new StaticPoolPacketManager(64)` | Program lifetime | No (intentional) | None — 64-packet static pool |
| `s_tables` | `new SimpleMeshTables()` | Program lifetime | No (intentional) | None |
| `_chat` | `new OpenMeshChat(...)` | Program lifetime | No (intentional) | None |

**Assessment**: All allocations are singletons that live for the entire firmware session.
On ESP32/nRF52, shutdown = reboot, so these are never freed. This is standard embedded
practice and not a leak.

### BLECompanion (src/mesh/BLECompanion.cpp)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `ServerCallbacks` | `new ServerCallbacks(*this)` | BLE server lifetime | By ESP32 BLE stack | None — ESP32 BLE owns these |
| `ConfigWriteCallback` | `new ConfigWriteCallback(*this)` | Characteristic lifetime | By ESP32 BLE stack | None |
| `MessageWriteCallback` | `new MessageWriteCallback(*this)` | Characteristic lifetime | By ESP32 BLE stack | None |
| `FirmwareWriteCallback` | `new FirmwareWriteCallback(*this)` | Characteristic lifetime | By ESP32 BLE stack | None |
| `BLE2902` descriptors | `new BLE2902()` | Characteristic lifetime | By ESP32 BLE stack | None |

**Assessment**: ESP32 BLE `setCallbacks()` and `addDescriptor()` take ownership. The BLE
stack frees these when the characteristic/server is destroyed. Standard ESP32 BLE pattern.

### TileRenderer (src/map/TileRenderer.cpp)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `_decodeBuf` | `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` | Renderer lifetime | In destructor | None — freed in `~TileRenderer()` |
| `_cache[i].pixels` | `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` | Cache entry lifetime | In destructor | None — freed in `~TileRenderer()` |

**Assessment**: Properly managed with PSRAM allocation and freed in destructor. Falls back
to regular malloc if PSRAM unavailable. No leak.

### MsgRingBuffer (src/mesh/MsgRingBuffer.h)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `_buf` | `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` or `calloc` | Buffer lifetime | In destructor | None — freed in `~MsgRingBuffer()` |

**Assessment**: Properly managed. PSRAM preferred, falls back to heap. Freed in destructor.

### UIScreen (src/ui/UIScreen.cpp)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `buf1` | `ps_malloc(...)` | Screen lifetime | On screen exit | None — freed when screen destroyed |

### ScreenMap (src/ui/ScreenMap.cpp)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `cbuf` | `ps_malloc(...)` | Canvas lifetime | On screen exit | None — freed when screen destroyed |

### Notification (src/hardware/Notification.cpp)
| Object | Allocation | Lifetime | Freed? | Risk |
|--------|-----------|----------|--------|------|
| `buf` | `heap_caps_malloc(..., MALLOC_CAP_8BIT)` | Function scope | `free()` before return | None — properly scoped |

**Assessment**: Audio buffer is allocated for buzzer playback and freed before function
returns. No leak.

## PSRAM Usage Summary

- **TileRenderer**: 256KB decode buffer + 9 × 64KB tile cache = ~832KB PSRAM
- **MsgRingBuffer**: 1000 × ~300B = ~300KB PSRAM
- **UIScreen**: 320 × 40 × 2B = 25.6KB PSRAM (drawing buffer)
- **ScreenMap**: Canvas buffer ~150KB PSRAM
- **Total PSRAM budget**: ~1.3MB of 8MB available (T-Deck PSRAM)

**Status**: Well within limits. No PSRAM exhaustion risk.

## Recommendations

1. **No action needed** — all allocations are correctly managed for embedded lifecycle
2. For v1.0: consider adding heap fragmentation monitoring in the status bar (alongside
   battery and RSSI) to catch any runtime issues during long-duration testing
3. The 24h/48h stress tests (Phase 5) will validate that heap fragmentation stays stable
   over time. The static analysis confirms no known leak sources

## Dynamic Allocation Patterns to Watch

- **String handling**: Arduino `String` usage should be avoided in hot paths (mesh loop).
  The OTA checksum verification was already refactored to eliminate String (commit b6b080b).
- **LVGL**: LVGL uses its own memory pool (`lv_malloc`). This is configured in `lv_conf.h`
  and is separate from the heap allocations audited above. LVGL's internal fragmentation
  should be monitored during stress testing.