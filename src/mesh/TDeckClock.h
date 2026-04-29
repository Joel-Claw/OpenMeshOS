// OpenMeshOS — TDeckClock.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore RTCClock implementation for T-Deck.
// Uses GPS time when available, falls back to NTP or manual set.

#pragma once

#include <MeshCore.h>
#include <cstdint>

namespace oms {

class TDeckClock : public mesh::RTCClock {
public:
    TDeckClock();

    uint32_t getCurrentTime() override;
    void setCurrentTime(uint32_t time) override;
    void tick() override;

    // Called when GPS gets a valid timestamp
    void onGpsTime(uint32_t gpsEpoch);

    // Called when NTP sync completes
    void onNtpTime(uint32_t ntpEpoch);

    bool hasTime() const { return _epoch != 0; }

private:
    uint32_t _epoch = 0;        // Last known UNIX epoch
    uint32_t _millisAtEpoch = 0; // millis() when _epoch was set
};

}  // namespace oms