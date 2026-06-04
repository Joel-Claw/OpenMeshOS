// OpenMeshOS — Trackball.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Runtime trackball detection and driver.
// Strategy:
//   1. Probe I2C bus for optical sensor at 0x4A (AFBR S10 or similar)
//   2. Try GPIO trackball (all known boards: pins 3,15,1,2,0)
//   3. Fall back to NONE
//
// NOTE: Earlier code distinguished "V1" (pins 21,43,44) and "V2" (pins 1,2,0)
// trackball variants. This was WRONG: GPIO 21 is ES7210 mic LRCK, GPIO 43 is
// GPS TX, and GPIO 44 is GPS RX per the official LilyGo utilities.h. No known
// T-Deck hardware uses those pins for trackball. All GPIO trackballs use
// pins 3,15,1,2,0 (confirmed by Meshtastic variant.h and LilyGo TBOX_G01-G04).
//
// I2C detection is most reliable for optical sensors. GPIO detection uses
// pullup probing: if the GPIO trackball pins read HIGH with pullup, they
// are connected. We default to GPIO if I2C is not found.

#include "Trackball.h"
#include "Board.h"
#include "../utils/Log.h"

namespace oms {

// GPIO trackball pin set (all known T-Deck hardware)
// Confirmed by Meshtastic variant.h: TB_UP=3, TB_DOWN=15, TB_LEFT=1, TB_RIGHT=2, TB_PRESS=0
// And by LilyGo utilities.h: BOARD_TBOX_G01=3, G02=2, G03=15, G04=1
const Trackball::GPIOPins Trackball::PINS_GPIO = {
    pins::TB_V2_UP,     // UP = GPIO 3
    pins::TB_V2_DOWN,   // DOWN = GPIO 15
    pins::TB_V2_LEFT,   // LEFT = GPIO 1
    pins::TB_V2_RIGHT,  // RIGHT = GPIO 2
    pins::TB_V2_PRESS    // PRESS = GPIO 0 (also BOOT button)
};

// Legacy aliases: V1 and V2 now both point to the same (correct) GPIO config
const Trackball::GPIOPins Trackball::PINS_V1 = Trackball::PINS_GPIO;
const Trackball::GPIOPins Trackball::PINS_V2 = Trackball::PINS_GPIO;

const char* Trackball::typeName() const {
    switch (_type) {
        case TrackballType::NONE:        return "none";
        case TrackballType::GPIO_V1:     // DEPRECATED — same as GPIO_STD
        case TrackballType::GPIO_STD:    return "GPIO (pins 3,15,1,2,0)";
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

    // Step 2: try GPIO trackball (all known hardware)
    if (probeGPIO()) {
        _type = TrackballType::GPIO_STD;
        _pins = PINS_GPIO;
        OMS_LOG("Trackball", "Detected GPIO trackball (pins 3,15,1,2,0)");
        configureGPIOPins();
        return;
    }

    // Step 3: default to GPIO (most common in the wild)
    _type = TrackballType::GPIO_STD;
    _pins = PINS_GPIO;
    OMS_LOG("Trackball", "No trackball conclusively detected, defaulting to GPIO (pins 3,15,1,2,0)");
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

bool Trackball::probeGPIO() {
    // Configure GPIO trackball pins with pullup
    // GPIO 0 (BOOT/PRESS) is special: on ESP32-S3 it has external pullup
    // and reads HIGH when not pressed. GPIO 1 and 2 are trackball
    // LEFT and RIGHT, both with external pullups on T-Deck.
    // If these pins read HIGH (not floating/LOW), trackball is likely present.
    pinMode(pins::TB_V2_UP,    INPUT_PULLUP);
    pinMode(pins::TB_V2_DOWN,  INPUT_PULLUP);
    pinMode(pins::TB_V2_LEFT,  INPUT_PULLUP);
    pinMode(pins::TB_V2_RIGHT, INPUT_PULLUP);
    pinMode(pins::TB_V2_PRESS, INPUT_PULLUP);
    delayMicroseconds(100);

    // All trackball GPIO pins should read HIGH with pullup when not pressed.
    // If they all read HIGH, trackball is likely connected.
    // This is not 100% conclusive (floating pins also read HIGH with pullup)
    // but combined with the I2C check first, it is sufficient.
    bool all_high = (digitalRead(pins::TB_V2_UP)    == HIGH &&
                     digitalRead(pins::TB_V2_DOWN)  == HIGH &&
                     digitalRead(pins::TB_V2_LEFT)  == HIGH &&
                     digitalRead(pins::TB_V2_RIGHT) == HIGH);

    // GPIO 0 is the BOOT button and reads LOW during boot.
    // After boot, it reads HIGH with pullup when not pressed.
    // Don't require it HIGH for detection (user might be pressing it).
    return all_high;
}

// Legacy probe functions — now redirect to unified probeGPIO()
bool Trackball::probeGPIO_V1() { return probeGPIO(); }
bool Trackball::probeGPIO_V2() { return probeGPIO(); }

int16_t Trackball::accelerate(int16_t raw) {
    if (raw == 0) return 0;

    int16_t absRaw = (raw > 0) ? raw : -raw;

    // Below threshold: linear (1:1, minimum 1 px)
    if (absRaw <= ACCEL_THRESHOLD) {
        int16_t result = (absRaw < ACCEL_MIN) ? ACCEL_MIN : absRaw;
        return (raw > 0) ? result : -result;
    }

    // Above threshold: quadratic curve with shift for smoother feel
    // result = (raw >> shift)^2, capped at ACCEL_MAX
    int16_t shifted = absRaw >> ACCEL_CURVE_SHIFT;
    int16_t result = shifted * shifted;
    if (result > ACCEL_MAX) result = ACCEL_MAX;
    if (result < ACCEL_MIN) result = ACCEL_MIN;

    return (raw > 0) ? result : -result;
}

void Trackball::tick() {
    switch (_type) {
        case TrackballType::GPIO_V1:     // DEPRECATED — same as GPIO_STD
        case TrackballType::GPIO_STD: {
            // Accumulate raw deltas (before acceleration)
            int16_t rawDy = (!digitalRead(_pins.down) - !digitalRead(_pins.up));
            int16_t rawDx = (!digitalRead(_pins.right) - !digitalRead(_pins.left));
            _rawDx += rawDx;
            _rawDy += rawDy;

            // Debounce press: must be held for PRESS_DEBOUNCE_TICKS consecutive ticks
            bool pressNow = !digitalRead(_pins.press);
            if (pressNow) {
                _pressCounter++;
                if (_pressCounter >= PRESS_DEBOUNCE_TICKS && !_pressRegistered) {
                    _pressRegistered = true;
                    _pressed = true;
                }
            } else {
                _pressCounter = 0;
                _pressRegistered = false;
            }
            break;
        }

        case TrackballType::I2C_OPTICAL:
            if (_wire) {
                // Read X and Y deltas from I2C sensor
                int8_t dX = readI2CDelta(*_wire, 0x02);
                int8_t dY = readI2CDelta(*_wire, 0x03);

                // Apply dead zone filter: ignore tiny deltas (sensor noise)
                // This fixes the "scrolling itself down" bug (MeshCore #1469)
                // where the AFBR S10 constantly reports small downward drift.
                if (dX > -I2C_DEAD_ZONE && dX < I2C_DEAD_ZONE) dX = 0;
                if (dY > -I2C_DEAD_ZONE && dY < I2C_DEAD_ZONE) dY = 0;

                // Drift suppression: if the sensor has been reporting movement
                // in the same direction for many consecutive ticks with no
                // user interaction (no press), it is likely sensor drift.
                // Suppress that axis until the user moves the other way.
                if (dX != 0) {
                    int8_t dir = (dX > 0) ? 1 : -1;
                    if (_driftDirX == dir) {
                        _driftCountX++;
                    } else {
                        _driftCountX = 1;
                        _driftDirX = dir;
                    }
                } else {
                    if (_driftSuppressX > 0) {
                        _driftSuppressX--;
                    }
                }

                if (dY != 0) {
                    int8_t dir = (dY > 0) ? 1 : -1;
                    if (_driftDirY == dir) {
                        _driftCountY++;
                    } else {
                        _driftCountY = 1;
                        _driftDirY = dir;
                    }
                } else {
                    if (_driftSuppressY > 0) {
                        _driftSuppressY--;
                    }
                }

                // If drift threshold reached on an axis, start suppressing
                if (_driftCountX >= I2C_DRIFT_MAX) {
                    _driftSuppressX = I2C_DRIFT_SUPPRESS_TICKS;
                    _driftCountX = 0;
                }
                if (_driftCountY >= I2C_DRIFT_MAX) {
                    _driftSuppressY = I2C_DRIFT_SUPPRESS_TICKS;
                    _driftCountY = 0;
                }

                // Suppress drift axis movement
                if (_driftSuppressX > 0) dX = 0;
                if (_driftSuppressY > 0) dY = 0;

                _rawDx += dX;
                _rawDy += dY;

                // Press detection: use GPIO 0 (BOOT/PRESS) for I2C sensor too
                bool pressNow = false;
                pinMode(pins::TB_V2_PRESS, INPUT_PULLUP);
                if (!digitalRead(pins::TB_V2_PRESS)) {
                    pressNow = true;
                }

                if (pressNow) {
                    _pressCounter++;
                    if (_pressCounter >= PRESS_DEBOUNCE_TICKS && !_pressRegistered) {
                        _pressRegistered = true;
                        _pressed = true;
                        // User is actively pressing: reset drift counters
                        _driftCountX = 0;
                        _driftCountY = 0;
                        _driftSuppressX = 0;
                        _driftSuppressY = 0;
                    }
                } else {
                    _pressCounter = 0;
                    _pressRegistered = false;
                }
            }
            break;

        case TrackballType::NONE:
            break;
    }
}

void Trackball::consumeDelta(int16_t& dx, int16_t& dy) {
    // Apply acceleration curve to raw deltas before handing to consumer
    dx = accelerate(_rawDx);
    dy = accelerate(_rawDy);
    _rawDx = 0;
    _rawDy = 0;
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