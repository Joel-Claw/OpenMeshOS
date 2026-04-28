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

    // Poll for movement (call from main loop)
    void tick();

    // Consume accumulated delta and press
    void consumeDelta(int16_t& dx, int16_t& dy);
    bool consumePress();

    TrackballType type() const { return _type; }
    const char* typeName() const;

private:
    TrackballType _type = TrackballType::NONE;

    // Accumulated movement
    int16_t _dx = 0;
    int16_t _dy = 0;
    bool   _pressed = false;

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