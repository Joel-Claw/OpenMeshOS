// OpenMeshOS — BoardRAK4631.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// IBoard implementation for the RAK Wireless WisBlock RAK4631.
// nRF52840 MCU + SX1262 LoRa + optional SSD1306 OLED (128x64, I2C).
//
// Pin definitions cross-referenced with:
//   - MeshCore variant: variants/rak4631/variant.h
//   - RAK4631 datasheet (RAK4630 stamp module on RAK5005-O baseboard)
//   - Meshtastic RAK4631 platform definitions
//
// NOTE: This board has NO physical keyboard, NO trackball, and NO touch screen.
// Input is via optional external sensors or BLE companion app.
// The display is an optional SSD1306 OLED (128x64, I2C) on WB_I2C1.
// This board is primarily suited for:
//   - Repeater nodes (headless operation with long battery life)
//   - BLE companion mode (controlled from phone)
//   - Sensor nodes (environmental sensors on WisBlock slots)
//   - Solar-powered remote nodes (excellent low-power characteristics)
//
// Key differences from ESP32 boards:
//   - nRF52840 MCU (ARM Cortex-M4F, 64MHz) vs ESP32-S3 (Xtensa dual-core, 240MHz)
//   - Built-in BLE 5.0 (Nordic SoftDevice) vs ESP32 BLE stack
//   - No PSRAM (nRF52 has 256KB RAM, sufficient for mesh without PSRAM)
//   - No WiFi (nRF52 does not have WiFi — BLE only)
//   - Flash is internal (1MB) vs external SPI flash on ESP32
//   - Different low-power modes (SYSTEMOFF vs deep sleep)
//   - Different reset/watchdog APIs (Nordic WDT vs ESP WDT)
//   - Uses Arduino nRF52 core, not ESP32 Arduino core

#pragma once

#include "IBoard.h"
#include "IDisplay.h"
#include "IInput.h"
#include "Trackball.h"
#include "Keyboard.h"
#include "DisplaySSD1306.h"

namespace oms {

// ── Pin constants (RAK4631 WisBlock) ─────────────────────────────────
namespace rak4631 {
    // LoRa SX1262 (dedicated SPI bus on RAK4630 stamp module)
    constexpr uint8_t LORA_CS    = 42;   // P_LORA_NSS
    constexpr uint8_t LORA_RST   = 38;   // P_LORA_RESET
    constexpr uint8_t LORA_DIO1  = 47;   // P_LORA_DIO_1
    constexpr uint8_t LORA_BUSY  = 46;   // P_LORA_BUSY
    constexpr uint8_t LORA_SCK   = 43;   // P_LORA_SCLK
    constexpr uint8_t LORA_MISO  = 45;   // P_LORA_MISO
    constexpr uint8_t LORA_MOSI  = 44;   // P_LORA_MOSI
    constexpr uint8_t LORA_POWER_EN = 37; // SX126X_POWER_EN

    // OLED display (SSD1306, I2C — optional, on WB_I2C1)
    constexpr uint8_t OLED_SDA   = 13;   // WB_I2C1_SDA
    constexpr uint8_t OLED_SCL   = 14;   // WB_I2C1_SCL
    constexpr uint8_t OLED_RST   = 0xFF; // No reset pin (use -1 / 0xFF)

    // LEDs
    constexpr uint8_t LED_BLUE   = 35;   // PIN_LED1 (LED_BUILTIN)
    constexpr uint8_t LED_GREEN  = 36;   // PIN_LED2 (LED_CONN)

    // User buttons (RAK4631 has no built-in buttons; use WisBlock IO)
    constexpr uint8_t USER_BTN   = 33;   // WB_SW1 (IO_SLOT)

    // Battery ADC (nRF52 SAADC on AIN3 = P0.05)
    constexpr uint8_t BAT_ADC    = 5;    // PIN_A0 / WB_A0

    // WisBlock IO slots (for external sensors/peripherals)
    constexpr uint8_t WB_IO1     = 17;   // SLOT_A / SLOT_B
    constexpr uint8_t WB_IO2     = 34;   // SLOT_A / SLOT_B
    constexpr uint8_t WB_IO3     = 21;   // SLOT_C
    constexpr uint8_t WB_IO4     = 4;    // SLOT_C
    constexpr uint8_t WB_IO5     = 9;    // SLOT_D
    constexpr uint8_t WB_IO6     = 10;   // SLOT_D

    // I2C buses
    constexpr uint8_t I2C1_SDA   = 13;   // WB_I2C1_SDA (sensor slot)
    constexpr uint8_t I2C1_SCL   = 14;   // WB_I2C1_SCL (sensor slot)
    constexpr uint8_t I2C2_SDA   = 24;   // WB_I2C2_SDA (IO slot)
    constexpr uint8_t I2C2_SCL   = 25;   // WB_I2C2_SCL (IO slot)

    // SPI bus (IO slot)
    constexpr uint8_t SPI_CS     = 26;   // WB_SPI_CS
    constexpr uint8_t SPI_SCK    = 3;    // WB_SPI_CLK
    constexpr uint8_t SPI_MISO   = 29;   // WB_SPI_MISO
    constexpr uint8_t SPI_MOSI   = 30;   // WB_SPI_MOSI

    // GPS (optional external u-blox GPS on UART1)
    constexpr uint8_t GPS_RX     = 15;   // PIN_SERIAL1_RX
    constexpr uint8_t GPS_TX     = 16;   // PIN_SERIAL1_TX
    constexpr uint8_t GPS_1PPS   = 17;   // PIN_GPS_1PPS (shared with WB_IO1)

    // SX1262 features
    constexpr bool    DIO2_AS_RF_SWITCH = true;
    constexpr float   DIO3_TCXO_VOLTAGE = 1.8f;
    constexpr uint8_t CURRENT_LIMIT     = 140;  // mA
    constexpr bool    RX_BOOSTED_GAIN   = true;
}

/// IBoard implementation for RAK WisBlock RAK4631.
///
/// Key differences from T-Deck:
///   - nRF52840 MCU (not ESP32-S3)
///   - Optional SSD1306 OLED (128x64, I2C) instead of large TFT (320x240, SPI)
///   - No physical keyboard (BBQ10KB)
///   - No trackball
///   - No built-in GPS (optional external on UART1)
///   - No SD card slot (uses internal flash / SPIFFS equivalent)
///   - No speaker/buzzer
///   - No touch screen
///   - Excellent low-power characteristics (nRF52 designed for BLE/IoT)
///   - SX1262 with DIO2 RF switch and DIO3 TCXO (unlike T-Deck)
///
/// This board is best suited for:
///   - Repeater nodes (long battery life, headless operation)
///   - BLE companion mode (controlled from phone)
///   - Sensor nodes (WisBlock sensor modules in IO slots)
///   - Solar-powered remote nodes (low power consumption)
///   - Minimal chat nodes (OLED shows basic info, input via BLE)
class BoardRAK4631 : public IBoard {
public:
    BoardRAK4631() = default;

    // ── IBoard interface ──────────────────────────────────────────
    void init() override;
    void tick() override;

    const char* boardName() const override { return "RAK WisBlock RAK4631"; }

    BoardCaps capabilities() const override {
        return BoardCaps{
            .hasKeyboard    = false,  // No BBQ10KB
            .hasTrackball   = false,  // No trackball
            .hasGPS         = false,  // No built-in GPS (optional external)
            .hasSDCard      = false,  // No SD card slot
            .hasBLE         = true,   // nRF52840 has BLE 5.0
            .hasSpeaker     = false,  // No speaker/buzzer
            .hasTouchScreen = false,  // No touch
            .hasBatteryADC  = true,  // SAADC on AIN3 (P0.05)
            .hasLoRa        = true   // SX1262
        };
    }

    DisplayConfig displayConfig() const override {
        return DisplayConfig{
            .width    = 128,                        // SSD1306 128x64
            .height   = 64,
            .csPin    = 0,                           // I2C, no CS
            .dcPin    = 0,                           // I2C, no DC
            .rstPin   = -1,                           // No reset pin
            .blPin    = 0,                           // No backlight on OLED
            .sckPin   = (uint8_t)rak4631::I2C1_SCL,  // I2C SCL
            .mosiPin  = (uint8_t)rak4631::I2C1_SDA,  // I2C SDA
            .spiFreq  = 0                             // I2C, not SPI
        };
    }

    void setBacklight(bool on) override;
    void setBrightness(int level) override;

    // ── Display driver (IBoard override) ───────────────────────────────
    // Returns the SSD1306 OLED display driver.
    IDisplay* display() override { return &_display; }

    // ── Input driver (IBoard override) ────────────────────────────────
    // No keyboard/trackball on RAK4631 — input via BLE companion app.
    IInput* input() override { return nullptr; }

    LoRaConfig loraConfig() const override {
        return LoRaConfig{
            .freqMHz  = 868.0f,   // EU868 default (configurable via Settings)
            .bwMHz    = 125.0f,
            .sf       = 9,
            .cr       = 5,
            .txPower  = 22,       // SX1262 max 22 dBm
            .csPin    = rak4631::LORA_CS,
            .dio1Pin  = rak4631::LORA_DIO1,
            .rstPin   = rak4631::LORA_RST,
            .busyPin  = rak4631::LORA_BUSY,
            .sckPin   = rak4631::LORA_SCK,
            .misoPin  = rak4631::LORA_MISO,
            .mosiPin  = rak4631::LORA_MOSI
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
    // No keyboard or trackball on RAK4631
    bool hasKeyboard() const override { return false; }
    bool consumeTrackballPress() override { return false; }
    void consumeTrackballDelta(int16_t& dx, int16_t& dy) override { dx = 0; dy = 0; }

    // ── Input drivers ───────────────────────────────────────────────
    // Keyboard and trackball are not present. Return stub instances
    // so the interface contract is satisfied without null checks.
    Keyboard& keyboard() override { return _stubKeyboard; }
    Trackball& trackball() override { return _stubTrackball; }

    /// Legacy singleton accessor
    static BoardRAK4631& instance();

    bool initialized() const { return _initialized; }

    // ── RAK4631 specific ───────────────────────────────────────────
    /// Enable/disable LoRa module power (SX126X_POWER_EN pin)
    void setLoRaPower(bool on);

    /// Enable/disable onboard LED (blue, LED_BUILTIN)
    void setLed(bool on);

    /// Enable/disable green LED (LED_CONN)
    void setGreenLed(bool on);

    /// Enable/disable WisBlock IO slot power
    void setIOPower(bool on);

private:
    bool _initialized = false;

    // Stub drivers (never initialized, always report not-present)
    Trackball _stubTrackball;
    Keyboard _stubKeyboard;

    // SSD1306 OLED display driver (128x64, I2C, no reset pin on RAK4631)
    DisplaySSD1306 _display{128, 64,
        (int8_t)rak4631::I2C1_SDA,
        (int8_t)rak4631::I2C1_SCL,
        -1,  // No reset pin on RAK4631 OLED
        0x3C};

    // Battery ADC config
    // RAK4631 uses voltage divider on AIN3 (P0.05)
    // Vbat = ADC_reading * (3.6V / 2^14) * divider_ratio
    // RAK5005-O baseboard uses a 1.5x divider (1M + 2M)
    float _adcMultiplier = 1.5f;  // Voltage divider ratio for RAK5005-O
};

}  // namespace oms