// OpenMeshOS — TDeckClock.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Keeps time by tracking when epoch was last set + millis() offset.
// GPS or NTP updates correct drift periodically.

#include "TDeckClock.h"
#include <Arduino.h>

namespace oms {

TDeckClock::TDeckClock() {
    // Time starts unknown until GPS/NTP/manual set
}

uint32_t TDeckClock::getCurrentTime() {
    if (_epoch == 0) return 0;

    // Calculate current time from last known epoch + elapsed millis
    uint32_t elapsed = (millis() - _millisAtEpoch) / 1000;
    return _epoch + elapsed;
}

void TDeckClock::setCurrentTime(uint32_t time) {
    _epoch = time;
    _millisAtEpoch = millis();
}

void TDeckClock::tick() {
    // No periodic drift correction needed — we recalculate from millis()
    // each time getCurrentTime() is called. Override if RTC hardware exists.
}

void TDeckClock::onGpsTime(uint32_t gpsEpoch) {
    setCurrentTime(gpsEpoch);
}

void TDeckClock::onNtpTime(uint32_t ntpEpoch) {
    setCurrentTime(ntpEpoch);
}

}  // namespace oms