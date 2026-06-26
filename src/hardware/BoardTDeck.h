// OpenMeshOS — BoardTDeck.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// IBoard implementation for the LilyGo T-Deck and T-Deck Plus.
// This is the original Board class refactored to implement the
// IBoard interface for Phase 4 multi-device support.
//
// Pin definitions cross-referenced with official LilyGo T-Deck utilities.h:
// https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h

#pragma once

#include "IBoard.h"
#include "IDisplay.h"
#include "IInput.h"
#include "Keyboard.h"
#include "Trackball.h"

#ifdef OMS_HAS_BUILTIN_GPS
#include <TinyGPSPlus.h>
#endif

namespace oms {

// ── Pin constants (T-Deck / T-Deck Plus) ──────────────────────────────
namespace tdeck {
    constexpr gpio_num_t LORA_CS    = GPIO_NUM_9;
    constexpr gpio_num_t LORA_RST   = GPIO_NUM_17;
    constexpr gpio_num_t LORA_DIO1  = GPIO_NUM_45;
    constexpr gpio_num_t LORA_BUSY  = GPIO_NUM_13;
    constexpr gpio_num_t LORA_SCK   = GPIO_NUM_40;
    constexpr gpio_num_t LORA_MISO  = GPIO_NUM_38;
    constexpr gpio_num_t LORA_MOSI  = GPIO_NUM_41;

    // Display pins (named with DISP_ prefix to avoid TFT_eSPI macro conflicts)
    constexpr gpio_num_t DISP_CS    = GPIO_NUM_12;
    constexpr gpio_num_t DISP_DC    = GPIO_NUM_11;
    constexpr gpio_num_t DISP_SCK   = GPIO_NUM_40;   // shared SPI bus
    constexpr gpio_num_t DISP_MOSI  = GPIO_NUM_41;   // shared SPI bus
    constexpr gpio_num_t DISP_BL    = GPIO_NUM_42;

    constexpr gpio_num_t KB_SDA     = GPIO_NUM_18;
    constexpr gpio_num_t KB_SCL     = GPIO_NUM_8;
    constexpr gpio_num_t KB_INT     = GPIO_NUM_46;

    // Trackball GPIO (all known T-Deck hardware)
    constexpr gpio_num_t TB_UP     = GPIO_NUM_3;   // TBOX_G01
    constexpr gpio_num_t TB_DOWN   = GPIO_NUM_15;  // TBOX_G03
    constexpr gpio_num_t TB_LEFT   = GPIO_NUM_1;   // TBOX_G04
    constexpr gpio_num_t TB_RIGHT  = GPIO_NUM_2;   // TBOX_G02
    constexpr gpio_num_t TB_PRESS  = GPIO_NUM_0;   // BOOT button

    // GPS (T-Deck Plus only)
    constexpr gpio_num_t GPS_TX     = GPIO_NUM_43;
    constexpr gpio_num_t GPS_RX     = GPIO_NUM_44;

    // SD card
    constexpr gpio_num_t SD_CS      = GPIO_NUM_39;

    // Battery ADC
    constexpr gpio_num_t BAT_ADC    = GPIO_NUM_4;

    // Touch interrupt
    constexpr gpio_num_t TOUCH_INT   = GPIO_NUM_16;

    // I2S audio
    constexpr gpio_num_t I2S_BCK    = GPIO_NUM_7;
    constexpr gpio_num_t I2S_WS     = GPIO_NUM_5;
    constexpr gpio_num_t I2S_DOUT   = GPIO_NUM_6;

    // Board power enable (must be HIGH for LoRa, SD, audio)
    constexpr gpio_num_t POWER_EN    = GPIO_NUM_10;

    // Boot button (shared with trackball press)
    constexpr gpio_num_t BOOT_PIN    = GPIO_NUM_0;
}

/// IBoard implementation for LilyGo T-Deck and T-Deck Plus.
///
/// Owns: display backlight, keyboard, trackball, GPS (if present).
/// The SX1262 LoRa radio and MeshCore stack are owned by MeshService,
/// not by this class — they use the LoRaConfig from this board.
class BoardTDeck : public IBoard {
public:
    BoardTDeck() = default;

    // ── IBoard interface ──────────────────────────────────────────
    void init() override;
    void tick() override;

    const char* boardName() const override { return "LilyGo T-Deck"; }

    BoardCaps capabilities() const override {
        return BoardCaps{
            .hasKeyboard    = _keyboard.isPresent(),
            .hasTrackball   = true,
            .hasGPS         =
                #ifdef OMS_HAS_BUILTIN_GPS
                    true,
                #else
                    false,
                #endif
            .hasSDCard      = true,
            .hasBLE          = true,   // ESP32-S3 always has BLE
            .hasSpeaker      = true,
            .hasTouchScreen  = false,  // T-Deck has no touch overlay
            .hasBatteryADC   = true,
            .hasLoRa         = true
        };
    }

    DisplayConfig displayConfig() const override {
        return DisplayConfig{
            .width    = OMS_SCREEN_W,
            .height   = OMS_SCREEN_H,
            .csPin    = (uint8_t)tdeck::DISP_CS,
            .dcPin    = (uint8_t)tdeck::DISP_DC,
            .rstPin   = -1,  // T-Deck has no display reset
            .blPin    = (uint8_t)tdeck::DISP_BL,
            .sckPin   = (uint8_t)tdeck::DISP_SCK,
            .mosiPin  = (uint8_t)tdeck::DISP_MOSI,
            .spiFreq  = 40000000  // 40MHz
        };
    }

    // ── Display driver (IBoard override) ───────────────────────────────
    // Returns nullptr until DisplayTFTeSPI implementation is added.
    // UIScreen.cpp currently uses TFT_eSPI directly; migration to IDisplay
    // will happen in a follow-up commit.
    IDisplay* display() override { return nullptr; }

    void setBacklight(bool on) override {
        digitalWrite(tdeck::DISP_BL, on ? HIGH : LOW);
    }

    void setBrightness(int level) override {
        // T-Deck backlight is digital (on/off), not PWM dimmable.
        // If brightness is 0, turn off; otherwise turn on.
        // Future: could use LEDC PWM on the backlight pin for true dimming.
        digitalWrite(tdeck::DISP_BL, (level > 0) ? HIGH : LOW);
    }

    LoRaConfig loraConfig() const override {
        return LoRaConfig{
            .freqMHz  = 868.0f,
            .bwMHz    = 125.0f,
            .sf       = 9,
            .cr        = 5,
            .txPower  = 17,   // default 17 dBm, configurable
            .csPin    = (uint8_t)tdeck::LORA_CS,
            .dio1Pin  = (uint8_t)tdeck::LORA_DIO1,
            .rstPin   = (uint8_t)tdeck::LORA_RST,
            .busyPin  = (uint8_t)tdeck::LORA_BUSY,
            .sckPin   = (uint8_t)tdeck::LORA_SCK,
            .misoPin  = (uint8_t)tdeck::LORA_MISO,
            .mosiPin  = (uint8_t)tdeck::LORA_MOSI
        };
    }

    // ── GPS ───────────────────────────────────────────────────────
    bool hasGPSFix() const override;
    float gpsLat() const override;
    float gpsLng() const override;
    float gpsAltitude() const override;
    float gpsSpeed() const override;
    float gpsCourse() const override;
    int gpsSatellites() const override;
    uint32_t gpsAge() const override;

    // ── Battery ───────────────────────────────────────────────────
    uint16_t batteryMilliVolts() const override;
    int batteryPercent() const override;
    float mcuTemperature() const override;

    // ── Power ──────────────────────────────────────────────────────
    void reboot() override;
    void powerOff() override;
    uint32_t resetReason() const override;

    // ── Input driver (IBoard override) ───────────────────────────────
    // Returns nullptr until InputTDeck implementation is added.
    // Keyboard/Trackball are currently accessed via concrete accessors.
    IInput* input() override { return nullptr; }

    // ── Input ──────────────────────────────────────────────────────
    bool hasKeyboard() const override { return _keyboard.isPresent(); }
    bool consumeTrackballPress() override { return _trackball.consumePress(); }
    void consumeTrackballDelta(int16_t& dx, int16_t& dy) override { _trackball.consumeDelta(dx, dy); }

    // ── Input drivers (IBoard override) ───────────────────────────────
    Keyboard& keyboard() override { return _keyboard; }
    Trackball& trackball() override { return _trackball; }

    /// Legacy singleton accessor (for gradual migration)
    static BoardTDeck& instance();

    bool initialized() const { return _initialized; }

private:
    bool _initialized = false;
    Trackball _trackball;
    Keyboard _keyboard;

#ifdef OMS_HAS_BUILTIN_GPS
    HardwareSerial _gpsSerial{1};
    TinyGPSPlus _gps;
#endif

    // Battery ADC config
    float _adcMultiplier = 2.0f;  // Voltage divider ratio
};

}  // namespace oms