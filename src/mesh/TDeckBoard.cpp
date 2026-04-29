// OpenMeshOS — TDeckBoard.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore MainBoard implementation for LilyGo T-Deck.
// Battery ADC on GPIO1, internal temperature sensor, ESP32 reset reason.

#include "TDeckBoard.h"
#include "../utils/Log.h"

namespace oms {

// T-Deck battery ADC pin
static constexpr gpio_num_t BATT_ADC_PIN = GPIO_NUM_1;

TDeckBoard::TDeckBoard() {
    _startupReason = 0;  // normal boot

    // Configure ADC for battery reading
    analogSetPinAttenuation(BATT_ADC_PIN, ADC_2_5db);  // 0-3.3V range
}

uint16_t TDeckBoard::getBattMilliVolts() {
    // Read ADC and convert through voltage divider
    int raw = analogRead(BATT_ADC_PIN);
    // ESP32 ADC is 12-bit (0-4095), ref voltage ~3.3V
    // Battery voltage = ADC_voltage * divider_ratio
    float adcVolt = (raw / 4095.0f) * 3.3f;
    float battVolt = adcVolt * _adcMult;
    return static_cast<uint16_t>(battVolt * 1000.0f);
}

float TDeckBoard::getMCUTemperature() {
    // ESP32 internal temperature sensor (approximate)
    return temperatureRead();
}

void TDeckBoard::reboot() {
    OMS_LOG("Board", "Reboot requested");
    ESP.restart();
}

uint8_t TDeckBoard::getStartupReason() const {
    return _startupReason;
}

uint32_t TDeckBoard::getResetReason() const {
    return esp_reset_reason();
}

bool TDeckBoard::setAdcMultiplier(float multiplier) {
    if (multiplier <= 0.0f) return false;
    _adcMult = multiplier;
    return true;
}

uint32_t TDeckBoard::getGpio() {
    // No general-purpose GPIO access needed for T-Deck basic operation
    return 0;
}

void TDeckBoard::setGpio(uint32_t values) {
    // No general-purpose GPIO control needed for T-Deck basic operation
    (void)values;
}

}  // namespace oms