// OpenMeshOS — DisplaySSD1306.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// SSD1306 OLED display driver implementing IDisplay.
//
// See DisplaySSD1306.h for design notes.

#include "DisplaySSD1306.h"
#include "utils/Log.h"

#ifdef OMS_DISPLAY_SSD1306

#include <Wire.h>
#include <Adafruit_GFX.h>

namespace oms {

// ── Constructor ──────────────────────────────────────────────────────
DisplaySSD1306::DisplaySSD1306(uint16_t width, uint16_t height,
                               int8_t sdaPin, int8_t sclPin, int8_t rstPin,
                               uint8_t i2cAddr)
    : _oled(width, height, &Wire, rstPin >= 0 ? rstPin : -1)
    , _width(width)
    , _height(height)
    , _sdaPin(sdaPin)
    , _sclPin(sclPin)
    , _rstPin(rstPin)
    , _i2cAddr(i2cAddr)
{
}

// ── Lifecycle ────────────────────────────────────────────────────────
void DisplaySSD1306::begin()
{
    // Initialize I2C with the board-specific pins
    // nRF52 TwoWire::begin() does not accept pin arguments
    // (pins are fixed by the board variant). ESP32 does.
#ifdef ARDUINO_ARCH_NRF52840
    Wire.begin();
#else
    Wire.begin(_sdaPin, _sclPin);
#endif

    // Initialize the SSD1306 display
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!_oled.begin(SSD1306_SWITCHCAPVCC, _i2cAddr))
    {
        OMS_LOG("display", "SSD1306 allocation failed (I2C addr 0x%02X)", _i2cAddr);
        return;
    }

    _begun = true;

    // Clear the buffer and display a splash screen
    _oled.clearDisplay();
    _oled.setTextSize(1);
    _oled.setTextColor(SSD1306_WHITE);
    _oled.setCursor(0, 0);
    _oled.println("OpenMeshOS");
    _oled.display();

    OMS_LOG("display", "SSD1306 initialized: %ux%u I2C 0x%02X (SDA=%d SCL=%d RST=%d)",
            _width, _height, _i2cAddr, _sdaPin, _sclPin, _rstPin);
}

void DisplaySSD1306::end()
{
    if (!_begun) return;

    // Turn off the display
    _oled.ssd1306_command(SSD1306_DISPLAYOFF);
    _begun = false;

    OMS_LOG("display", "SSD1306 powered off");
}

// ── Drawing: 1bpp monochrome ─────────────────────────────────────────
void DisplaySSD1306::pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h,
                               const uint8_t* data)
{
    // 1bpp monochrome bitmap: 8 pixels per byte, MSB first
    // This maps directly to SSD1306's internal format
    // We use drawBitmap which expects 1bpp data
    _oled.drawBitmap(x, y, data, w, h, SSD1306_WHITE);
}

void DisplaySSD1306::pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h,
                               const uint16_t* data)
{
    // Convert RGB565 to 1bpp monochrome
    // LVGL flushes in RGB565, we need to convert each pixel
    for (uint16_t row = 0; row < h; row++)
    {
        for (uint16_t col = 0; col < w; col++)
        {
            uint16_t pixel = data[row * w + col];
            bool mono = rgb565ToMono(pixel);
            _oled.drawPixel(x + col, y + row, mono ? SSD1306_WHITE : SSD1306_BLACK);
        }
    }
}

void DisplaySSD1306::fillScreen(uint16_t color)
{
    // For OLED: color != 0 means "all pixels on" (white), 0 means "all off" (black)
    if (color != 0)
    {
        _oled.fillScreen(SSD1306_WHITE);
    }
    else
    {
        _oled.clearDisplay();
    }
}

void DisplaySSD1306::flushDone()
{
    // Push the internal buffer to the physical display
    // This is called by LVGL after all flush operations are complete
    if (_begun)
    {
        _oled.display();
    }
}

// ── Backlight / Brightness ───────────────────────────────────────────
void DisplaySSD1306::setBacklight(bool on)
{
    // SSD1306 has no backlight. To save power, we can turn the display on/off.
    if (!_begun) return;
    _oled.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void DisplaySSD1306::setBrightness(int level)
{
    // SSD1306 has no brightness control via PWM.
    // We could use the contrast setting (SSD1306_SETCONTRAST) as a proxy.
    // Range: 0-255, where 0x80 is default contrast.
    if (!_begun) return;
    uint8_t contrast = (uint8_t)((level * 255) / 255);  // Map 0-255 to 0-255
    _oled.ssd1306_command(SSD1306_SETCONTRAST);
    _oled.ssd1306_command(contrast);
}

// ── Power ────────────────────────────────────────────────────────────
void DisplaySSD1306::sleep()
{
    if (!_begun) return;
    _oled.ssd1306_command(SSD1306_DISPLAYOFF);
    OMS_LOG("display", "SSD1306 entering sleep");
}

void DisplaySSD1306::wake()
{
    if (!_begun) return;
    _oled.ssd1306_command(SSD1306_DISPLAYON);
    OMS_LOG("display", "SSD1306 waking up");
}

// ── Rotation ─────────────────────────────────────────────────────────
void DisplaySSD1306::setRotation(uint8_t rotation)
{
    _rotation = rotation & 0x03;
    _oled.setRotation(_rotation);
}

// ── SSD1306-specific helpers ─────────────────────────────────────────
void DisplaySSD1306::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    _oled.drawPixel(x, y, color ? SSD1306_WHITE : SSD1306_BLACK);
}

void DisplaySSD1306::display()
{
    if (_begun)
    {
        _oled.display();
    }
}

void DisplaySSD1306::clearBuffer()
{
    _oled.clearDisplay();
}

void DisplaySSD1306::drawText(int16_t x, int16_t y, const char* text, uint16_t color)
{
    _oled.setCursor(x, y);
    _oled.setTextColor(color ? SSD1306_WHITE : SSD1306_BLACK);
    _oled.setTextSize(1);  // Default 6x8 font
    _oled.print(text);
}

void DisplaySSD1306::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                     uint8_t progress)
{
    // Adafruit_GFX in this version doesn't have drawProgressBar,
    // so we implement it manually: outline + filled portion
    _oled.drawRect(x, y, w, h, SSD1306_WHITE);
    if (progress > 0 && w > 2)
    {
        int16_t fillW = (int16_t)((w - 2) * progress / 100);
        if (fillW > 0)
        {
            _oled.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
        }
    }
}

// ── RGB565 to monochrome conversion ──────────────────────────────────
bool DisplaySSD1306::rgb565ToMono(uint16_t pixel)
{
    // Extract RGB565 components
    uint8_t r = (pixel >> 11) & 0x1F;
    uint8_t g = (pixel >> 5) & 0x3F;
    uint8_t b = pixel & 0x1F;

    // Compute luminance (standard weights for 565 format)
    // Y = 0.299R + 0.587G + 0.114B
    // Scale to 0-255 range: R*8, G*4, B*8
    uint16_t luma = (r * 8 * 299 + g * 4 * 587 + b * 8 * 114) / 1000;

    // Threshold: pixels brighter than ~50% are "on" (white on OLED)
    // On OLED, "on" = white pixel = we treat bright RGB565 as "on"
    // LVGL dark backgrounds → OLED black, LVGL light text → OLED white
    return luma >= 128;
}

}  // namespace oms

#endif  // OMS_DISPLAY_SSD1306