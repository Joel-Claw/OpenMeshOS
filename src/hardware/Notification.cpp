// OpenMeshOS — Notification.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Audio notification system for T-Deck.
// Plays beep tones via I2S through the built-in speaker
// and wakes the screen on incoming messages.

#include "Notification.h"
#include "IBoard.h"
#include "../ui/ScreenLock.h"
#include "../utils/Config.h"
#include "../utils/Log.h"

#include <driver/i2s.h>
#include <cmath>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
Notification& Notification::instance() {
    static Notification s_notif;
    return s_notif;
}

// ── Pre-computed sine wave for beep generation ────────────────────
// We generate tones on the fly using sinf(). On ESP32-S3 this is fast
// enough for short notification beeps.

// ── init ───────────────────────────────────────────────────────────
void Notification::init() {
    OMS_LOG("Notif", "Initialising notification system");

    // Ensure board power is on (needed for audio amp)
    pinMode(PIN_POWER_EN, OUTPUT);
    digitalWrite(PIN_POWER_EN, HIGH);

    // Read sound/wake preferences from config
    _soundEnabled = config::get().notifySound;
    _wakeEnabled = true;

#ifndef OMS_PLATFORM_HELTEC_V3
    // Pre-allocate tone buffer once (eliminates per-event heap allocation).
    // kBufferSize * sizeof(int16_t) = 3200 bytes — small, lives for app lifetime.
    if (!_toneBuf) {
        _toneBuf = (int16_t*)heap_caps_malloc(kBufferSize * sizeof(int16_t), MALLOC_CAP_8BIT);
        if (!_toneBuf) {
            OMS_LOG("Notif", "WARNING: Failed to pre-allocate tone buffer, will use per-call malloc");
        } else {
            OMS_LOG("Notif", "Tone buffer pre-allocated: %u bytes",
                    (unsigned)(kBufferSize * sizeof(int16_t)));
        }
    }
#endif

    _initialized = true;

    OMS_LOG("Notif", "Sound: %s, Wake: %s",
            _soundEnabled ? "on" : "off",
            _wakeEnabled ? "on" : "off");
}

// ── I2S configuration ─────────────────────────────────────────────
bool Notification::configureI2S() {
    if (_i2sRunning) return true;  // already configured

    i2s_config_t i2sConfig = {};
    i2sConfig.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2sConfig.sample_rate           = kSampleRate;
    i2sConfig.bits_per_sample       = I2S_BITS_PER_SAMPLE_16BIT;
    i2sConfig.channel_format        = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2sConfig.communication_format  = I2S_COMM_FORMAT_STAND_I2S;
    i2sConfig.intr_alloc_flags      = ESP_INTR_FLAG_LEVEL1;
    i2sConfig.dma_buf_count         = 4;
    i2sConfig.dma_buf_len           = 256;
    i2sConfig.use_apll              = false;
    i2sConfig.tx_desc_auto_clear    = true;
    i2sConfig.fixed_mclk            = 0;

    i2s_pin_config_t pinConfig = {};
    pinConfig.bck_io_num           = (int)PIN_I2S_BCK;
    pinConfig.ws_io_num            = (int)PIN_I2S_WS;
    pinConfig.data_out_num         = (int)PIN_I2S_DOUT;
    pinConfig.data_in_num          = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(kI2S_PORT, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        OMS_LOG("Notif", "I2S driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    err = i2s_set_pin(kI2S_PORT, &pinConfig);
    if (err != ESP_OK) {
        OMS_LOG("Notif", "I2S set pin failed: %s", esp_err_to_name(err));
        i2s_driver_uninstall(kI2S_PORT);
        return false;
    }

    // Clear DMA buffers
    i2s_zero_dma_buffer(kI2S_PORT);

    _i2sRunning = true;
    return true;
}

void Notification::deinitI2S() {
    if (!_i2sRunning) return;
    i2s_driver_uninstall(kI2S_PORT);
    _i2sRunning = false;
}

// ── Sine wave generation ──────────────────────────────────────────
size_t Notification::generateSine(int16_t* buf, size_t sampleCount,
                                   uint16_t freqHz, uint8_t volume,
                                   uint32_t sampleRate) {
    // Generate mono 16-bit signed samples
    // Volume: 0 = silent, 255 = max (maps to 0..32767)
    float amplitude = (volume / 255.0f) * 32767.0f;
    float phaseStep = 2.0f * M_PI * (float)freqHz / (float)sampleRate;

    for (size_t i = 0; i < sampleCount; i++) {
        float phase = phaseStep * (float)i;
        buf[i] = (int16_t)(sinf(phase) * amplitude);
    }

    return sampleCount;
}

// ── Tone playback ──────────────────────────────────────────────────
void Notification::playTone(NotifyTone tone) {
    if (!_soundEnabled) return;

    switch (tone) {
        case NotifyTone::MessageIn:
            // Short pleasant beep: 880Hz, 80ms
            playToneCustom(880, 80, 100);
            break;
        case NotifyTone::AlertHigh:
            // Double beep: 1200Hz, 100ms, pause, 1200Hz, 100ms
            playPattern(
                (const uint16_t[]){1200, 1200},
                (const uint16_t[]){100, 100},
                2, 140);
            break;
        case NotifyTone::AlertLow:
            // Long low tone: 440Hz, 300ms
            playToneCustom(440, 300, 120);
            break;
        case NotifyTone::KeyClick:
            // Tiny click: 2000Hz, 15ms
            playToneCustom(2000, 15, 60);
            break;
    }
}

void Notification::playToneCustom(uint16_t freqHz, uint16_t durationMs, uint8_t volume) {
    if (!_soundEnabled) return;
    if (freqHz == 0 || durationMs == 0) return;

    // Don't interrupt an ongoing tone
    if (_playing) return;

    if (!configureI2S()) return;

    // Ensure board power is on for audio amplifier
    digitalWrite(PIN_POWER_EN, HIGH);

    // Calculate number of samples needed
    size_t sampleCount = (kSampleRate * durationMs) / 1000;
    if (sampleCount > kBufferSize) sampleCount = kBufferSize;

    // Use pre-allocated buffer if available, otherwise fall back to malloc
    int16_t* buf = _toneBuf;
    bool bufWasAllocated = false;
    if (!buf) {
        buf = (int16_t*)heap_caps_malloc(kBufferSize * sizeof(int16_t), MALLOC_CAP_8BIT);
        bufWasAllocated = true;
        if (!buf) {
            OMS_LOG("Notif", "Failed to allocate tone buffer");
            return;
        }
    }

    // Generate sine wave
    generateSine(buf, sampleCount, freqHz, volume, kSampleRate);

    // I2S wants stereo 32-bit frames (16-bit left channel, 16-bit right channel)
    // For mono, we duplicate left to right
    // But since we configured ONLY_LEFT, we can just write mono 16-bit samples
    size_t bytesWritten = 0;
    esp_err_t err = i2s_write(kI2S_PORT, buf, sampleCount * sizeof(int16_t),
                               &bytesWritten, pdMS_TO_TICKS(200));
    if (err != ESP_OK) {
        OMS_LOG("Notif", "I2S write failed: %s", esp_err_to_name(err));
    }

    if (bufWasAllocated) free(buf);

    // Track that we're playing (for tick() to know when to deinit I2S)
    _playing = true;
    _playStartMs = millis();
    _playDurationMs = durationMs;
}

void Notification::playPattern(const uint16_t* freqHz, const uint16_t* durationMs,
                                uint8_t count, uint8_t volume) {
    if (!_soundEnabled) return;
    if (!configureI2S()) return;

    // Ensure board power is on
    digitalWrite(PIN_POWER_EN, HIGH);

    int16_t* buf = _toneBuf;
    bool bufWasAllocated = false;
    if (!buf) {
        buf = (int16_t*)heap_caps_malloc(kBufferSize * sizeof(int16_t), MALLOC_CAP_8BIT);
        bufWasAllocated = true;
        if (!buf) {
            OMS_LOG("Notif", "Failed to allocate pattern buffer");
            return;
        }
    }

    uint32_t totalDuration = 0;

    for (uint8_t i = 0; i < count; i++) {
        size_t sampleCount = (kSampleRate * durationMs[i]) / 1000;
        if (sampleCount > kBufferSize) sampleCount = kBufferSize;

        generateSine(buf, sampleCount, freqHz[i], volume, kSampleRate);

        size_t bytesWritten = 0;
        i2s_write(kI2S_PORT, buf, sampleCount * sizeof(int16_t),
                  &bytesWritten, pdMS_TO_TICKS(500));

        totalDuration += durationMs[i];

        // 50ms pause between tones
        if (i < count - 1) {
            delay(50);
            totalDuration += 50;
        }
    }

    if (bufWasAllocated) free(buf);

    _playing = true;
    _playStartMs = millis();
    _playDurationMs = totalDuration;
}

// ── Screen wake ────────────────────────────────────────────────────
void Notification::wakeScreen() {
    if (!_wakeEnabled) return;

    // Wake screen from dim/lock
    if (ui::ScreenLock::isActive() || ui::ScreenLock::isDimmed()) {
        ui::ScreenLock::resetIdleTimer();
        // If screen was dimmed (not fully locked), restore backlight
        oms::theBoard()->setBacklight(true);
    }
}

void Notification::setSoundEnabled(bool enabled) {
    _soundEnabled = enabled;
    // Persist to config
    const_cast<Config&>(config::get()).notifySound = enabled;
}

void Notification::setWakeEnabled(bool enabled) {
    _wakeEnabled = enabled;
}

// ── tick ───────────────────────────────────────────────────────────
void Notification::tick() {
    // After a tone finishes playing, clean up I2S to save power
    if (_playing) {
        if (millis() - _playStartMs > _playDurationMs + 100) {
            _playing = false;
            // Keep I2S driver installed for quick re-use
            // (deinit would save power but adds latency on next beep)
        }
    }
}

}  // namespace oms