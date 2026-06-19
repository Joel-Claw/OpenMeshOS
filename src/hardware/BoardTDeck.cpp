// OpenMeshOS — BoardTDeck.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// IBoard implementation for the LilyGo T-Deck and T-Deck Plus.
// Hardware init: SPIFFS, I2C, keyboard, trackball, GPS, backlight.
// LoRa radio init is handled by MeshService (which reads loraConfig()).

#include "BoardTDeck.h"
#include "../utils/Config.h"
#include "../utils/Log.h"
#include <SPIFFS.h>
#include <Wire.h>
#include <esp_sleep.h>

namespace oms {

// ── Static instance (legacy singleton, will migrate) ──────────────
static BoardTDeck s_boardTDeck;

BoardTDeck& BoardTDeck::instance() {
    return s_boardTDeck;
}

// ── init ───────────────────────────────────────────────────────────
void BoardTDeck::init() {
    OMS_LOG("Board", "Initialising T-Deck hardware");

    // Power enable: GPIO 10 must be HIGH for LoRa, SD card, and audio
    pinMode(tdeck::POWER_EN, OUTPUT);
    digitalWrite(tdeck::POWER_EN, HIGH);

    // SPIFFS for config / keys / messages
    if (!SPIFFS.begin(true)) {
        OMS_LOG("Board", "SPIFFS mount failed, formatting");
        SPIFFS.format();
        SPIFFS.begin(true);
    }

    // Backlight on
    pinMode(tdeck::DISP_BL, OUTPUT);
    digitalWrite(tdeck::DISP_BL, HIGH);

    // Keyboard I2C
    Wire.begin(tdeck::KB_SDA, tdeck::KB_SCL);

    // BBQ10KB keyboard init
    if (_keyboard.begin(&Wire)) {
        OMS_LOG("Board", "Keyboard ready");
    } else {
        OMS_LOG("Board", "Keyboard not found, continuing without");
    }

    // Trackball auto-detection (probes I2C + GPIO variants)
    _trackball.begin(Wire);
    OMS_LOG("Board", "Trackball: %s", _trackball.typeName());

    // GPS serial (T-Deck Plus has built-in GPS)
#ifdef OMS_HAS_BUILTIN_GPS
    _gpsSerial.begin(9600, SERIAL_8N1, tdeck::GPS_RX, tdeck::GPS_TX);
#endif

    // Configure ADC for battery reading
    analogSetPinAttenuation(tdeck::BAT_ADC, ADC_2_5db);

    _initialized = true;
    OMS_LOG("Board", "Hardware ready");
}

// ── tick ───────────────────────────────────────────────────────────
void BoardTDeck::tick() {
    if (!_initialized) return;

    // Poll trackball (handles GPIO and I2C internally)
    _trackball.tick();

    // GPS serial read (if present)
#ifdef OMS_HAS_BUILTIN_GPS
    while (_gpsSerial.available()) {
        _gps.encode(_gpsSerial.read());
    }
#endif
}

// ── GPS ───────────────────────────────────────────────────────────
bool BoardTDeck::hasGPSFix() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return _gps.location.isValid();
#else
    return false;
#endif
}

float BoardTDeck::gpsLat() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).location.lat();
#else
    return 0.0f;
#endif
}

float BoardTDeck::gpsLng() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).location.lng();
#else
    return 0.0f;
#endif
}

float BoardTDeck::gpsAltitude() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).altitude.meters();
#else
    return 0.0f;
#endif
}

float BoardTDeck::gpsSpeed() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).speed.kmph();
#else
    return 0.0f;
#endif
}

float BoardTDeck::gpsCourse() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).course.deg();
#else
    return 0.0f;
#endif
}

int BoardTDeck::gpsSatellites() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).satellites.value();
#else
    return 0;
#endif
}

uint32_t BoardTDeck::gpsAge() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return _gps.location.age();
#else
    return UINT32_MAX;
#endif
}

// ── Battery ───────────────────────────────────────────────────────
uint16_t BoardTDeck::batteryMilliVolts() const {
    int raw = analogRead(tdeck::BAT_ADC);
    float adcVolt = (raw / 4095.0f) * 3.3f;
    float battVolt = adcVolt * _adcMultiplier;
    return static_cast<uint16_t>(battVolt * 1000.0f);
}

int BoardTDeck::batteryPercent() const {
    uint16_t mv = batteryMilliVolts();
    // LiPo approximate curve:
    // 4200mV = 100%, 3200mV = 0%
    if (mv >= 4200) return 100;
    if (mv <= 3200) return 0;
    return (int)((mv - 3200) * 100.0f / (4200 - 3200));
}

float BoardTDeck::mcuTemperature() const {
    return temperatureRead();
}

// ── Power ──────────────────────────────────────────────────────────
void BoardTDeck::reboot() {
    OMS_LOG("Board", "Reboot requested");
    ESP.restart();
}

void BoardTDeck::powerOff() {
    OMS_LOG("Board", "Deep sleep");
    // Configure wake sources: trackball press (GPIO 0) or LoRa IRQ (GPIO 45)
    esp_sleep_enable_ext0_wakeup(tdeck::TB_PRESS, 0);  // wake on trackball press
    esp_sleep_enable_ext0_wakeup(tdeck::LORA_DIO1, 1); // wake on LoRa interrupt
    esp_deep_sleep_start();
}

uint32_t BoardTDeck::resetReason() const {
    return esp_reset_reason();
}

}  // namespace oms