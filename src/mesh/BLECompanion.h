// OpenMeshOS — BLECompanion.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BLE companion app connectivity.
// Allows a phone/tablet to connect via BLE to:
//   - Configure the device (callsign, region, channel, etc)
//   - Send/receive messages through the mesh
//   - Read device status (battery, RSSI, uptime, node count)
//   - Trigger firmware update via BLE OTA
//
// BLE service layout:
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
// The companion app must authenticate with a shared secret
// derived from the device identity key.

#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>
#include "MessageBus.h"
#include "../utils/Config.h"

namespace oms {

/// BLE companion service for phone/tablet connectivity.
class BLECompanion {
public:
    static BLECompanion& instance();

    /// Initialise BLE stack and start advertising.
    /// Call once after MeshService is ready.
    void init();

    /// Called from main loop to process BLE events.
    void tick();

    /// Push an incoming mesh message to BLE (sends notification
    /// to connected phone). Called when a new message arrives.
    void notifyMessage(const InboxMessage& msg);

    /// Push device status update to connected phone.
    void notifyStatus();

    /// Whether a phone is currently connected.
    bool isConnected() const { return _connected; }

    /// Set enabled/disabled (from config).
    void setEnabled(bool enabled);
    bool enabled() const { return _enabled; }

private:
    BLECompanion() = default;

    /// BLE server callbacks (connect/disconnect)
    class ServerCallbacks;
    friend class ServerCallbacks;

    /// Characteristic write callbacks
    class ConfigWriteCallback;
    class MessageWriteCallback;
    class FirmwareWriteCallback;
    friend class ConfigWriteCallback;
    friend class MessageWriteCallback;
    friend class FirmwareWriteCallback;

    /// Build device status payload (battery, RSSI, uptime, nodes, heap)
    void buildStatusPayload(uint8_t* buf, size_t& len);

    /// Handle config write from phone (JSON or key=value)
    void handleConfigWrite(BLECharacteristic* pChar);

    /// Handle message write from phone (sends to mesh)
    void handleMessageWrite(BLECharacteristic* pChar);

    /// Handle BLE OTA firmware chunk write
    void handleFirmwareWrite(BLECharacteristic* pChar);

    /// Send OTA progress notification to phone
    void notifyOtaProgress(uint8_t step, uint32_t current, uint32_t total);

    // BLE objects
    BLEServer*       _server       = nullptr;
    BLEService*      _service      = nullptr;
    BLECharacteristic* _cfgReadChar  = nullptr;
    BLECharacteristic* _cfgWriteChar = nullptr;
    BLECharacteristic* _msgInChar    = nullptr;
    BLECharacteristic* _msgOutChar   = nullptr;
    BLECharacteristic* _statusChar   = nullptr;
    BLECharacteristic* _fwUpdateChar = nullptr;

    bool _connected = false;
    bool _enabled   = true;    // default on
    bool _advertising = false;

    // OTA state
    bool    _otaInProgress = false;
    size_t  _otaWritten = 0;
    size_t  _otaTotalSize = 0;
    uint8_t _otaStep = 0;  // 0=idle, 1=started, 2=data, 3=finish, 0xFF=error

    // Device name for BLE advertising
    static constexpr const char* BLE_DEVICE_PREFIX = "OpenMesh-";

    // Service and characteristic UUIDs (16-bit for efficiency)
    static constexpr uint16_t UUID_SERVICE         = 0xFEDC;
    static constexpr uint16_t UUID_CFG_READ        = 0x0001;
    static constexpr uint16_t UUID_CFG_WRITE       = 0x0002;
    static constexpr uint16_t UUID_MSG_IN          = 0x0003;
    static constexpr uint16_t UUID_MSG_OUT         = 0x0004;
    static constexpr uint16_t UUID_STATUS          = 0x0005;
    static constexpr uint16_t UUID_FW_UPDATE       = 0x0006;

    // Advertising interval (ms)
    static constexpr uint16_t ADV_INTERVAL_MIN = 100;   // 100ms
    static constexpr uint16_t ADV_INTERVAL_MAX = 500;   // 500ms

    // Status update interval (ms)
    static constexpr uint32_t STATUS_UPDATE_MS = 5000;
    uint32_t _lastStatusMs = 0;
};

}  // namespace oms