// OpenMeshOS — MeshClock.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Keeps time by tracking when epoch was last set + millis() offset.
// GPS, NTP, or BLE updates correct drift periodically.
// Works on any platform that has Arduino's millis().

#include "MeshClock.h"
#include <Arduino.h>

namespace oms {

MeshClock::MeshClock() {
    // Time starts unknown until GPS/NTP/BLE/manual set
}

uint32_t MeshClock::getCurrentTime() {
    if (_epoch == 0) return 0;

    // Calculate current time from last known epoch + elapsed millis
    uint32_t elapsed = (millis() - _millisAtEpoch) / 1000;
    return _epoch + elapsed;
}

void MeshClock::setCurrentTime(uint32_t time) {
    _epoch = time;
    _millisAtEpoch = millis();
}

void MeshClock::tick() {
    // No periodic drift correction needed - we recalculate from millis()
    // each time getCurrentTime() is called. Override if RTC hardware exists.
}

void MeshClock::onGpsTime(uint32_t gpsEpoch) {
    setCurrentTime(gpsEpoch);
}

void MeshClock::onNtpTime(uint32_t ntpEpoch) {
    setCurrentTime(ntpEpoch);
}

void MeshClock::onBleTime(uint32_t bleEpoch) {
    setCurrentTime(bleEpoch);
}

}  // namespace oms