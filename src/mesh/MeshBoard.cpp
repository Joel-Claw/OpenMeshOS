// OpenMeshOS — MeshBoard.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore MainBoard implementation, platform-aware.
// Delegates battery reading, MCU temp, reboot, and reset reason
// to the IBoard hardware abstraction (BoardFactory selects the
// correct implementation at compile time).
//
// This replaces the old TDeckBoard which hardcoded T-Deck specific
// ADC pins and ESP32-only APIs. Now the same class works on all
// supported platforms (T-Deck, Heltec V3, RAK4631).

#include "MeshBoard.h"
#include "../hardware/IBoard.h"
#include "../hardware/PlatformCompat.h"
#include "../utils/Log.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <esp_system.h>
#endif

namespace oms {

MeshBoard::MeshBoard() {
    _startupReason = 0;  // normal boot (BD_STARTUP_NORMAL)
}

uint16_t MeshBoard::getBattMilliVolts() {
    // Delegate to IBoard, which knows the correct ADC pin and
    // voltage divider for the specific hardware.
    return theBoard()->batteryMilliVolts();
}

float MeshBoard::getMCUTemperature() {
    // Delegate to IBoard
    return theBoard()->mcuTemperature();
}

void MeshBoard::reboot() {
    OMS_LOG("Board", "Reboot requested");
    theBoard()->reboot();
}

uint8_t MeshBoard::getStartupReason() const {
    return _startupReason;
}

uint32_t MeshBoard::getResetReason() const {
    return theBoard()->resetReason();
}

bool MeshBoard::setAdcMultiplier(float multiplier) {
    if (multiplier <= 0.0f) return false;
    _adcMult = multiplier;
    return true;
}

uint32_t MeshBoard::getGpio() {
    return 0;
}

void MeshBoard::setGpio(uint32_t values) {
    (void)values;
}

const char* MeshBoard::getManufacturerName() const {
    // Delegate to IBoard's boardName()
    return theBoard()->boardName();
}

}  // namespace oms