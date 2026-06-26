// OpenMeshOS — BoardHeltecV3.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// IBoard implementation for the Heltec WiFi LoRa 32 V3.
// ESP32-S3FN8 + SX1262 LoRa + 0.96" SSD1306 OLED (128x64, I2C).
//
// Pin definitions cross-referenced with:
//   - Heltec official GPIO usage guide:
//     https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/lora-32/wifi-lora-32-v3/
//   - MeshCore variant: variants/heltec_v3/platformio.ini
//   - Meshtastic firmware variant definitions
//
// NOTE: This board has NO physical keyboard, NO trackball, and NO touch screen.
// Input is via the single BOOT button (GPIO0) and optional external peripherals.
// The display is a small SSD1306 OLED (128x64), not a large TFT like the T-Deck.
// This board is primarily suited for headless/repeater operation or BLE companion mode.

#pragma once

#include "IBoard.h"
#include "IDisplay.h"
#include "IInput.h"
#include "Trackball.h"
#include "Keyboard.h"

namespace oms {

// ── Pin constants (Heltec WiFi LoRa 32 V3) ──────────────────────────
namespace heltec_v3 {
    // LoRa SX1262 (SPI bus)
    constexpr gpio_num_t LORA_CS    = GPIO_NUM_8;
    constexpr gpio_num_t LORA_RST   = GPIO_NUM_12;
    constexpr gpio_num_t LORA_DIO1  = GPIO_NUM_14;
    constexpr gpio_num_t LORA_BUSY  = GPIO_NUM_13;
    constexpr gpio_num_t LORA_SCK   = GPIO_NUM_9;
    constexpr gpio_num_t LORA_MISO  = GPIO_NUM_11;
    constexpr gpio_num_t LORA_MOSI  = GPIO_NUM_10;

    // OLED display (SSD1306, I2C)
    constexpr gpio_num_t OLED_SDA   = GPIO_NUM_17;
    constexpr gpio_num_t OLED_SCL   = GPIO_NUM_18;
    constexpr gpio_num_t OLED_RST   = GPIO_NUM_21;

    // Onboard LED (inverted: LOW = ON)
    constexpr gpio_num_t LED_PIN    = GPIO_NUM_35;

    // Vext power control (HIGH = on, powers external sensors)
    constexpr gpio_num_t VEXT_EN    = GPIO_NUM_36;

    // Boot/user button (also used for trackball press on T-Deck)
    constexpr gpio_num_t BOOT_BTN   = GPIO_NUM_0;

    // Battery ADC
    // Heltec V3 uses ADC1_CH0 on GPIO1 for battery voltage
    // Voltage divider: 2x (max ~3.3V ADC reads ~6.6V battery)
    constexpr gpio_num_t BAT_ADC    = GPIO_NUM_1;

    // I2C for user peripherals
    constexpr gpio_num_t USER_SDA   = GPIO_NUM_41;
    constexpr gpio_num_t USER_SCL   = GPIO_NUM_42;

    // GPS pins (optional external GPS on user I/O)
    constexpr gpio_num_t GPS_RX     = GPIO_NUM_47;
    constexpr gpio_num_t GPS_TX     = GPIO_NUM_48;
    constexpr gpio_num_t GPS_EN    = GPIO_NUM_26;

    // LoRa TX LED
    constexpr gpio_num_t LORA_TX_LED = GPIO_NUM_35;  // same as LED_PIN
}

/// IBoard implementation for Heltec WiFi LoRa 32 V3.
///
/// Key differences from T-Deck:
///   - Small OLED (128x64, I2C) instead of large TFT (320x240, SPI)
///   - No physical keyboard (BBQ10KB)
///   - No trackball
///   - No built-in GPS (optional external on user I2C/UART)
///   - No SD card slot
///   - No speaker/buzzer
///   - No touch screen
///   - SX1262 DIO2 used as RF switch (not available on T-Deck variant)
///
/// This board is best suited for:
///   - Repeater nodes (headless operation)
///   - BLE companion mode (controlled from phone)
///   - Sensor nodes (with environmental sensors on user I2C)
///   - Minimal chat nodes (OLED shows basic info, input via BLE)
class BoardHeltecV3 : public IBoard {
public:
    BoardHeltecV3() = default;

    // ── IBoard interface ──────────────────────────────────────────
    void init() override;
    void tick() override;

    const char* boardName() const override { return "Heltec WiFi LoRa 32 V3"; }

    BoardCaps capabilities() const override {
        return BoardCaps{
            .hasKeyboard    = false,  // No BBQ10KB
            .hasTrackball   = false,  // No trackball
            .hasGPS         = false,  // No built-in GPS (optional external)
            .hasSDCard      = false,  // No SD card slot
            .hasBLE         = true,   // ESP32-S3 always has BLE
            .hasSpeaker     = false,  // No speaker/buzzer
            .hasTouchScreen = false,  // No touch
            .hasBatteryADC  = true,    // ADC on GPIO1
            .hasLoRa        = true    // SX1262
        };
    }

    DisplayConfig displayConfig() const override {
        return DisplayConfig{
            .width    = 128,                       // SSD1306 128x64
            .height   = 64,
            .csPin    = 0,                         // I2C, no CS
            .dcPin    = 0,                         // I2C, no DC
            .rstPin   = (uint8_t)heltec_v3::OLED_RST,
            .blPin    = 0,                         // No backlight on OLED
            .sckPin   = (uint8_t)heltec_v3::OLED_SCL,
            .mosiPin  = (uint8_t)heltec_v3::OLED_SDA,
            .spiFreq  = 0                          // I2C, not SPI
        };
    }

    void setBacklight(bool on) override;
    void setBrightness(int level) override;

    // ── Display driver (IBoard override) ───────────────────────────────
    // Returns nullptr until DisplaySSD1306 implementation is added.
    IDisplay* display() override { return nullptr; }

    // ── Input driver (IBoard override) ────────────────────────────────
    // No keyboard/trackball on Heltec V3 — input via BLE companion app.
    IInput* input() override { return nullptr; }

    LoRaConfig loraConfig() const override {
        return LoRaConfig{
            .freqMHz  = 868.0f,   // EU868 default (configurable via Settings)
            .bwMHz    = 125.0f,
            .sf       = 9,
            .cr       = 5,
            .txPower  = 22,       // SX1262 max 22 dBm
            .csPin    = (uint8_t)heltec_v3::LORA_CS,
            .dio1Pin  = (uint8_t)heltec_v3::LORA_DIO1,
            .rstPin   = (uint8_t)heltec_v3::LORA_RST,
            .busyPin  = (uint8_t)heltec_v3::LORA_BUSY,
            .sckPin   = (uint8_t)heltec_v3::LORA_SCK,
            .misoPin  = (uint8_t)heltec_v3::LORA_MISO,
            .mosiPin  = (uint8_t)heltec_v3::LORA_MOSI
        };
    }

    // ── GPS ───────────────────────────────────────────────────────
    // No built-in GPS. Returns 0/false if no external GPS connected.
    bool hasGPSFix() const override { return false; }
    float gpsLat() const override { return 0.0f; }
    float gpsLng() const override { return 0.0f; }
    float gpsAltitude() const override { return 0.0f; }
    float gpsSpeed() const override { return 0.0f; }
    float gpsCourse() const override { return 0.0f; }
    int gpsSatellites() const override { return 0; }
    uint32_t gpsAge() const override { return UINT32_MAX; }

    // ── Battery ───────────────────────────────────────────────────
    uint16_t batteryMilliVolts() const override;
    int batteryPercent() const override;
    float mcuTemperature() const override;

    // ── Power ──────────────────────────────────────────────────────
    void reboot() override;
    void powerOff() override;
    uint32_t resetReason() const override;

    // ── Input ──────────────────────────────────────────────────────
    // No keyboard or trackball on Heltec V3
    bool hasKeyboard() const override { return false; }
    bool consumeTrackballPress() override { return false; }
    void consumeTrackballDelta(int16_t& dx, int16_t& dy) override { dx = 0; dy = 0; }

    // ── Input drivers ───────────────────────────────────────────────
    // Keyboard and trackball are not present. Return stub instances
    // so the interface contract is satisfied without null checks.
    Keyboard& keyboard() override { return _stubKeyboard; }
    Trackball& trackball() override { return _stubTrackball; }

    /// Legacy singleton accessor
    static BoardHeltecV3& instance();

    bool initialized() const { return _initialized; }

    // ── Heltec V3 specific ─────────────────────────────────────────
    /// Enable/disable Vext power (3.3V to user peripherals)
    void setVext(bool on);

    /// Enable/disable onboard LED (inverted: LOW = ON)
    void setLed(bool on);

private:
    bool _initialized = false;

    // Stub drivers (never initialized, always report not-present)
    Trackball _stubTrackball;
    Keyboard _stubKeyboard;

    // Battery ADC config
    float _adcMultiplier = 2.0f;  // Voltage divider ratio (matches T-Deck)
};

}  // namespace oms