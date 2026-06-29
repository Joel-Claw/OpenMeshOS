// OpenMeshOS — BLECompanionNRF52.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// nRF52 BLE companion app connectivity using ArduinoBLE library.
// Implements IBLECompanion for nRF52840 targets (RAK WisBlock RAK4631).
//
// The nRF52 uses the Nordic SoftDevice for BLE, accessed via the
// ArduinoBLE library (included with the Arduino nRF52 core).
// This is a completely different API from the ESP32 BLE stack.
//
// Key differences from ESP32 BLE:
//   - No BLEServer/BLECharacteristic classes (ArduinoBLE uses BLEService/BLECharacteristic)
//   - No ESP_GATT_PERM_* constants (ArduinoBLE uses BLE_READ/BLE_WRITE/BLE_NOTIFY)
//   - No esp_system.h Update.h (nRF52 uses different OTA mechanism)
//   - BLE bonding/pairing handled via BLE.setAdvertisedServiceUuid / BLE.setAuthorization

#pragma once

#include "IBLECompanion.h"

#if defined(ARDUINO_ARCH_NRF52840)

#include <ArduinoBLE.h>
#include "../utils/Config.h"

namespace oms {

/// nRF52 BLE companion service implementation.
class BLECompanionNRF52 : public IBLECompanion {
public:
    static BLECompanionNRF52& instance();

    void init() override;
    void tick() override;
    void notifyMessage(const InboxMessage& msg) override;
    void notifyStatus() override;
    bool isConnected() const override { return _connected; }
    void setEnabled(bool enabled) override;
    bool enabled() const override { return _enabled; }

private:
    BLECompanionNRF52() = default;

    void buildStatusPayload(uint8_t* buf, size_t& len);
    void handleConfigWrite(BLECharacteristic& charRef);
    void handleMessageWrite(BLECharacteristic& charRef);

    // BLE objects (ArduinoBLE API)
    BLEService*           _service      = nullptr;
    BLECharacteristic*    _cfgReadChar  = nullptr;
    BLECharacteristic*    _cfgWriteChar = nullptr;
    BLECharacteristic*    _msgInChar    = nullptr;
    BLECharacteristic*    _msgOutChar   = nullptr;
    BLECharacteristic*    _statusChar   = nullptr;

    bool _connected = false;
    bool _enabled   = true;
    bool _begun     = false;

    // Constants (must match ESP32 implementation for companion app compat)
    static constexpr const char* BLE_DEVICE_PREFIX = "OpenMesh-";

    // Using 128-bit UUIDs for nRF52 (ArduinoBLE requires full UUIDs
    // for custom services; 16-bit short UUIDs are for standard services)
    // We use a custom base UUID: 0000XXXX-0000-1000-8000-00805F9B34FB
    // with XXXX = our 16-bit IDs from the ESP32 implementation
    // This ensures the same companion app works across platforms.
    static constexpr const char* UUID_SERVICE    = "0000fedc-0000-1000-8000-00805f9b34fb";
    static constexpr const char* UUID_CFG_READ   = "00000001-0000-1000-8000-00805f9b34fb";
    static constexpr const char* UUID_CFG_WRITE  = "00000002-0000-1000-8000-00805f9b34fb";
    static constexpr const char* UUID_MSG_IN     = "00000003-0000-1000-8000-00805f9b34fb";
    static constexpr const char* UUID_MSG_OUT    = "00000004-0000-1000-8000-00805f9b34fb";
    static constexpr const char* UUID_STATUS     = "00000005-0000-1000-8000-00805f9b34fb";

    // Status update interval (ms)
    static constexpr uint32_t STATUS_UPDATE_MS = 5000;
    uint32_t _lastStatusMs = 0;

    // Config write rate limiting
    static constexpr uint32_t CFG_WRITE_MIN_INTERVAL_MS = 1000;
    uint32_t _lastCfgWriteMs = 0;

    // Device name buffer
    char _deviceName[24] = {0};
};

}  // namespace oms

#endif  // ARDUINO_ARCH_NRF52840