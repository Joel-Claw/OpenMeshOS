// OpenMeshOS — BoardHeltecV3.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// IBoard implementation for the Heltec WiFi LoRa 32 V3.
// ESP32-S3FN8 + SX1262 + SSD1306 OLED (128x64, I2C).
//
// This is a minimal board: no keyboard, no trackball, no GPS,
// no SD card, no speaker. It operates in headless/repeater mode
// or BLE companion mode.
//
// The SSD1306 OLED is used for basic status display (node name,
// channel, RSSI, battery). Full UI screens are not rendered here;
// instead, a minimal status line is shown.

#include "BoardHeltecV3.h"
#include "utils/Log.h"
#include <Arduino.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <esp_system.h>

#ifdef OMS_PLATFORM_HELTEC_V3

namespace oms {

// ── Singleton ────────────────────────────────────────────────────────
static BoardHeltecV3 s_board;

BoardHeltecV3& BoardHeltecV3::instance()
{
    return s_board;
}

// ── Init ─────────────────────────────────────────────────────────────
void BoardHeltecV3::init()
{
    OMS_LOG("board", "Heltec WiFi LoRa 32 V3 initializing");

    // 1) Configure LoRa SPI pins
    SPI.begin(heltec_v3::LORA_SCK, heltec_v3::LORA_MISO, heltec_v3::LORA_MOSI, heltec_v3::LORA_CS);

    // 2) Configure LoRa radio pins
    pinMode(heltec_v3::LORA_RST, OUTPUT);
    pinMode(heltec_v3::LORA_BUSY, INPUT);
    pinMode(heltec_v3::LORA_DIO1, INPUT);

    // 3) Configure onboard LED (inverted: LOW = ON)
    pinMode(heltec_v3::LED_PIN, OUTPUT);
    digitalWrite(heltec_v3::LED_PIN, HIGH);  // LED off (inverted)

    // 4) Configure Vext power control
    pinMode(heltec_v3::VEXT_EN, OUTPUT);
    digitalWrite(heltec_v3::VEXT_EN, HIGH);  // Enable Vext

    // 5) Configure battery ADC
    // ADC1_CH0 on GPIO1, 12-bit resolution, 3.3V reference
    // Voltage divider: Vbat = ADC_reading * 2.0
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // Full range: 0-3.3V (approx)
    pinMode(heltec_v3::BAT_ADC, INPUT);

    // 6) Configure boot button (GPIO0)
    pinMode(heltec_v3::BOOT_BTN, INPUT_PULLUP);

    // 7) Configure I2C for OLED (SSD1306)
    // Note: OLED init is handled by the display driver, not here.
    // We just configure the pins.
    pinMode(heltec_v3::OLED_RST, OUTPUT);
    // Reset OLED display
    digitalWrite(heltec_v3::OLED_RST, LOW);
    delay(10);
    digitalWrite(heltec_v3::OLED_RST, HIGH);
    delay(10);

    _initialized = true;
    OMS_LOG("board", "Heltec V3 init complete (no keyboard, no trackball, OLED on I2C)");
}

// ── Tick ─────────────────────────────────────────────────────────────
void BoardHeltecV3::tick()
{
    // Nothing to poll on Heltec V3 (no trackball, no GPS, no keyboard)
    // The boot button press detection is handled by LVGL indev or
    // by the main loop checking GPIO0 directly.
}

// ── Display ──────────────────────────────────────────────────────────
void BoardHeltecV3::setBacklight(bool on)
{
    // SSD1306 OLED has no backlight. The display is always "on" when
    // initialized. We could turn off the display to save power.
    // For now, this is a no-op.
    (void)on;
}

void BoardHeltecV3::setBrightness(int level)
{
    // SSD1306 OLED has no brightness control.
    // Could implement display on/off via sleep command, but that's
    // a different concept from brightness.
    (void)level;
}

// ── Battery ──────────────────────────────────────────────────────────
uint16_t BoardHeltecV3::batteryMilliVolts() const
{
    // ADC1_CH0 reads battery voltage through a voltage divider.
    // Heltec V3 uses a 2:1 divider (1MΩ + 1MΩ).
    // ADC range: 0-4095 (12-bit) → 0-3.3V (with ADC_11db attenuation)
    // Actual battery voltage = ADC_reading * 3.3V * 2.0 / 4095
    int raw = analogRead(heltec_v3::BAT_ADC);
    float voltage = (raw / 4095.0f) * 3.3f * _adcMultiplier;
    return static_cast<uint16_t>(voltage * 1000.0f);
}

int BoardHeltecV3::batteryPercent() const
{
    uint16_t mv = batteryMilliVolts();
    if (mv >= 4200) return 100;
    if (mv <= 3200) return 0;
    // Linear interpolation between 3.2V and 4.2V
    return (mv - 3200) / 10;
}

float BoardHeltecV3::mcuTemperature() const {
    return temperatureRead();  // ESP32 internal temp sensor
}

// ── Power ────────────────────────────────────────────────────────────
void BoardHeltecV3::reboot()
{
    OMS_LOG("board", "Rebooting Heltec V3");
    ESP.restart();
}

void BoardHeltecV3::powerOff()
{
    OMS_LOG("board", "Entering deep sleep (power off)");
    // Disable Vext to save power
    digitalWrite(heltec_v3::VEXT_EN, LOW);
    // Enter deep sleep with wake on BOOT button (GPIO0)
    esp_sleep_enable_ext0_wakeup(heltec_v3::BOOT_BTN, 0);  // LOW = wake
    esp_deep_sleep_start();
}

uint32_t BoardHeltecV3::resetReason() const
{
    return static_cast<uint32_t>(esp_reset_reason());
}

// ── Heltec V3 specific ───────────────────────────────────────────────
void BoardHeltecV3::setVext(bool on)
{
    digitalWrite(heltec_v3::VEXT_EN, on ? HIGH : LOW);
}

void BoardHeltecV3::setLed(bool on)
{
    // LED is inverted on Heltec V3: LOW = ON, HIGH = OFF
    digitalWrite(heltec_v3::LED_PIN, on ? LOW : HIGH);
}

}  // namespace oms

#endif  // OMS_PLATFORM_HELTEC_V3