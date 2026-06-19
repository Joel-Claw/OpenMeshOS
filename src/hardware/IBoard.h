// OpenMeshOS — IBoard.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Hardware abstraction interface for multi-device support.
// Each supported device implements this interface to provide
// pin mappings, peripheral access, and device-specific init.
//
// Phase 4 roadmap item: "Abstract Board interface"
// Board.h becomes BoardTDeck.h (T-Deck specific impl of IBoard).
// New targets (Heltec V3, RAK WisBlock, etc.) each get their own impl.
//
// The IBoard interface is deliberately minimal: it exposes only what
// the mesh, UI, and power subsystems need. Hardware-specific details
// (exact pin numbers, SPI bus assignments) stay in the implementation.

#pragma once

#include <Arduino.h>
#include <cstdint>

namespace oms {

// Forward declarations for concrete input drivers.
// These are board-specific types but all supported boards currently
// share the same BBQ10KB keyboard and GPIO/I2C trackball drivers.
// When we add boards with different input hardware, we'll introduce
// abstract IKeyboard/ITrackball interfaces and migrate these accessors.
class Keyboard;
class Trackball;

// ── Screen dimensions (set at compile time per target) ────────────
// These are defined via build flags (DOMS_SCREEN_W, OMS_SCREEN_H)
// but also available through IBoard for runtime queries.

/// Board capabilities flags — each implementation reports what it has
struct BoardCaps {
    bool hasKeyboard      : 1;  // BBQ10KB or similar I2C keyboard
    bool hasTrackball     : 1;  // Trackball or rotary encoder
    bool hasGPS           : 1;  // Built-in GPS module
    bool hasSDCard        : 1;  // SD card slot
    bool hasBLE           : 1;  // BLE radio (ESP32-S3 always has this)
    bool hasSpeaker       : 1;  // I2S speaker/buzzer
    bool hasTouchScreen   : 1;  // Touch overlay on display
    bool hasBatteryADC    : 1;  // Battery voltage ADC
    bool hasLoRa          : 1;  // LoRa radio (SX1262/SX1276)
};

/// LoRa radio configuration — each board provides its own
struct LoRaConfig {
    float    freqMHz;      // Centre frequency (e.g. 868.0)
    float    bwMHz;        // Bandwidth (e.g. 125.0)
    uint8_t  sf;           // Spreading factor (7-12)
    uint8_t  cr;           // Coding rate (5-8)
    int8_t   txPower;      // TX power in dBm (5-22 for SX1262)
    uint8_t  csPin;        // Chip select GPIO
    uint8_t  dio1Pin;      // DIO1 interrupt GPIO
    uint8_t  rstPin;       // Reset GPIO
    uint8_t  busyPin;      // Busy GPIO
    uint8_t  sckPin;       // SPI clock GPIO
    uint8_t  misoPin;      // SPI MISO GPIO
    uint8_t  mosiPin;      // SPI MOSI GPIO
};

/// Display configuration — each board provides its own
struct DisplayConfig {
    uint16_t width;         // Pixels (e.g. 320)
    uint16_t height;        // Pixels (e.g. 240)
    uint8_t  csPin;         // Chip select GPIO
    uint8_t  dcPin;         // Data/Command GPIO
    int8_t   rstPin;        // Reset GPIO (-1 = none)
    uint8_t  blPin;         // Backlight GPIO
    uint8_t  sckPin;        // SPI clock GPIO
    uint8_t  mosiPin;       // SPI MOSI GPIO
    uint32_t spiFreq;       // SPI frequency in Hz
};

/// IBoard — abstract hardware interface for OpenMeshOS
///
/// Usage:
///   IBoard* board = BoardFactory::create();
///   board->init();
///   // ... throughout the app:
///   board->tick();
///   float lat = board->gpsLat();
///
/// Concrete implementations:
///   - BoardTDeck   (LilyGo T-Deck / T-Deck Plus)
///   - (Future: BoardHeltecV3, BoardRAKWisBlock, etc.)
///
class IBoard {
public:
    virtual ~IBoard() = default;

    // ── Lifecycle ──────────────────────────────────────────────────
    /// Initialise all hardware peripherals. Call once in setup().
    virtual void init() = 0;

    /// Poll peripherals (trackball, GPS, etc.). Call once per loop().
    virtual void tick() = 0;

    // ── Identity ───────────────────────────────────────────────────
    /// Human-readable board name (e.g. "LilyGo T-Deck")
    virtual const char* boardName() const = 0;

    /// Board capabilities (what hardware this device has)
    virtual BoardCaps capabilities() const = 0;

    // ── Display ────────────────────────────────────────────────────
    /// Display configuration (pins, dimensions, SPI freq)
    virtual DisplayConfig displayConfig() const = 0;

    /// Set display backlight on/off
    virtual void setBacklight(bool on) = 0;

    /// Set display brightness (0-255)
    virtual void setBrightness(int level) = 0;

    // ── LoRa Radio ────────────────────────────────────────────────
    /// LoRa radio configuration (pins, frequency, TX power)
    virtual LoRaConfig loraConfig() const = 0;

    // ── GPS ────────────────────────────────────────────────────────
    /// Whether we have a GPS fix
    virtual bool hasGPSFix() const = 0;

    /// GPS latitude (degrees, 0.0 if no fix)
    virtual float gpsLat() const = 0;

    /// GPS longitude (degrees, 0.0 if no fix)
    virtual float gpsLng() const = 0;

    /// GPS altitude (meters, 0.0 if no fix)
    virtual float gpsAltitude() const = 0;

    /// GPS speed (km/h, 0.0 if no fix)
    virtual float gpsSpeed() const = 0;

    /// GPS course (degrees, 0.0 if no fix)
    virtual float gpsCourse() const = 0;

    /// Number of GPS satellites (0 if no fix)
    virtual int gpsSatellites() const = 0;

    /// Milliseconds since last GPS fix (UINT32_MAX if never)
    virtual uint32_t gpsAge() const = 0;

    // ── Battery ───────────────────────────────────────────────────
    /// Battery voltage in millivolts (0 if no battery ADC)
    virtual uint16_t batteryMilliVolts() const = 0;

    /// Battery percentage estimate (0-100, -1 if unknown)
    virtual int batteryPercent() const = 0;

    /// MCU internal temperature in degrees Celsius
    virtual float mcuTemperature() const = 0;

    // ── Power ──────────────────────────────────────────────────────
    /// Reboot the device
    virtual void reboot() = 0;

    /// Power off the device (deep sleep if supported)
    virtual void powerOff() = 0;

    /// Get the ESP32 reset reason
    virtual uint32_t resetReason() const = 0;

    // ── Input (delegates to concrete drivers) ──────────────────────
    /// Whether a physical keyboard is present
    virtual bool hasKeyboard() const = 0;

    /// Consume a trackball/encoder press event (returns false if none)
    virtual bool consumeTrackballPress() = 0;

    /// Consume trackball/encoder delta since last call
    virtual void consumeTrackballDelta(int16_t& dx, int16_t& dy) = 0;

    // ── Input drivers (board-specific) ───────────────────────────────
    // Direct access to keyboard and trackball drivers.
    // These return concrete types because we only support T-Deck boards
    // right now. When we add boards with different input hardware,
    // these will be replaced with abstract interface accessors.
    //
    // For now, callers should prefer IBoard virtual methods
    // (consumeTrackballPress, consumeTrackballDelta, hasKeyboard)
    // and only use these direct-access methods when the LVGL input
    // bridge or other low-level code needs the concrete driver.
    virtual Keyboard& keyboard() = 0;
    virtual Trackball& trackball() = 0;

    // ── Screen dimensions ──────────────────────────────────────────
    /// Screen width in pixels (convenience accessor)
    uint16_t screenWidth() const { return displayConfig().width; }

    /// Screen height in pixels (convenience accessor)
    uint16_t screenHeight() const { return displayConfig().height; }
};

// ── Global board accessor ─────────────────────────────────────────
// Returns the singleton IBoard* created by BoardFactory::create().
// Call this instead of Board::instance() or BoardTDeck::instance().
// Available after setup() calls board->init().
IBoard* theBoard();

// ── Board Factory ──────────────────────────────────────────────────
// Creates the correct IBoard implementation based on build flags.
// This isolates the rest of the codebase from knowing which
// concrete board class to instantiate.

namespace BoardFactory {
    /// Create the board instance for this build target.
    /// Returns a heap-allocated IBoard; caller must not delete
    /// (it lives for the lifetime of the app).
    IBoard* create();
}

}  // namespace oms