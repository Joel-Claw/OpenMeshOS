// OpenMeshOS — Trackball.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Runtime trackball detection and abstraction.
// T-Deck hardware has three known variants:
//   1. GPIO trackball (original): pins 3,15,21,43,44
//   2. GPIO trackball (newer):    pins 3,15,1,2,0
//   3. I2C optical sensor at 0x4A (AFBR S10 or similar)
//
// We detect at startup which variant is present and switch behavior.

#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace oms {

enum class TrackballType : uint8_t {
    NONE = 0,       // No trackball detected
    GPIO_V1 = 1,    // Original T-Deck: UP=3, DOWN=15, LEFT=21, RIGHT=43, PRESS=44
    GPIO_V2 = 2,    // Newer T-Deck:  UP=3, DOWN=15, LEFT=1,  RIGHT=2,  PRESS=0
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
        gpio_num_t up;
        gpio_num_t down;
        gpio_num_t left;
        gpio_num_t right;
        gpio_num_t press;
    };

    static const GPIOPins PINS_V1;
    static const GPIOPins PINS_V2;

    static constexpr uint8_t I2C_ADDR = 0x4A;

    GPIOPins _pins{};

    // Detection helpers
    bool probeI2C(TwoWire& wire);
    bool probeGPIO_V1();
    bool probeGPIO_V2();
    void configureGPIOPins();

    // I2C read helper: returns signed delta byte
    int8_t readI2CDelta(TwoWire& wire, uint8_t reg);

    TwoWire* _wire = nullptr;
};

}  // namespace oms