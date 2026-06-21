// OpenMeshOS — NotificationStub.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Headless stub for Notification on platforms without audio hardware
// (e.g. Heltec WiFi LoRa 32 V3). Provides the same API as Notification.h
// but all methods are no-ops, allowing mesh code to link without the
// full I2S audio implementation.

#include "Notification.h"

namespace oms {

Notification& Notification::instance() {
    static Notification s_instance;
    return s_instance;
}

void Notification::init() {}
void Notification::playTone(NotifyTone) {}
void Notification::playToneCustom(uint16_t, uint16_t, uint8_t) {}
void Notification::playPattern(const uint16_t*, const uint16_t*, uint8_t, uint8_t) {}
void Notification::wakeScreen() {}
void Notification::setSoundEnabled(bool) {}
void Notification::tick() {}

}  // namespace oms