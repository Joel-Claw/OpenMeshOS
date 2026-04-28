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
static constexpr gpio_num_t PIN_LORA_CS    = GPIO_NUM_9;
static constexpr gpio_num_t PIN_LORA_RST   = GPIO_NUM_12;
static constexpr gpio_num_t PIN_LORA_DIO1  = GPIO_NUM_14;
static constexpr gpio_num_t PIN_LORA_BUSY  = GPIO_NUM_13;
static constexpr gpio_num_t PIN_LORA_SCK   = GPIO_NUM_40;
static constexpr gpio_num_t PIN_LORA_MISO  = GPIO_NUM_41;
static constexpr gpio_num_t PIN_LORA_MOSI  = GPIO_NUM_42;

static constexpr gpio_num_t PIN_TFT_CS     = GPIO_NUM_4;
static constexpr gpio_num_t PIN_TFT_DC     = GPIO_NUM_5;
static constexpr gpio_num_t PIN_TFT_RST    = GPIO_NUM_6;
static constexpr gpio_num_t PIN_TFT_SCK    = GPIO_NUM_40;  // shared SPI
static constexpr gpio_num_t PIN_TFT_MOSI   = GPIO_NUM_42;  // shared SPI
static constexpr gpio_num_t PIN_BACKLIGHT   = GPIO_NUM_2;

static constexpr gpio_num_t PIN_KB_SDA     = GPIO_NUM_18;
static constexpr gpio_num_t PIN_KB_SCL     = GPIO_NUM_8;

// Trackball GPIO V1 (original boards)
static constexpr gpio_num_t PIN_TB_V1_UP    = GPIO_NUM_3;
static constexpr gpio_num_t PIN_TB_V1_DOWN  = GPIO_NUM_15;
static constexpr gpio_num_t PIN_TB_V1_LEFT  = GPIO_NUM_21;
static constexpr gpio_num_t PIN_TB_V1_RIGHT = GPIO_NUM_43;
static constexpr gpio_num_t PIN_TB_V1_PRESS = GPIO_NUM_44;

// Trackball GPIO V2 (newer boards)
static constexpr gpio_num_t PIN_TB_V2_UP    = GPIO_NUM_3;
static constexpr gpio_num_t PIN_TB_V2_DOWN  = GPIO_NUM_15;
static constexpr gpio_num_t PIN_TB_V2_LEFT  = GPIO_NUM_1;
static constexpr gpio_num_t PIN_TB_V2_RIGHT = GPIO_NUM_2;
static constexpr gpio_num_t PIN_TB_V2_PRESS = GPIO_NUM_0;

static constexpr gpio_num_t PIN_GPS_TX     = GPIO_NUM_17;
static constexpr gpio_num_t PIN_GPS_RX     = GPIO_NUM_16;

// ── Static instance ────────────────────────────────────────────────
static Board s_board;

Board& Board::instance() {
    return s_board;
}

// ── init ───────────────────────────────────────────────────────────
void Board::init() {
    OMS_LOG("Board", "Initialising T-Deck hardware");

    // SPIFFS for config / keys / messages
    if (!SPIFFS.begin(true)) {
        OMS_LOG("Board", "SPIFFS mount failed, formatting");
        SPIFFS.format();
        SPIFFS.begin(true);
    }

    // Backlight on
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, HIGH);

    // Display init is handled by LVGL / TFT_eSPI driver
    // (configured via build flags in platformio.ini)

    // Keyboard I2C
    Wire.begin(PIN_KB_SDA, PIN_KB_SCL);

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
    _gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
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

    // Battery ADC (T-Deck uses voltage divider on IO4 or IO1 depending on rev)
    // TODO: implement proper ADC read after pin validation
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
    return _gps.location.lat();
#else
    return 0.0f;
#endif
}

float Board::gpsLng() const {
#ifdef OMS_HAS_BUILTIN_GPS
    return _gps.location.lng();
#else
    return 0.0f;
#endif
}

}  // namespace oms