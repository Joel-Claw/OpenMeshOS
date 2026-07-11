// OpenMeshOS — Trackball.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Runtime trackball detection and abstraction.
// T-Deck hardware has two known trackball variants:
//   1. GPIO trackball (all known boards): pins 3,15,1,2,0
//      Confirmed by Meshtastic variant.h and LilyGo utilities.h (TBOX_G01-G04).
//      GPIO 1=LEFT, 2=RIGHT, 0=PRESS (also BOOT button).
//   2. I2C optical sensor at 0x4A (AFBR S10 or similar)
//
// NOTE: Earlier code defined a "V1" variant with pins 21,43,44. This was WRONG.
// Those pins are: GPIO 21 = ES7210 mic LRCK, GPIO 43 = GPS TX, GPIO 44 = GPS RX.
// No known T-Deck hardware uses those pins for trackball.
// The GPIO trackball config (3,15,1,2,0) is valid for ALL hardware revisions.
//
// We detect at startup which variant (GPIO vs I2C) is present and switch behavior.

#pragma once

#include <Arduino.h>
#include <Wire.h>

#ifdef ARDUINO_ARCH_NRF52840
  // nRF52 doesn't have gpio_num_t — use uint32_t for pin numbers
  // (nRF52 Arduino core uses plain integer pin numbers)
  #define OMS_GPIO_T uint32_t
#else
  #define OMS_GPIO_T gpio_num_t
#endif

namespace oms {

enum class TrackballType : uint8_t {
    NONE = 0,       // No trackball detected
    GPIO_V1 = 1,    // DEPRECATED — was wrong (pins 21,43,44 are mic/GPS). Do not use.
    GPIO_STD = 2,   // All known boards: UP=3, DOWN=15, LEFT=1, RIGHT=2, PRESS=0
    GPIO_V2 = 2,    // Alias: same as GPIO_STD, for backward compat
    I2C_OPTICAL = 3 // I2C optical sensor at 0x4A
};

class Trackball {
public:
    // Initialize: detect trackball type and configure
    void begin(TwoWire& wire);

    // Poll for movement (call from main loop, ~60Hz)
    void tick();

    // Consume accumulated delta and press
    // dx/dy are accelerated values (pixels per frame)
    void consumeDelta(int16_t& dx, int16_t& dy);
    bool consumePress();

    TrackballType type() const { return _type; }
    const char* typeName() const;

private:
    TrackballType _type = TrackballType::NONE;

    // Accumulated raw movement (before acceleration)
    int16_t _rawDx = 0;
    int16_t _rawDy = 0;
    bool   _pressed = false;

    // Debounce: press must be stable for N ticks
    static constexpr uint8_t PRESS_DEBOUNCE_TICKS = 3;
    uint8_t _pressCounter = 0;     // counts consecutive pressed ticks
    bool    _pressRegistered = false; // true once per press event

    // Acceleration: exponential curve for comfortable navigation
    // Raw delta is squared (with minimum) to create faster movement
    // when scrolling quickly
    static constexpr int16_t ACCEL_MIN = 1;          // minimum movement per frame
    static constexpr int16_t ACCEL_THRESHOLD = 4;    // raw delta above this gets accelerated
    static constexpr int16_t ACCEL_CURVE_SHIFT = 1;  // divide raw by 2^N before squaring
    static constexpr int16_t ACCEL_MAX = 12;           // cap maximum delta per frame

    // I2C optical sensor noise filtering:
    // Some AFBR S10 sensors produce constant drift (e.g., always reporting
    // small downward movement) even when the trackball is idle. This causes
    // the "scrolling itself down" bug reported in MeshCore issue #1469.
    // We apply two filters:
    //   1. Dead zone: ignore deltas smaller than I2C_DEAD_ZONE
    //   2. Drift suppression: if we see I2C_DRIFT_MAX consecutive ticks
    //      with movement only in one direction (and no user press),
    //      we suppress that axis until the user actively moves the other way.
    static constexpr int16_t I2C_DEAD_ZONE = 1;          // ignore deltas with |value| <= this
    static constexpr uint8_t  I2C_DRIFT_MAX = 20;        // consecutive same-direction ticks = drift
    static constexpr uint8_t  I2C_DRIFT_SUPPRESS_TICKS = 10; // suppress drift axis for N ticks

    // Drift state for I2C sensor
    uint8_t  _driftCountX = 0;   // consecutive ticks with X movement
    uint8_t  _driftCountY = 0;   // consecutive ticks with Y movement
    int8_t   _driftDirX = 0;     // last drift direction for X: +1 or -1
    int8_t   _driftDirY = 0;     // last drift direction for Y: +1 or -1
    uint8_t  _driftSuppressX = 0; // ticks remaining to suppress X axis
    uint8_t  _driftSuppressY = 0; // ticks remaining to suppress Y axis

    /// Apply acceleration curve to raw delta
    static int16_t accelerate(int16_t raw);

    // GPIO pin sets per variant (defined in .cpp)
    struct GPIOPins {
        OMS_GPIO_T up;
        OMS_GPIO_T down;
        OMS_GPIO_T left;
        OMS_GPIO_T right;
        OMS_GPIO_T press;
    };

    // GPIO trackball (all known hardware revisions)
    // Confirmed by Meshtastic variant.h and LilyGo utilities.h TBOX_G01-G04
    // UP=3 (G01), DOWN=15 (G03), LEFT=1 (G04), RIGHT=2 (G02), PRESS=0 (BOOT)
    static const GPIOPins PINS_GPIO;

    // Legacy aliases for backward compatibility
    static const GPIOPins PINS_V1;  // DEPRECATED: same as PINS_GPIO
    static const GPIOPins PINS_V2;  // same as PINS_GPIO

    static constexpr uint8_t I2C_ADDR = 0x4A;

    GPIOPins _pins{};

    // Detection helpers
    bool probeI2C(TwoWire& wire);
    bool probeGPIO();
    bool probeGPIO_V1();  // legacy alias for probeGPIO
    bool probeGPIO_V2();  // legacy alias for probeGPIO
    void configureGPIOPins();

    // I2C read helper: returns signed delta byte
    int8_t readI2CDelta(TwoWire& wire, uint8_t reg);

    TwoWire* _wire = nullptr;
};

}  // namespace oms