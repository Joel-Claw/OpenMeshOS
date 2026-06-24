// OpenMeshOS — BoardRAK4631.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// IBoard implementation for the RAK WisBlock RAK4631.
// nRF52840 MCU + SX1262 LoRa + optional SSD1306 OLED (128x64, I2C).
//
// This is a low-power board: no keyboard, no trackball, no GPS,
// no SD card, no speaker. It operates in headless/repeater mode
// or BLE companion mode. The nRF52840 excels at low-power BLE
// applications, making this board ideal for solar-powered nodes.
//
// The optional SSD1306 OLED is used for basic status display (node name,
// channel, RSSI, battery). Full UI screens are not rendered here;
// instead, a minimal status line is shown.

#include "BoardRAK4631.h"
#include "utils/Log.h"

#ifdef OMS_PLATFORM_RAK4631

#include <Arduino.h>
#include <SPI.h>

// nRF52-specific headers (not available on ESP32)
#ifdef ARDUINO_ARCH_NRF52840
  #include <nrf.h>
  #include <nrf_power.h>
  #include <nrf_saadc.h>
  #include <nrf_temp.h>
#endif

namespace oms {

// ── Singleton ────────────────────────────────────────────────────────
static BoardRAK4631 s_board;

BoardRAK4631& BoardRAK4631::instance()
{
    return s_board;
}

// ── Init ─────────────────────────────────────────────────────────────
void BoardRAK4631::init()
{
    OMS_LOG("board", "RAK WisBlock RAK4631 initializing");

    // 1) Enable LoRa module power (SX126X_POWER_EN)
    pinMode(rak4631::LORA_POWER_EN, OUTPUT);
    digitalWrite(rak4631::LORA_POWER_EN, HIGH);
    delay(10);  // Allow power to stabilise

    // 2) Configure LoRa SPI pins
    SPI.begin(rak4631::LORA_SCK, rak4631::LORA_MISO, rak4631::LORA_MOSI, rak4631::LORA_CS);

    // 3) Configure LoRa radio control pins
    pinMode(rak4631::LORA_RST, OUTPUT);
    pinMode(rak4631::LORA_BUSY, INPUT);
    pinMode(rak4631::LORA_DIO1, INPUT);

    // 4) Configure LEDs
    pinMode(rak4631::LED_BLUE, OUTPUT);
    pinMode(rak4631::LED_GREEN, OUTPUT);
    digitalWrite(rak4631::LED_BLUE, LOW);   // LED off (active HIGH on nRF52)
    digitalWrite(rak4631::LED_GREEN, LOW);

    // 5) Configure battery ADC (nRF52 SAADC)
    // RAK5005-O uses AIN3 (P0.05) for battery voltage via divider
    // nRF52 SAADC: 12-bit resolution, 3.6V reference with 1/6 gain
    // Vbat = ADC_reading * 3.6V * divider_ratio / 4095
    // We use analogRead() which is available in the nRF52 Arduino core
    analogReadResolution(12);  // nRF52 SAADC supports up to 12-bit
    // nRF52 default reference is VDD/4 with 1/6 gain → 3.6V full scale
    pinMode(rak4631::BAT_ADC, INPUT);

    // 6) Configure user button (WB_SW1, if connected)
    pinMode(rak4631::USER_BTN, INPUT_PULLUP);

    // 7) Configure I2C for optional OLED (SSD1306)
    // OLED init is handled by the display driver, not here.
    // RAK4631 OLED has no reset pin.

    _initialized = true;
    OMS_LOG("board", "RAK4631 init complete (no keyboard, no trackball, OLED on I2C)");
}

// ── Tick ─────────────────────────────────────────────────────────────
void BoardRAK4631::tick()
{
    // Nothing to poll on RAK4631 (no trackball, no GPS, no keyboard)
    // The user button press detection is handled by the main loop
    // or by LVGL indev if a display is connected.
}

// ── Display ──────────────────────────────────────────────────────────
void BoardRAK4631::setBacklight(bool on)
{
    // SSD1306 OLED has no backlight. The display is always "on" when
    // initialized. We could turn off the display to save power via
    // SSD1306 sleep command, but that's handled at the display driver level.
    (void)on;
}

void BoardRAK4631::setBrightness(int level)
{
    // SSD1306 OLED has no brightness control.
    // Could implement display on/off via sleep command, but that's
    // a different concept from brightness.
    (void)level;
}

// ── Battery ──────────────────────────────────────────────────────────
uint16_t BoardRAK4631::batteryMilliVolts() const
{
    // nRF52 SAADC reads battery voltage through a voltage divider.
    // RAK5005-O baseboard uses a 1.5x divider (1MΩ + 2MΩ).
    // ADC range: 0-4095 (12-bit) → 0-3.6V (with internal reference)
    // Actual battery voltage = ADC_reading * 3.6V * _adcMultiplier / 4095
    int raw = analogRead(rak4631::BAT_ADC);
    float voltage = (raw / 4095.0f) * 3.6f * _adcMultiplier;
    return static_cast<uint16_t>(voltage * 1000.0f);
}

int BoardRAK4631::batteryPercent() const
{
    uint16_t mv = batteryMilliVolts();
    if (mv >= 4200) return 100;
    if (mv <= 3200) return 0;
    // Linear interpolation between 3.2V and 4.2V
    return (mv - 3200) / 10;
}

float BoardRAK4631::mcuTemperature() const
{
    // nRF52 has an internal temperature sensor (TEMP register)
    // This reads the die temperature, which is higher than ambient.
    // The nRF52 Arduino core provides this via temp_measure() or
    // we can read the NRF_TEMP->TEMP register directly.
#ifdef ARDUINO_ARCH_NRF52840
    // Trigger temperature measurement
    NRF_TEMP->TASKS_START = 1;
    // Wait for measurement to complete
    while (NRF_TEMP->EVENTS_DATARDY == 0) {
        // Busy wait — nRF52 temp measurement takes ~36μs
    }
    NRF_TEMP->EVENTS_DATARDY = 0;
    // Temperature in 0.25°C units
    int32_t raw = NRF_TEMP->TEMP;
    NRF_TEMP->TASKS_STOP = 1;
    return static_cast<float>(raw) / 4.0f;
#else
    return 0.0f;  // Not on nRF52 platform
#endif
}

// ── Power ────────────────────────────────────────────────────────────
void BoardRAK4631::reboot()
{
    OMS_LOG("board", "Rebooting RAK4631");
    NVIC_SystemReset();
}

void BoardRAK4631::powerOff()
{
    OMS_LOG("board", "Entering SYSTEMOFF (power off)");
    // Disable LoRa power to save energy
    digitalWrite(rak4631::LORA_POWER_EN, LOW);
    // Turn off LEDs
    digitalWrite(rak4631::LED_BLUE, LOW);
    digitalWrite(rak4631::LED_GREEN, LOW);
    // nRF52 SYSTEMOFF: lowest power state, wake on GPIO or RTC
    // We wake on user button (WB_SW1) if connected
    // Configure GPIO sense on the user button pin
#ifdef ARDUINO_ARCH_NRF52840
    nrf_gpio_cfg_sense_input(rak4631::USER_BTN,
                              NRF_GPIO_PIN_PULLUP,
                              NRF_GPIO_PIN_SELOW);
#endif
    // Enter system off
#ifdef ARDUINO_ARCH_NRF52840
    NRF_POWER->SYSTEMOFF = POWER_SYSTEMOFF_SYSTEMOFF_Enter;
#endif
    // Should never reach here
}

uint32_t BoardRAK4631::resetReason() const
{
    // nRF52 reset reasons are in the RESETREAS register
#ifdef ARDUINO_ARCH_NRF52840
    return NRF_POWER->RESETREAS;
#else
    return 0;
#endif
}

// ── RAK4631 specific ─────────────────────────────────────────────────
void BoardRAK4631::setLoRaPower(bool on)
{
    digitalWrite(rak4631::LORA_POWER_EN, on ? HIGH : LOW);
}

void BoardRAK4631::setLed(bool on)
{
    // LED is active HIGH on nRF52 (non-inverted, unlike Heltec V3)
    digitalWrite(rak4631::LED_BLUE, on ? HIGH : LOW);
}

void BoardRAK4631::setGreenLed(bool on)
{
    digitalWrite(rak4631::LED_GREEN, on ? HIGH : LOW);
}

void BoardRAK4631::setIOPower(bool on)
{
    // WisBlock IO slot power is controlled via WB_IO1 (shared with GPS 1PPS)
    // In practice, IO slot power is always on when the baseboard is powered.
    // This is a placeholder for future power management.
    (void)on;
}

}  // namespace oms

#endif  // OMS_PLATFORM_RAK4631