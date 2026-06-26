// OpenMeshOS — IDisplay.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Abstract display driver interface for multi-device support.
//
// Each supported device implements this interface to provide a
// common API for initialising the display, pushing pixels, and
// controlling backlight/brightness.
//
// Concrete implementations:
//   - DisplayTFTeSPI  (T-Deck: ST7789 320x240 SPI TFT)
//   - DisplaySSD1306   (Heltec V3 / RAK4631: 128x64 I2C OLED)
//
// The UI layer (UIScreen) uses IDisplay instead of TFT_eSPI directly,
// so the same LVGL integration code works on all displays.
//
// Phase 4 roadmap item: "Display driver abstraction"

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

/// Display type — determines which driver implementation to use
enum class DisplayType : uint8_t {
    TFT_SPI,     ///< SPI TFT display (ST7789, ILI9341, etc.)
    OLED_I2C,    ///< I2C OLED display (SSD1306, SH1106, etc.)
    NONE,        ///< No display (headless / repeater mode)
};

/// Display capabilities — what features this display supports
struct DisplayCaps {
    DisplayType type;
    uint16_t    width;        ///< Pixels (e.g. 320)
    uint16_t    height;       ///< Pixels (e.g. 240)
    bool        hasBacklight; ///< Can turn backlight on/off
    bool        hasBrightness; ///< Can adjust brightness (PWM)
    bool        isColor;      ///< Color (true) vs monochrome (false)
    uint8_t     bpp;          ///< Bits per pixel (16 for TFT, 1 for OLED)
    bool        partialFlush; ///< Supports partial region flush
};

/// IDisplay — abstract display driver interface
///
/// Usage:
///   IDisplay* disp = theBoard()->display();
///   disp->begin();
///   disp->setBacklight(true);
///   // In LVGL flush callback:
///   disp->pushImage(x, y, w, h, pixels);
///   disp->flushDone();
///
class IDisplay {
public:
    virtual ~IDisplay() = default;

    // ── Lifecycle ──────────────────────────────────────────────────

    /// Initialise the display hardware (SPI/I2C, reset, config).
    /// Call once in setup() before any draw calls.
    virtual void begin() = 0;

    /// De-initialise and power down the display.
    virtual void end() = 0;

    // ── Properties ─────────────────────────────────────────────────

    /// Display capabilities
    virtual DisplayCaps capabilities() const = 0;

    /// Screen width in pixels
    uint16_t width() const { return capabilities().width; }

    /// Screen height in pixels
    uint16_t height() const { return capabilities().height; }

    /// Display type (TFT, OLED, NONE)
    DisplayType type() const { return capabilities().type; }

    /// True if display supports color
    bool isColor() const { return capabilities().isColor; }

    // ── Drawing ────────────────────────────────────────────────────

    /// Push a rectangular block of pixels to the display.
    /// \param x, y  Top-left corner of the region
    /// \param w, h  Width and height of the region
    /// \param data  Pixel data (format depends on display: RGB565 for TFT, 1bpp for OLED)
    ///
    /// For OLED displays, the data format is a monochrome bitmap
    /// (1 bit per pixel, 8 pixels per byte, MSB first).
    /// For TFT displays, the data format is RGB565 (2 bytes per pixel).
    ///
    /// This is called from the LVGL flush callback.
    virtual void pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h,
                           const uint8_t* data) = 0;

    /// Convenience overload for RGB565 pixel data (TFT displays)
    virtual void pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h,
                           const uint16_t* data) = 0;

    /// Fill the entire screen with a colour (RGB565 for TFT, on/off for OLED)
    virtual void fillScreen(uint16_t color) = 0;

    /// Notify the display that the current flush is complete.
    /// For SPI TFT: may trigger DMA completion.
    /// For I2C OLED: may start the next I2C transaction.
    virtual void flushDone() = 0;

    // ── Backlight / Brightness ─────────────────────────────────────

    /// Turn backlight on/off (if supported)
    virtual void setBacklight(bool on) = 0;

    /// Set brightness level (0-255, if supported)
    virtual void setBrightness(int level) = 0;

    // ── Power ──────────────────────────────────────────────────────

    /// Enter sleep mode (display off, backlight off, save power)
    virtual void sleep() = 0;

    /// Wake from sleep mode
    virtual void wake() = 0;

    // ── Rotation ───────────────────────────────────────────────────

    /// Set display rotation (0=portrait, 1=landscape, 2=rev-portrait, 3=rev-landscape)
    virtual void setRotation(uint8_t rotation) = 0;

    /// Get current rotation
    virtual uint8_t getRotation() const = 0;
};

}  // namespace oms