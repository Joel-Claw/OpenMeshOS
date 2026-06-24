// OpenMeshOS — PlatformCompat.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Platform abstraction layer for ESP32 and nRF52 differences.
// Provides unified APIs for:
//   - MAC address reading (esp_read_mac vs nRF52 FICR)
//   - Random number generation (esp_random vs nRF52 RNG)
//   - Free heap size (esp_get_free_heap_size vs Nordic API)
//   - Minimum free heap (ESP-IDF specific, nRF52 tracks manually)
//
// This header is included by Config.cpp, MeshService.cpp, and HeapMonitor.cpp
// to avoid #ifdef soup in those files. Each platform-specific function is
// resolved at compile time via build flags.

#pragma once

#include <Arduino.h>

namespace oms {
namespace platform {

/// Read the 6-byte MAC address of the device.
/// On ESP32: uses esp_read_mac() from esp_mac.h
/// On nRF52: reads DEVICEID[0..1] registers from FICR
void readMacAddress(uint8_t mac[6]);

/// Fill a buffer with random bytes.
/// On ESP32: uses esp_random() from esp_system.h
/// On nRF52: uses the nRF52 RNG peripheral
void fillRandom(uint8_t* buf, size_t len);

/// Get current free heap size in bytes.
/// On ESP32: esp_get_free_heap_size()
/// On nRF52: Nordic provides this via a different API
uint32_t freeHeap();

/// Get minimum free heap ever seen (low watermark since boot).
/// On ESP32: esp_get_minimum_free_heap_size()
/// On nRF52: tracked manually (nRF52 doesn't have a built-in watermark)
uint32_t minFreeHeap();

/// Get the largest free block on the heap.
/// On ESP32: heap_caps_get_largest_free_block()
/// On nRF52: not directly available, returns freeHeap() as approximation
uint32_t largestFreeBlock();

}  // namespace platform
}  // namespace oms