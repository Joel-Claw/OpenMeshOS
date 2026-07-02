// OpenMeshOS — MeshBoard.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore MainBoard implementation, platform-aware.
// Replaces the old TDeckBoard with a generic implementation that
// gets hardware specifics (ADC pin, manufacturer name) from IBoard.
//
// This allows the same class to work on T-Deck, Heltec V3, and RAK4631
// without separate implementations for each platform.

#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <cstdint>

namespace oms {

class MeshBoard : public mesh::MainBoard {
public:
    MeshBoard();

    // ── MainBoard interface ────────────────────────────────────────
    uint16_t getBattMilliVolts() override;
    float getMCUTemperature() override;
    const char* getManufacturerName() const override;
    void reboot() override;
    uint8_t getStartupReason() const override;
    uint32_t getResetReason() const override;

    // ── ADC multiplier ────────────────────────────────────────────
    bool setAdcMultiplier(float multiplier) override;
    float getAdcMultiplier() const override { return _adcMult; }

    // ── GPIO ──────────────────────────────────────────────────────
    uint32_t getGpio() override;
    void setGpio(uint32_t values) override;

private:
    float _adcMult = 2.0f;  // Default voltage divider ratio
    uint8_t _startupReason = 0;
};

}  // namespace oms