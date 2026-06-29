// OpenMeshOS — DisplaySSD1306.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// SSD1306 OLED display driver implementing IDisplay.
//
// Supports the 0.96" SSD1306 OLED (128x64, I2C) used on:
//   - Heltec WiFi LoRa 32 V3 (ESP32-S3, I2C on GPIO 17/18, RST on GPIO 21)
//   - RAK WisBlock RAK4631 (nRF52840, I2C on WB_I2C1, no reset pin)
//
// Uses the Adafruit SSD1306 library for low-level I2C communication.
// LVGL integration: the UI layer calls pushImage() to flush pixels.
// Since SSD1306 is 1bpp (monochrome), we convert from LVGL's RGB565
// colour data to 1-bit monochrome at flush time.
//
// Display buffer: SSD1306 128x64 = 1024 bytes (1bpp). This is small
// enough to keep in internal RAM (no PSRAM needed, works on nRF52).
//
// Phase 4 roadmap: "Display driver abstraction (IDisplay.h)"

#pragma once

#include "IDisplay.h"

#ifdef OMS_DISPLAY_SSD1306

#include <Adafruit_SSD1306.h>

namespace oms {

/// DisplaySSD1306 — concrete IDisplay implementation for SSD1306 OLED.
///
/// Usage:
///   DisplaySSD1306 disp(width, height, sdaPin, sclPin, rstPin, i2cAddr);
///   disp.begin();
///   disp.setBacklight(true);
///   // In LVGL flush callback:
///   disp.pushImage(x, y, w, h, pixels);
///   disp.flushDone();
///
class DisplaySSD1306 : public IDisplay {
public:
    /// Constructor.
    /// \param width   Display width in pixels (128 for standard SSD1306)
    /// \param height  Display height in pixels (64 for standard SSD1306)
    /// \param sdaPin  I2C SDA pin
    /// \param sclPin  I2C SCL pin
    /// \param rstPin  Reset pin (-1 / 0xFF if no reset pin)
    /// \param i2cAddr I2C address (typically 0x3C or 0x3D)
    DisplaySSD1306(uint16_t width, uint16_t height,
                   int8_t sdaPin, int8_t sclPin, int8_t rstPin,
                   uint8_t i2cAddr = 0x3C);

    // ── IDisplay interface ─────────────────────────────────────────

    void begin() override;
    void end() override;

    DisplayCaps capabilities() const override
    {
        return DisplayCaps{
            .type           = DisplayType::OLED_I2C,
            .width          = _width,
            .height         = _height,
            .hasBacklight   = false,   // OLED has no backlight
            .hasBrightness  = false,   // SSD1306 has no brightness PWM
            .isColor        = false,   // Monochrome
            .bpp            = 1,       // 1 bit per pixel
            .partialFlush   = true     // SSD1306 supports partial updates
        };
    }

    void pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h,
                   const uint8_t* data) override;

    void pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h,
                   const uint16_t* data) override;

    void fillScreen(uint16_t color) override;

    void flushDone() override;

    void setBacklight(bool on) override;
    void setBrightness(int level) override;

    void sleep() override;
    void wake() override;

    void setRotation(uint8_t rotation) override;
    uint8_t getRotation() const override { return _rotation; }

    // ── SSD1306-specific ───────────────────────────────────────────

    /// Get the underlying Adafruit SSD1306 instance (for direct access)
    Adafruit_SSD1306& raw() { return _oled; }

    /// Draw a single pixel (for non-LVGL direct drawing, e.g. status line)
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    /// Display the internal buffer to the screen (call after drawing)
    void display();

    /// Clear the internal buffer (does not push to screen)
    void clearBuffer();

    /// Draw a text string at (x, y) using the built-in font
    void drawText(int16_t x, int16_t y, const char* text, uint16_t color = 1);

    /// Draw a horizontal progress bar
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                         uint8_t progress);

private:
    Adafruit_SSD1306 _oled;

    uint16_t _width;
    uint16_t _height;
    int8_t   _sdaPin;
    int8_t   _sclPin;
    int8_t   _rstPin;
    uint8_t  _i2cAddr;
    uint8_t  _rotation = 0;
    bool     _begun = false;

    // Convert RGB565 pixel to 1-bit monochrome (threshold on luminance)
    static bool rgb565ToMono(uint16_t pixel);

    // Luminance threshold: pixels darker than this are "on" (white on OLED)
    // SSD1306: colour=1 means pixel ON (white), colour=0 means OFF (black)
    static constexpr uint16_t LUMA_THRESHOLD = 0x8000;
};

}  // namespace oms

#endif  // OMS_DISPLAY_SSD1306