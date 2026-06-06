// OpenMeshOS — hardware abstraction for LilyGo T-Deck
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Board.cpp ties together the ESP32-S3 peripherals unique to the
// T-Deck family: ST7789 display, BBQ10KB keyboard, trackball, SX1262
// LoRa radio, and (on Plus) GPS.

#include "Board.h"
#include "../utils/Config.h"
#include "../utils/Log.h"
#include <SPIFFS.h>

#include <Wire.h>

namespace oms {

// ── Pin definitions (T-Deck / T-Deck Plus) ─────────────────────────
// Defined in Board.h as oms::pins::* constants.
// Cross-referenced with official LilyGo T-Deck utilities.h.
// See Board.h for full pin mapping and change history.

// ── Static instance ────────────────────────────────────────────────
static Board s_board;

Board& Board::instance() {
    return s_board;
}

// ── init ───────────────────────────────────────────────────────────
void Board::init() {
    OMS_LOG("Board", "Initialising T-Deck hardware");

    // Power enable: GPIO 10 must be HIGH for LoRa, SD card, and audio
    pinMode(pins::POWER_EN, OUTPUT);
    digitalWrite(pins::POWER_EN, HIGH);

    // SPIFFS for config / keys / messages
    if (!SPIFFS.begin(true)) {
        OMS_LOG("Board", "SPIFFS mount failed, formatting");
        SPIFFS.format();
        SPIFFS.begin(true);
    }

    // Backlight on
    pinMode(pins::DISP_BL, OUTPUT);
    digitalWrite(pins::DISP_BL, HIGH);

    // Display init is handled by LVGL / TFT_eSPI driver
    // (configured via build flags in platformio.ini)

    // Keyboard I2C
    Wire.begin(pins::KB_SDA, pins::KB_SCL);

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
    _gpsSerial.begin(9600, SERIAL_8N1, pins::GPS_RX, pins::GPS_TX);
#endif

    _initialized = true;
    OMS_LOG("Board", "Hardware ready");
}

// ── tick ───────────────────────────────────────────────────────────
void Board::tick() {
    if (!_initialized) return;

    // Poll trackball (handles GPIO and I2C internally)
    _trackball.tick();

    // GPS serial read (if present)
#ifdef OMS_HAS_BUILTIN_GPS
    while (_gpsSerial.available()) {
        _gps.encode(_gpsSerial.read());
    }
#endif

    // Battery ADC sampling is handled by TDeckBoard::getBattMilliVolts()
    // which reads the ADC on each call. No periodic caching needed here.
}

// ── GPS ───────────────────────────────────────────────────────────
bool Board::hasGPSFix() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return _gps.location.isValid();
#else
    return false;
#endif
}

float Board::gpsLat() const {
#ifdef OMS_HAS_BUILTIN_GPS
    // TinyGPSPlus lat()/lng() are non-const (clear updated flags), but read-only semantically
    return const_cast<TinyGPSPlus&>(_gps).location.lat();
#else
    return 0.0f;
#endif
}

float Board::gpsLng() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).location.lng();
#else
    return 0.0f;
#endif
}

float Board::gpsAltitude() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).altitude.meters();
#else
    return 0.0f;
#endif
}

float Board::gpsSpeed() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).speed.kmph();
#else
    return 0.0f;
#endif
}

float Board::gpsCourse() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return const_cast<TinyGPSPlus&>(_gps).course.deg();
#else
    return 0.0f;
#endif
}

int Board::gpsSatellites() const {
#ifdef OMS_HAS_BUILTIN_GPS
    // TinyGPSInteger::value() is non-const (clears updated flag), but read-only semantically
    return const_cast<TinyGPSPlus&>(_gps).satellites.value();
#else
    return 0;
#endif
}

uint32_t Board::gpsAge() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return _gps.location.age();
#else
    return UINT32_MAX;
#endif
}

}  // namespace oms