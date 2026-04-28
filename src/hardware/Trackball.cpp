// OpenMeshOS — Trackball.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Runtime trackball detection and driver.
// Strategy:
//   1. Probe I2C bus for optical sensor at 0x4A
//   2. Try GPIO V2 pins (newer boards: 1,2,0 for L,R,Click)
//   3. Try GPIO V1 pins (original boards: 21,43,44 for L,R,Click)
//   4. Fall back to NONE
//
// Detection heuristic:
//   - I2C: attempt to read a register. If we get an ACK, sensor present.
//   - GPIO: set pullups, read state. On V2 boards, GPIO 0 is boot pin
//     so it reads LOW during normal operation. We detect V2 by checking
//     if the V1-exclusive pins (21, 43, 44) float HIGH with pullup
//     while V2-exclusive pins (1, 2, 0) show activity.
//
// In practice, the I2C check is most reliable. GPIO variant detection
// uses a simpler approach: configure both pin sets with pullups, then
// check which set the user is actually moving. For the initial release,
// we default to V1 if I2C is not found, and allow override via config.

#include "Trackball.h"
#include "../utils/Log.h"

namespace oms {

// Static pin set definitions
const Trackball::GPIOPins Trackball::PINS_V1 = {
    GPIO_NUM_3,   // UP
    GPIO_NUM_15,  // DOWN
    GPIO_NUM_21,  // LEFT
    GPIO_NUM_43,  // RIGHT
    GPIO_NUM_44   // PRESS
};

const Trackball::GPIOPins Trackball::PINS_V2 = {
    GPIO_NUM_3,   // UP
    GPIO_NUM_15,  // DOWN
    GPIO_NUM_1,   // LEFT
    GPIO_NUM_2,   // RIGHT
    GPIO_NUM_0    // PRESS
};

const char* Trackball::typeName() const {
    switch (_type) {
        case TrackballType::NONE:        return "none";
        case TrackballType::GPIO_V1:     return "GPIO v1 (pins 3,15,21,43,44)";
        case TrackballType::GPIO_V2:     return "GPIO v2 (pins 3,15,1,2,0)";
        case TrackballType::I2C_OPTICAL: return "I2C optical (0x4A)";
        default:                         return "unknown";
    }
}

void Trackball::begin(TwoWire& wire) {
    _wire = &wire;

    // Step 1: probe I2C optical sensor
    if (probeI2C(wire)) {
        _type = TrackballType::I2C_OPTICAL;
        OMS_LOG("Trackball", "Detected I2C optical sensor at 0x%02X", I2C_ADDR);
        return;
    }

    // Step 2: try GPIO V2 first (newer boards)
    // GPIO 0 is the BOOT button on ESP32-S3, so on V2 boards it will
    // read LOW when pressed and HIGH when released. If we see GPIO 0
    // released (HIGH with pullup) AND GPIO 1/2 are also HIGH with pullup,
    // we tentatively identify V2. But we can't be 100% sure without
    // user interaction, so we fall through.
    if (probeGPIO_V2()) {
        _type = TrackballType::GPIO_V2;
        _pins = PINS_V2;
        OMS_LOG("Trackball", "Detected GPIO v2 trackball (pins 3,15,1,2,0)");
        configureGPIOPins();
        return;
    }

    // Step 3: try GPIO V1 (original boards)
    if (probeGPIO_V1()) {
        _type = TrackballType::GPIO_V1;
        _pins = PINS_V1;
        OMS_LOG("Trackball", "Detected GPIO v1 trackball (pins 3,15,21,43,44)");
        configureGPIOPins();
        return;
    }

    // Step 4: default to V1 (most common in the wild)
    _type = TrackballType::GPIO_V1;
    _pins = PINS_V1;
    OMS_LOG("Trackball", "No trackball detected, defaulting to GPIO v1");
    configureGPIOPins();
}

void Trackball::configureGPIOPins() {
    pinMode(_pins.up,    INPUT_PULLUP);
    pinMode(_pins.down,  INPUT_PULLUP);
    pinMode(_pins.left,  INPUT_PULLUP);
    pinMode(_pins.right, INPUT_PULLUP);
    pinMode(_pins.press, INPUT_PULLUP);
}

bool Trackball::probeI2C(TwoWire& wire) {
    wire.beginTransmission(I2C_ADDR);
    uint8_t err = wire.endTransmission();
    if (err == 0) {
        // Got ACK, try reading a byte to confirm it is responsive
        wire.requestFrom(I2C_ADDR, (uint8_t)1);
        if (wire.available()) {
            wire.read();  // discard, just confirming device responds
            return true;
        }
    }
    return false;
}

bool Trackball::probeGPIO_V1() {
    // Set V1-specific pins as input with pullup
    // If they float HIGH, they are likely connected to trackball switches
    // GPIO 21, 43, 44 are V1-specific
    pinMode(GPIO_NUM_21, INPUT_PULLUP);
    pinMode(GPIO_NUM_43, INPUT_PULLUP);
    pinMode(GPIO_NUM_44, INPUT_PULLUP);

    // Small delay for pullup to settle
    delayMicroseconds(100);

    // If any V1-specific pin reads LOW (pressed), the trackball exists
    // If all read HIGH, it is inconclusive (could be released or absent)
    // Check if the shared pins (3, 15) also read HIGH (expected)
    bool v1_pins_valid = (digitalRead(GPIO_NUM_21) == HIGH &&
                          digitalRead(GPIO_NUM_43) == HIGH &&
                          digitalRead(GPIO_NUM_44) == HIGH);

    // Check if V2-specific pins are ALSO high, which would mean V2 instead
    pinMode(GPIO_NUM_1, INPUT_PULLUP);
    pinMode(GPIO_NUM_2, INPUT_PULLUP);
    pinMode(GPIO_NUM_0, INPUT_PULLUP);
    delayMicroseconds(100);

    // GPIO 0 is the BOOT button, reads LOW normally
    // If GPIO 1 and 2 are LOW (unusual for floating), V2 is more likely
    bool v2_pins_active = (digitalRead(GPIO_NUM_1) == LOW ||
                           digitalRead(GPIO_NUM_2) == LOW);

    if (v2_pins_active) {
        return false;  // V2 pins are active, not V1
    }

    return v1_pins_valid;
}

bool Trackball::probeGPIO_V2() {
    // GPIO 0 is BOOT button on ESP32-S3, normally pulled HIGH externally
    // GPIO 1 and 2 are V2-specific trackball pins
    // On V2 boards, GPIO 1 = LEFT, GPIO 2 = RIGHT, GPIO 0 = PRESS
    pinMode(GPIO_NUM_1, INPUT_PULLUP);
    pinMode(GPIO_NUM_2, INPUT_PULLUP);
    pinMode(GPIO_NUM_0, INPUT_PULLUP);
    delayMicroseconds(100);

    // On V2 boards, GPIO 0 is connected to trackball press switch
    // It reads HIGH when not pressed (with pullup)
    // GPIO 1 and 2 also read HIGH when not pressed
    // The issue: these pins also read HIGH if trackball is absent
    // So we check the V1 pins: if GPIO 21/43/44 are floating/LOW,
    // then V1 is absent and V2 is more likely

    pinMode(GPIO_NUM_21, INPUT_PULLUP);
    pinMode(GPIO_NUM_43, INPUT_PULLUP);
    pinMode(GPIO_NUM_44, INPUT_PULLUP);
    delayMicroseconds(100);

    // If V1-specific pins read LOW (no pullup resistor present = floating),
    // they are likely NOT connected, suggesting V2
    bool v1_pins_absent = (digitalRead(GPIO_NUM_21) == LOW ||
                           digitalRead(GPIO_NUM_43) == LOW ||
                           digitalRead(GPIO_NUM_44) == LOW);

    // If V2 pins all read HIGH (not floating), V2 is possible
    bool v2_pins_present = (digitalRead(GPIO_NUM_1) == HIGH &&
                            digitalRead(GPIO_NUM_2) == HIGH &&
                            digitalRead(GPIO_NUM_0) == HIGH);

    // Strong signal: V1 pins are floating, V2 pins have pullups
    if (v1_pins_absent && v2_pins_present) {
        return true;
    }

    return false;
}

void Trackball::tick() {
    switch (_type) {
        case TrackballType::GPIO_V1:
        case TrackballType::GPIO_V2:
            _dy += (!digitalRead(_pins.down) - !digitalRead(_pins.up));
            _dx += (!digitalRead(_pins.right) - !digitalRead(_pins.left));
            if (!digitalRead(_pins.press)) {
                _pressed = true;
            }
            break;

        case TrackballType::I2C_OPTICAL:
            if (_wire) {
                // Read X and Y deltas from I2C sensor
                // The AFBR S10 / similar optical sensors provide
                // motion delta registers. Register map varies by sensor,
                // but common pattern: register 0x02 = deltaX, 0x03 = deltaY
                // We try reading 2 bytes starting at register 0x02
                int8_t dX = readI2CDelta(*_wire, 0x02);
                int8_t dY = readI2CDelta(*_wire, 0x03);
                _dx += dX;
                _dy += dY;

                // Press detection: GPIO 44 (V1) or GPIO 0 (V2)
                // The I2C sensor only provides motion, not press
                // Check both press pins as fallback
                pinMode(GPIO_NUM_44, INPUT_PULLUP);
                if (!digitalRead(GPIO_NUM_44)) {
                    _pressed = true;
                } else {
                    pinMode(GPIO_NUM_0, INPUT_PULLUP);
                    if (!digitalRead(GPIO_NUM_0)) {
                        _pressed = true;
                    }
                }
            }
            break;

        case TrackballType::NONE:
            break;
    }
}

void Trackball::consumeDelta(int16_t& dx, int16_t& dy) {
    dx = _dx;
    dy = _dy;
    _dx = 0;
    _dy = 0;
}

bool Trackball::consumePress() {
    if (_pressed) {
        _pressed = false;
        return true;
    }
    return false;
}

int8_t Trackball::readI2CDelta(TwoWire& wire, uint8_t reg) {
    wire.beginTransmission(I2C_ADDR);
    wire.write(reg);
    if (wire.endTransmission() != 0) {
        return 0;  // NACK
    }
    wire.requestFrom(I2C_ADDR, (uint8_t)1);
    if (wire.available()) {
        return (int8_t)wire.read();
    }
    return 0;
}

}  // namespace oms