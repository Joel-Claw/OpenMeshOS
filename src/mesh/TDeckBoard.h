// OpenMeshOS — TDeckBoard.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Implements MeshCore's MainBoard interface for the LilyGo T-Deck.
// Provides battery voltage, MCU temperature, reboot, and GPIO access.

#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <cstdint>

namespace oms {

class TDeckBoard : public mesh::MainBoard {
public:
    TDeckBoard();

    // ── MainBoard interface ────────────────────────────────────────
    uint16_t getBattMilliVolts() override;
    float getMCUTemperature() override;
    const char* getManufacturerName() const override { return "LilyGo T-Deck"; }
    void reboot() override;
    uint8_t getStartupReason() const override;
    uint32_t getResetReason() const override;

    // ── ADC multiplier ────────────────────────────────────────────
    // T-Deck voltage divider: battery → ADC
    bool setAdcMultiplier(float multiplier) override;
    float getAdcMultiplier() const override { return _adcMult; }

    // ── GPIO (keyboard trackball, etc.) ───────────────────────────
    uint32_t getGpio() override;
    void setGpio(uint32_t values) override;

private:
    float _adcMult = 2.0f;  // Default voltage divider ratio
    uint8_t _startupReason = 0;
};

}  // namespace oms