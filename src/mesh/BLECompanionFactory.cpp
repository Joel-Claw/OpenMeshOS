// OpenMeshOS — BLECompanionFactory.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Factory implementation for IBLECompanion.
// Returns the platform-appropriate BLE companion implementation.

#include "IBLECompanion.h"
#include "BLECompanion.h"      // ESP32 impl
#include "BLECompanionNRF52.h" // nRF52 impl
#include "../utils/Log.h"

namespace oms {

IBLECompanion* createBLECompanion() {
#if defined(ARDUINO_ARCH_ESP32)
    return &BLECompanionESP32::instance();
#elif defined(ARDUINO_ARCH_NRF52840)
    return &BLECompanionNRF52::instance();
#else
    // Unknown platform: return nullptr, BLE is unavailable
    OMS_LOG("BLE", "WARNING: No BLE companion implementation for this platform");
    return nullptr;
#endif
}

IBLECompanion& theBLECompanion() {
    // On unknown platforms this will crash — but that's acceptable
    // since BLE is a core feature and the build should not reach
    // main.cpp without a BLE implementation.
    static IBLECompanion* s_instance = createBLECompanion();
    return *s_instance;
}

}  // namespace oms