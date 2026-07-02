// OpenMeshOS — MeshClock.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore RTCClock implementation, platform-agnostic.
// Replaces the old TDeckClock with a generic implementation that
// works on all platforms (ESP32, nRF52, etc.).
//
// Time tracking: uses millis() as the drift baseline.
// Time sources (any or all):
//   - GPS time sync (T-Deck Plus, external GPS)
//   - NTP sync (ESP32 WiFi, not available on nRF52)
//   - BLE companion app time sync
//   - Manual set via settings screen

#pragma once

#include <MeshCore.h>
#include <cstdint>

namespace oms {

class MeshClock : public mesh::RTCClock {
public:
    MeshClock();

    uint32_t getCurrentTime() override;
    void setCurrentTime(uint32_t time) override;
    void tick() override;

    // Called when GPS gets a valid timestamp
    void onGpsTime(uint32_t gpsEpoch);

    // Called when NTP sync completes
    void onNtpTime(uint32_t ntpEpoch);

    // Called when BLE companion app sends time
    void onBleTime(uint32_t bleEpoch);

    bool hasTime() const { return _epoch != 0; }

private:
    uint32_t _epoch = 0;        // Last known UNIX epoch
    uint32_t _millisAtEpoch = 0; // millis() when _epoch was set
};

}  // namespace oms