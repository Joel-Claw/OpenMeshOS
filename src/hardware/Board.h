// OpenMeshOS — Board.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Hardware abstraction for the T-Deck family.
// One Board singleton owns: display, keyboard, trackball, LoRa, GPS.

#pragma once

#include <Arduino.h>
#ifdef OMS_HAS_BUILTIN_GPS
#include <TinyGPSPlus.h>
#endif
#include "Keyboard.h"
#include "Trackball.h"

namespace oms {

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

    // Display backlight
    void setBacklight(bool on) { digitalWrite(2, on ? HIGH : LOW); }

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