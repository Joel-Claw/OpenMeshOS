// OpenMeshOS — Board.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Backward-compatible Board singleton for the T-Deck family.
//
// Phase 4 multi-device refactoring:
//   - New code should use IBoard* (from IBoard.h) obtained via BoardFactory::create()
//   - Old code can still use Board::instance() for compatibility
//   - Board::instance() now delegates to BoardTDeck::instance()
//   - Pin constants in namespace `pins` are DEPRECATED; use `tdeck::` from BoardTDeck.h
//
// The pins:: namespace is kept for backward compatibility but new code
// should include BoardTDeck.h and use the tdeck:: namespace instead.

#pragma once

#include "IBoard.h"
#include "BoardTDeck.h"
#include <Arduino.h>

namespace oms {

// ── DEPRECATED pin constants (use tdeck:: from BoardTDeck.h instead) ──
namespace pins {
    // These delegate to tdeck:: for backward compatibility.
    // New code: #include "BoardTDeck.h" and use tdeck::LORA_CS etc.
    constexpr gpio_num_t LORA_CS    = tdeck::LORA_CS;
    constexpr gpio_num_t LORA_RST   = tdeck::LORA_RST;
    constexpr gpio_num_t LORA_DIO1  = tdeck::LORA_DIO1;
    constexpr gpio_num_t LORA_BUSY  = tdeck::LORA_BUSY;
    constexpr gpio_num_t LORA_SCK   = tdeck::LORA_SCK;
    constexpr gpio_num_t LORA_MISO  = tdeck::LORA_MISO;
    constexpr gpio_num_t LORA_MOSI  = tdeck::LORA_MOSI;

    constexpr gpio_num_t DISP_CS    = tdeck::DISP_CS;
    constexpr gpio_num_t DISP_DC    = tdeck::DISP_DC;
    constexpr gpio_num_t DISP_SCK   = tdeck::DISP_SCK;
    constexpr gpio_num_t DISP_MOSI  = tdeck::DISP_MOSI;
    constexpr gpio_num_t DISP_BL    = tdeck::DISP_BL;

    constexpr gpio_num_t KB_SDA     = tdeck::KB_SDA;
    constexpr gpio_num_t KB_SCL     = tdeck::KB_SCL;

    constexpr gpio_num_t TB_UP     = tdeck::TB_UP;
    constexpr gpio_num_t TB_DOWN   = tdeck::TB_DOWN;
    constexpr gpio_num_t TB_LEFT   = tdeck::TB_LEFT;
    constexpr gpio_num_t TB_RIGHT  = tdeck::TB_RIGHT;
    constexpr gpio_num_t TB_PRESS  = tdeck::TB_PRESS;

    // DEPRECATED V1 pins (kept for compile compat)
    constexpr gpio_num_t TB_V1_UP    = tdeck::TB_UP;
    constexpr gpio_num_t TB_V1_DOWN  = tdeck::TB_DOWN;
    constexpr gpio_num_t TB_V1_LEFT  = GPIO_NUM_21;   // DEPRECATED — actually ES7210 LRCK
    constexpr gpio_num_t TB_V1_RIGHT = GPIO_NUM_43;   // DEPRECATED — actually GPS TX
    constexpr gpio_num_t TB_V1_PRESS = GPIO_NUM_44;   // DEPRECATED — actually GPS RX

    constexpr gpio_num_t TB_V2_UP    = tdeck::TB_UP;
    constexpr gpio_num_t TB_V2_DOWN  = tdeck::TB_DOWN;
    constexpr gpio_num_t TB_V2_LEFT  = tdeck::TB_LEFT;
    constexpr gpio_num_t TB_V2_RIGHT = tdeck::TB_RIGHT;
    constexpr gpio_num_t TB_V2_PRESS = tdeck::TB_PRESS;

    constexpr gpio_num_t GPS_TX     = tdeck::GPS_TX;
    constexpr gpio_num_t GPS_RX     = tdeck::GPS_RX;
    constexpr gpio_num_t SD_CS       = tdeck::SD_CS;
    constexpr gpio_num_t BAT_ADC     = tdeck::BAT_ADC;
    constexpr gpio_num_t KB_INT      = tdeck::KB_INT;
    constexpr gpio_num_t TOUCH_INT   = tdeck::TOUCH_INT;
    constexpr gpio_num_t I2S_BCK    = tdeck::I2S_BCK;
    constexpr gpio_num_t I2S_WS     = tdeck::I2S_WS;
    constexpr gpio_num_t I2S_DOUT   = tdeck::I2S_DOUT;
    constexpr gpio_num_t POWER_EN    = tdeck::POWER_EN;
    constexpr gpio_num_t BOOT_PIN    = tdeck::BOOT_PIN;
}

// ── Board singleton (backward compat wrapper) ─────────────────────
// Board::instance() returns the global IBoard* as a BoardTDeck&.
// All existing code using Board::instance() continues to work.
// New code should use BoardFactory::create() for multi-device support.

class Board {
public:
    static Board& instance() {
        return s_self;
    }

    void init() { s_tdeck->init(); }
    void tick() { s_tdeck->tick(); }

    // Delegate to BoardTDeck
    bool consumeTrackballPress() { return s_tdeck->consumeTrackballPress(); }
    void consumeTrackballDelta(int16_t &dx, int16_t &dy) { s_tdeck->consumeTrackballDelta(dx, dy); }
    Trackball& trackball() { return s_tdeck->trackball(); }
    Keyboard& keyboard() { return s_tdeck->keyboard(); }
    bool hasKeyboard() const { return s_tdeck->hasKeyboard(); }
    bool hasGPSFix() const { return s_tdeck->hasGPSFix(); }
    float gpsLat() const { return s_tdeck->gpsLat(); }
    float gpsLng() const { return s_tdeck->gpsLng(); }
    float gpsAltitude() const { return s_tdeck->gpsAltitude(); }
    float gpsSpeed() const { return s_tdeck->gpsSpeed(); }
    float gpsCourse() const { return s_tdeck->gpsCourse(); }
    int gpsSatellites() const { return s_tdeck->gpsSatellites(); }
    uint32_t gpsAge() const { return s_tdeck->gpsAge(); }
    void setBacklight(bool on) { s_tdeck->setBacklight(on); }
    bool initialized() const { return s_tdeck->initialized(); }

private:
    static BoardTDeck* s_tdeck;
    static Board s_self;
};

}  // namespace oms