// OpenMeshOS — Board.h
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Hardware abstraction for the T-Deck family.
// One Board singleton owns: display, keyboard, trackball, LoRa, GPS.

#pragma once

#include <Arduino.h>
#ifdef OMS_HAS_BUILTIN_GPS
#include <TinyGPSPlus.h>
#endif

namespace oms {

class Board {
public:
    static Board& instance();

    void init();
    void tick();

    // Trackball
    bool consumeTrackballPress();
    void consumeTrackballDelta(int16_t &dx, int16_t &dy);

    // GPS
    bool hasGPSFix() const;
    float gpsLat() const;
    float gpsLng() const;

    // Display backlight
    void setBacklight(bool on) { digitalWrite(2, on ? HIGH : LOW); }

    bool initialized() const { return _initialized; }

private:
    bool _initialized = false;

    // Trackball accumulator
    int16_t _trackballX = 0;
    int16_t _trackballY = 0;
    bool    _trackballPressed = false;

#ifdef OMS_HAS_BUILTIN_GPS
    HardwareSerial _gpsSerial{1};   // UART1 for GPS
    TinyGPSPlus    _gps;
#endif
};

}  // namespace oms