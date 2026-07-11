// OpenMeshOS — Notification.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Notification system for incoming messages and alerts.
// Uses I2S to play beep tones through the T-Deck speaker
// and wakes the screen from dim/lock on incoming messages.
//
// T-Deck audio hardware:
//   - I2S BCK  = GPIO 7
//   - I2S WS   = GPIO 5
//   - I2S DOUT = GPIO 6
//   - Board power enable = GPIO 10 (must be HIGH for audio)
//
// The speaker is driven via an I2S DAC/amplifier. We generate
// simple sine wave tones in software and push them through I2S.

#pragma once

#include <Arduino.h>

#if !defined(OMS_PLATFORM_HELTEC_V3) && !defined(ARDUINO_ARCH_NRF52840)
  #include <driver/i2s.h>
  #include "BoardTDeck.h"
#endif

namespace oms {

/// Notification sound patterns
enum class NotifyTone : uint8_t {
    MessageIn,     // single short beep
    AlertHigh,     // double beep, high priority
    AlertLow,      // long low tone
    KeyClick,      // tiny click for keypress feedback
};

/// Manages audio notifications (beeps) and screen wake-on-message.
class Notification {
public:
    static Notification& instance();

    /// Initialise I2S and audio hardware. Call once in setup().
    void init();

    /// Play a notification tone (non-blocking, returns immediately).
    /// The tone plays in the background via I2S DMA.
    void playTone(NotifyTone tone);

    /// Play a custom tone: frequency (Hz), duration (ms), volume (0-255).
    /// Non-blocking.
    void playToneCustom(uint16_t freqHz, uint16_t durationMs, uint8_t volume = 128);

    /// Play a pattern of tones. Non-blocking.
    /// tones: array of {freqHz, durationMs} pairs
    /// count: number of tone pairs
    /// volume: 0-255
    void playPattern(const uint16_t* freqHz, const uint16_t* durationMs,
                     uint8_t count, uint8_t volume = 128);

    /// Wake the screen from dim/lock state (called on incoming message).
    void wakeScreen();

    /// Set whether notification sounds are enabled.
    void setSoundEnabled(bool enabled);
    bool soundEnabled() const { return _soundEnabled; }

    /// Set whether screen wake on message is enabled.
    void setWakeEnabled(bool enabled);
    bool wakeEnabled() const { return _wakeEnabled; }

    /// Call from main loop to manage I2S cleanup after tones finish.
    void tick();

private:
    Notification() = default;

#if !defined(OMS_PLATFORM_HELTEC_V3) && !defined(ARDUINO_ARCH_NRF52840)
    /// Generate a sine wave buffer at the given frequency and volume.
    /// Returns the number of samples written.
    size_t generateSine(int16_t* buf, size_t sampleCount,
                        uint16_t freqHz, uint8_t volume, uint32_t sampleRate);

    /// Configure I2S for tone output (simplex TX, 16-bit, 8kHz).
    bool configureI2S();

    /// Stop I2S and release the driver.
    void deinitI2S();

    /// I2S port number (use port 1 to avoid conflict with other I2S users)
    static constexpr i2s_port_t kI2S_PORT = I2S_NUM_1;

    // I2S configuration (from tdeck:: namespace in BoardTDeck.h)
    static constexpr gpio_num_t PIN_I2S_BCK  = tdeck::I2S_BCK;
    static constexpr gpio_num_t PIN_I2S_WS   = tdeck::I2S_WS;
    static constexpr gpio_num_t PIN_I2S_DOUT = tdeck::I2S_DOUT;
    static constexpr gpio_num_t PIN_POWER_EN = tdeck::POWER_EN;

    // Audio parameters
    static constexpr uint32_t kSampleRate = 8000;  // 8kHz sample rate (sufficient for beeps)
    static constexpr size_t kBufferSize = 1600;     // 200ms at 8kHz (int16 mono)
#endif

    // Pre-allocated tone buffer (eliminates per-event heap allocation).
    // Allocated once in init() and reused for all tone/pattern playback.
    // On non-audio platforms (heltec-v3), this stays null and is never used.
    int16_t* _toneBuf = nullptr;

    bool _i2sRunning = false;
    bool _soundEnabled = true;
    bool _wakeEnabled = true;
    bool _playing = false;
    uint32_t _playStartMs = 0;
    uint16_t _playDurationMs = 0;
    bool _initialized = false;
};

}  // namespace oms