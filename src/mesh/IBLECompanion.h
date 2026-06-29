// OpenMeshOS — IBLECompanion.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BLE companion abstraction layer.
// Provides a platform-agnostic interface for BLE companion app connectivity,
// allowing phone/tablet to:
//   - Configure the device (callsign, region, channel, etc)
//   - Send/receive messages through the mesh
//   - Read device status (battery, RSSI, uptime, node count)
//   - Trigger firmware update via BLE OTA
//
// Platform implementations:
//   - BLECompanionESP32 (ESP32 Arduino BLE stack)
//   - BLECompanionNRF52 (nRF52 ArduinoBLE / Nordic SoftDevice)
//
// The factory function createBLECompanion() returns the correct implementation
// for the current build target.
//
// BLE service layout (shared across platforms):
//   Service UUID: 0xFEDC (OpenMeshOS companion)
//
//   Characteristic          UUID    Properties    Size
//   ----------------------  ------  -----------    ----
//   Config Read             0x0001  READ           128B
//   Config Write            0x0002  WRITE          128B
//   Messages Inbound        0x0003  NOTIFY         251B
//   Messages Outbound       0x0004  WRITE          251B
//   Device Status           0x0005  NOTIFY+READ    64B
//   Firmware Update         0x0006  WRITE          512B
//
// Security: BLE pairing required (encrypted link).

#pragma once

#include <Arduino.h>
#include "MessageBus.h"

namespace oms {

/// Platform-agnostic BLE companion interface.
class IBLECompanion {
public:
    virtual ~IBLECompanion() = default;

    /// Initialise BLE stack and start advertising.
    /// Call once after MeshService is ready.
    virtual void init() = 0;

    /// Called from main loop to process BLE events.
    virtual void tick() = 0;

    /// Push an incoming mesh message to BLE (sends notification
    /// to connected phone). Called when a new message arrives.
    virtual void notifyMessage(const InboxMessage& msg) = 0;

    /// Push device status update to connected phone.
    virtual void notifyStatus() = 0;

    /// Whether a phone is currently connected.
    virtual bool isConnected() const = 0;

    /// Set enabled/disabled (from config).
    virtual void setEnabled(bool enabled) = 0;
    virtual bool enabled() const = 0;
};

/// Factory function: returns the platform-appropriate BLE companion.
/// Caller does NOT own the pointer (it's a static singleton).
IBLECompanion* createBLECompanion();

/// Convenience accessor (same as createBLECompanion, cached).
IBLECompanion& theBLECompanion();

}  // namespace oms