// OpenMeshOS — PlatformCompat.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Platform abstraction implementation for ESP32 and nRF52.
// This file is compiled once and the correct implementation is selected
// at compile time via build flags (ARDUINO_ARCH_ESP32 / ARDUINO_ARCH_NRF52840).

#include "PlatformCompat.h"
#include "../utils/Log.h"

#if defined(ARDUINO_ARCH_ESP32)

// ── ESP32 implementation ──────────────────────────────────────────
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

namespace oms {
namespace platform {

void readMacAddress(uint8_t mac[6]) {
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

void fillRandom(uint8_t* buf, size_t len) {
    // ESP32 has a hardware RNG; esp_random() yields 4 bytes at a time
    size_t i = 0;
    while (i + 4 <= len) {
        uint32_t r = esp_random();
        memcpy(buf + i, &r, 4);
        i += 4;
    }
    if (i < len) {
        uint32_t r = esp_random();
        memcpy(buf + i, &r, len - i);
    }
}

uint32_t freeHeap() {
    return esp_get_free_heap_size();
}

uint32_t minFreeHeap() {
    return esp_get_minimum_free_heap_size();
}

uint32_t largestFreeBlock() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

}  // namespace platform
}  // namespace oms

#elif defined(ARDUINO_ARCH_NRF52840)

// ── nRF52 implementation ───────────────────────────────────────────
// nRF52840 uses FICR (Factory Information Configuration Registers) for
// the device address, and the RNG peripheral for random numbers.
//
// The Arduino nRF52 core provides analogRead() and other wrappers,
// but does not provide a MAC address or RNG wrapper. We access the
// Nordic registers directly via nrf.h (included with the nRF52 Arduino core).

#include <nrf.h>
#include <nrf_nvic.h>

// nRF52 heap size from linker symbols.
// The Adafruit nRF52 core uses FreeRTOS heap_3 (wraps libc malloc/free),
// which does not implement xPortGetFreeHeapSize(). We query the linker
// symbols __HeapBase and __HeapLimit, plus the current sbrk pointer,
// to compute free heap. This is less precise than ESP-IDF's tracking
// but sufficient for heap monitoring and watchdog purposes.
extern char __HeapBase;
extern char __HeapLimit;

// newlib's _sbrk increments the break; we declare it here.
extern "C" char* _sbrk(int incr);

namespace oms {
namespace platform {

// nRF52 minimum free heap tracking (ESP-IDF provides this, nRF52 does not)
static uint32_t s_minFreeHeap = UINT32_MAX;

void readMacAddress(uint8_t mac[6]) {
    // nRF52 FICR DEVICEID registers contain a 64-bit device address.
    // DEVICEID[0] = low 32 bits, DEVICEID[1] = high 32 bits.
    // We use the low 6 bytes as a MAC address.
    //
    // Note: This is the Nordic device ID, not an IEEE-assigned OUI MAC.
    // It is unique per chip, which is sufficient for our config key derivation.
    uint64_t deviceId = ((uint64_t)NRF_FICR->DEVICEID[1] << 32) | NRF_FICR->DEVICEID[0];
    memcpy(mac, &deviceId, 6);

    // Per IEEE 802, the second-least-significant bit of the first octet
    // indicates locally administered (1) vs universally administered (0).
    // Nordic device IDs have bit 1 set (locally administered), which is
    // actually what we want for a mesh node, so no modification needed.
}

void fillRandom(uint8_t* buf, size_t len) {
    // nRF52 has a hardware RNG that can be used in two modes:
    //   1. Polling mode (no interrupt) - slower but simpler
    //   2. Interrupt-driven - faster but requires setup
    //
    // For our use case (occasional key generation, advert jitter), polling
    // mode is sufficient. The RNG produces 1 byte per ~125us when running.
    //
    // We must ensure the RNG is enabled and wait for each value to be ready.

    // Enable RNG (if not already enabled)
    NRF_RNG->CONFIG = (NRF_RNG->CONFIG & ~RNG_CONFIG_DERCEN_Msk) | RNG_CONFIG_DERCEN_Enabled;
    NRF_RNG->TASKS_START = 1;

    for (size_t i = 0; i < len; i++) {
        // Wait for a new random value to be available
        while (NRF_RNG->EVENTS_VALRDY == 0) {
            // Busy-wait. On nRF52 this takes ~125us per byte.
        }
        buf[i] = NRF_RNG->VALUE;
        NRF_RNG->EVENTS_VALRDY = 0;  // clear the event
    }

    // Stop RNG to save power (will be restarted on next call)
    NRF_RNG->TASKS_STOP = 1;
}

uint32_t freeHeap() {
    // nRF52 FreeRTOS uses heap_3 (wraps libc malloc/free).
    // xPortGetFreeHeapSize() is not implemented in heap_3.
    // Use the current sbrk pointer vs the heap limit to estimate free heap.
    // This overestimates slightly (doesn't account for freed blocks below brk),
    // but is sufficient for heap monitoring and watchdog purposes.
    char* brk = _sbrk(0);
    ptrdiff_t used = brk - &__HeapBase;
    ptrdiff_t total = &__HeapLimit - &__HeapBase;
    if (used < 0 || used > total) return 0;
    return (uint32_t)(total - used);
}

uint32_t minFreeHeap() {
    // nRF52/FreeRTOS doesn't have a built-in minimum-heap watermark.
    // We track it manually: every time freeHeap() is called, update the low.
    uint32_t current = freeHeap();
    if (current < s_minFreeHeap) {
        s_minFreeHeap = current;
    }
    return s_minFreeHeap;
}

uint32_t largestFreeBlock() {
    // nRF52 doesn't have heap_caps_get_largest_free_block().
    // Return freeHeap() as a conservative approximation.
    return freeHeap();
}

}  // namespace platform
}  // namespace oms

#else

// ── Unknown platform fallback ──────────────────────────────────────
// If neither ESP32 nor nRF52, provide stubs that log warnings.
// This allows compilation on host for unit tests.

#include <cstring>
#include <cstdlib>

namespace oms {
namespace platform {

void readMacAddress(uint8_t mac[6]) {
    // Fallback: use a fixed MAC for testing
    static const uint8_t defaultMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    memcpy(mac, defaultMac, 6);
}

void fillRandom(uint8_t* buf, size_t len) {
    // Fallback: use stdlib rand() (not cryptographically secure)
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
}

uint32_t freeHeap() {
    // Host platform: return a large number (not meaningful)
    return UINT32_MAX / 2;
}

uint32_t minFreeHeap() {
    return UINT32_MAX / 2;
}

uint32_t largestFreeBlock() {
    return UINT32_MAX / 2;
}

}  // namespace platform
}  // namespace oms

#endif