// OpenMeshOS — Board.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Hardware abstraction for the T-Deck family.
// One Board singleton owns: display, keyboard, trackball, LoRa, GPS.
//
// Pin definitions cross-referenced with official LilyGo T-Deck utilities.h:
// https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h

#pragma once

#include <Arduino.h>
#ifdef OMS_HAS_BUILTIN_GPS
#include <TinyGPSPlus.h>
#endif
#include "Keyboard.h"
#include "Trackball.h"

namespace oms {

// ── Pin constants (T-Deck / T-Deck Plus) ──────────────────────────────
// These MUST match the official LilyGo T-Deck schematic.
// Do NOT change without cross-referencing the official repo.
namespace pins {
    constexpr gpio_num_t LORA_CS    = GPIO_NUM_9;
    constexpr gpio_num_t LORA_RST   = GPIO_NUM_17;
    constexpr gpio_num_t LORA_DIO1  = GPIO_NUM_45;
    constexpr gpio_num_t LORA_BUSY  = GPIO_NUM_13;
    constexpr gpio_num_t LORA_SCK   = GPIO_NUM_40;
    constexpr gpio_num_t LORA_MISO  = GPIO_NUM_38;
    constexpr gpio_num_t LORA_MOSI  = GPIO_NUM_41;

    // Named with DISP_ prefix to avoid macro conflict with TFT_eSPI
    // (TFT_eSPI defines TFT_CS, TFT_DC, etc. as preprocessor macros)
    constexpr gpio_num_t DISP_CS    = GPIO_NUM_12;
    constexpr gpio_num_t DISP_DC    = GPIO_NUM_11;
    constexpr gpio_num_t DISP_SCK   = GPIO_NUM_40;   // shared SPI
    constexpr gpio_num_t DISP_MOSI  = GPIO_NUM_41;   // shared SPI
    constexpr gpio_num_t DISP_BL    = GPIO_NUM_42;   // backlight

    constexpr gpio_num_t KB_SDA     = GPIO_NUM_18;
    constexpr gpio_num_t KB_SCL     = GPIO_NUM_8;

    // Trackball GPIO (all known T-Deck hardware)
    // Confirmed by Meshtastic variant.h and LilyGo utilities.h TBOX_G01-G04
    // GPIO 0 is also the BOOT button
    constexpr gpio_num_t TB_UP     = GPIO_NUM_3;   // TBOX_G01
    constexpr gpio_num_t TB_DOWN   = GPIO_NUM_15;  // TBOX_G03
    constexpr gpio_num_t TB_LEFT   = GPIO_NUM_1;   // TBOX_G04
    constexpr gpio_num_t TB_RIGHT  = GPIO_NUM_2;   // TBOX_G02
    constexpr gpio_num_t TB_PRESS  = GPIO_NUM_0;   // BOOT button

    // DEPRECATED: V1-specific pins were WRONG. These pins are NOT trackball:
    // GPIO 21 = ES7210 mic LRCK, GPIO 43 = GPS TX, GPIO 44 = GPS RX
    // Kept for compile compat only; do not use in new code.
    constexpr gpio_num_t TB_V1_UP    = GPIO_NUM_3;   // shared with TB_UP
    constexpr gpio_num_t TB_V1_DOWN  = GPIO_NUM_15;  // shared with TB_DOWN
    constexpr gpio_num_t TB_V1_LEFT  = GPIO_NUM_21;  // DEPRECATED — actually ES7210 LRCK
    constexpr gpio_num_t TB_V1_RIGHT = GPIO_NUM_43;  // DEPRECATED — actually GPS TX
    constexpr gpio_num_t TB_V1_PRESS = GPIO_NUM_44;  // DEPRECATED — actually GPS RX

    constexpr gpio_num_t TB_V2_UP    = GPIO_NUM_3;   // same as TB_UP
    constexpr gpio_num_t TB_V2_DOWN  = GPIO_NUM_15;  // same as TB_DOWN
    constexpr gpio_num_t TB_V2_LEFT  = GPIO_NUM_1;  // same as TB_LEFT
    constexpr gpio_num_t TB_V2_RIGHT = GPIO_NUM_2;  // same as TB_RIGHT
    constexpr gpio_num_t TB_V2_PRESS = GPIO_NUM_0;  // same as TB_PRESS

    // GPS (T-Deck Plus only)
    constexpr gpio_num_t GPS_TX     = GPIO_NUM_43;
    constexpr gpio_num_t GPS_RX     = GPIO_NUM_44;

    // SD card chip select
    constexpr gpio_num_t SD_CS       = GPIO_NUM_39;

    // Battery ADC
    constexpr gpio_num_t BAT_ADC     = GPIO_NUM_4;

    // Keyboard interrupt
    constexpr gpio_num_t KB_INT      = GPIO_NUM_46;

    // Touch interrupt
    constexpr gpio_num_t TOUCH_INT   = GPIO_NUM_16;

    // I2S audio (speaker)
    constexpr gpio_num_t I2S_BCK    = GPIO_NUM_7;
    constexpr gpio_num_t I2S_WS     = GPIO_NUM_5;
    constexpr gpio_num_t I2S_DOUT   = GPIO_NUM_6;

    // Board power enable (must be HIGH for peripherals)
    constexpr gpio_num_t POWER_EN    = GPIO_NUM_10;

    // Boot button
    constexpr gpio_num_t BOOT_PIN    = GPIO_NUM_0;
}

class Board {
public:
    static Board& instance();

    void init();
    void tick();

    // Trackball (delegates to Trackball driver)
    bool consumeTrackballPress() { return _trackball.consumePress(); }
    void consumeTrackballDelta(int16_t &dx, int16_t &dy) { _trackball.consumeDelta(dx, dy); }
    Trackball& trackball() { return _trackball; }

    // Keyboard
    Keyboard& keyboard() { return _keyboard; }
    bool hasKeyboard() const { return _keyboard.isPresent(); }

    // GPS
    bool hasGPSFix() const;
    float gpsLat() const;
    float gpsLng() const;
    float gpsAltitude() const;  // meters
    float gpsSpeed() const;     // km/h
    float gpsCourse() const;    // degrees
    int   gpsSatellites() const;
    uint32_t gpsAge() const;    // ms since last fix

    // Display backlight
    void setBacklight(bool on) { digitalWrite(pins::DISP_BL, on ? HIGH : LOW); }

    bool initialized() const { return _initialized; }

private:
    bool _initialized = false;

    // Trackball driver (auto-detects GPIO v1, v2, or I2C)
    Trackball _trackball;

    // BBQ10KB keyboard
    Keyboard _keyboard;

#ifdef OMS_HAS_BUILTIN_GPS
    HardwareSerial _gpsSerial{1};   // UART1 for GPS
    TinyGPSPlus    _gps;
#endif
};

}  // namespace oms