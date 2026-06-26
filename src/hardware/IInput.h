// OpenMeshOS — IInput.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Abstract input device interface for multi-device support.
//
// Not all devices have a keyboard and trackball. The Heltec V3 and
// RAK WisBlock have no keyboard and no trackball — they rely on
// BLE companion app input or serial CLI.
//
// This interface abstracts the input sources so the UI and mesh
// layers can query input capabilities and poll events without
// knowing the concrete hardware.
//
// Concrete implementations:
//   - InputTDeck    (BBQ10KB keyboard + GPIO/I2C trackball)
//   - InputSerial   (serial CLI input — fallback for headless boards)
//   - InputNone     (no input — for pure repeater nodes)
//
// Phase 4 roadmap item: "Input abstraction (some devices have no
// keyboard, only touch)"

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

/// Input event types
enum class InputEventType : uint8_t {
    None,       ///< No event (sentinel)
    KeyPress,   ///< Key was pressed
    KeyRelease, ///< Key was released
    KeyLongPress, ///< Key was held
    EncoderDelta, ///< Trackball/encoder moved
    EncoderPress, ///< Trackball/encoder clicked
    TouchTap,   ///< Touch screen tap
    TouchLongPress, ///< Touch screen long press
};

/// Input event — unified event from any input source
struct InputEvent {
    InputEventType type;

    // Key data (for KeyPress/KeyRelease/KeyLongPress)
    char    key;        ///< ASCII character (0 if modifier/special)
    uint8_t keyCode;    ///< Raw key code
    uint8_t modifiers;  ///< Bitmask: Shift=1, Ctrl=2, Alt=4, Sym=8

    // Encoder/trackball data (for EncoderDelta)
    int16_t deltaX;     ///< Horizontal delta (pixels)
    int16_t deltaY;     ///< Vertical delta (pixels)

    // Touch data (for TouchTap/TouchLongPress)
    int16_t touchX;     ///< Touch X coordinate
    int16_t touchY;     ///< Touch Y coordinate

    // Timestamp (millis() when event was generated)
    uint32_t timestamp;
};

/// Input capabilities — what input methods this device supports
struct InputCaps {
    bool hasKeyboard;     ///< Physical keyboard
    bool hasTrackball;    ///< Trackball / rotary encoder
    bool hasTouch;        ///< Touch screen
    bool hasSerial;       ///< Serial CLI input
    uint8_t maxKeyEvents; ///< Max buffered key events (0 if no keyboard)
};

/// IInput — abstract input interface
///
/// Usage:
///   IInput* in = theBoard()->input();
///   InputEvent evt;
///   while (in->pollEvent(evt)) {
///       // handle evt
///   }
///
class IInput {
public:
    virtual ~IInput() = default;

    // ── Lifecycle ──────────────────────────────────────────────────

    /// Initialise input hardware (I2C keyboard, GPIO trackball, etc.)
    virtual void begin() = 0;

    /// Poll input devices. Call once per loop() iteration.
    /// This reads hardware and enqueues events.
    virtual void tick() = 0;

    // ── Properties ─────────────────────────────────────────────────

    /// Input capabilities (what this device supports)
    virtual InputCaps capabilities() const = 0;

    /// True if device has a physical keyboard
    bool hasKeyboard() const { return capabilities().hasKeyboard; }

    /// True if device has a trackball/encoder
    bool hasTrackball() const { return capabilities().hasTrackball; }

    /// True if device has a touch screen
    bool hasTouch() const { return capabilities().hasTouch; }

    // ── Event Queue ────────────────────────────────────────────────

    /// Poll for next input event. Returns true if an event was available.
    /// Call in a loop until it returns false.
    virtual bool pollEvent(InputEvent& evt) = 0;

    /// Number of events waiting in the queue
    virtual size_t eventCount() const = 0;

    /// Clear all pending events
    virtual void clearEvents() = 0;

    // ── Convenience Accessors ──────────────────────────────────────

    /// Consume trackball/encoder press (returns false if none pending)
    virtual bool consumePress() = 0;

    /// Consume trackball/encoder delta since last call
    virtual void consumeDelta(int16_t& dx, int16_t& dy) = 0;

    // ── Modifier State ─────────────────────────────────────────────

    /// Get current modifier key state (bitmask: Shift=1, Ctrl=2, Alt=4, Sym=8)
    virtual uint8_t modifiers() const = 0;

    /// True if Shift is held
    bool shiftHeld() const { return modifiers() & 0x01; }

    /// True if Ctrl is held
    bool ctrlHeld() const { return modifiers() & 0x02; }

    /// True if Alt is held
    bool altHeld() const { return modifiers() & 0x04; }

    /// True if Sym is held
    bool symHeld() const { return modifiers() & 0x08; }
};

}  // namespace oms