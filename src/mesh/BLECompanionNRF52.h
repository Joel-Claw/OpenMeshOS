// OpenMeshOS — BLECompanionNRF52.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// nRF52 BLE companion app connectivity using Adafruit Bluefruit nRF52 library.
// Implements IBLECompanion for nRF52840 targets (RAK WisBlock RAK4631).
//
// The nRF52 uses the Nordic SoftDevice for BLE, accessed via the
// Adafruit Bluefruit52Lib (bundled with the Adafruit nRF52 Arduino core).
// This is a completely different API from both the ESP32 BLE stack and
// the ArduinoBLE library.
//
// Key differences from ESP32 BLE:
//   - Uses Bluefruit global object (Bluefruit.begin(), Bluefruit.Periph, etc.)
//   - BLEService/BLECharacteristic from Bluefruit52Lib (not ArduinoBLE)
//   - SECMODE_OPEN / SECMODE_NO_ACCESS for permissions (not ESP_GATT_PERM_*)
//   - CHR_PROPS_READ / CHR_PROPS_WRITE / CHR_PROPS_NOTIFY for properties
//   - Write callbacks via setWriteCallback() (event-driven, no polling needed)
//   - Advertising via Bluefruit.Advertising.addService() / .start()
//   - Connection callbacks via Bluefruit.Periph.setConnectCallback()
//
// Key differences from ArduinoBLE:
//   - No BLE.begin() / BLE.poll() / BLE.central() polling
//   - Bluefruit.begin() initialises the SoftDevice
//   - Characteristics are added to the last service that called .begin()
//   - Use setWriteCallback() for event-driven write handling
//   - notify() instead of setValue() for notifications

#pragma once

#include "IBLECompanion.h"

#if defined(ARDUINO_ARCH_NRF52840)

#include <bluefruit.h>
#include "../utils/Config.h"

namespace oms {

/// nRF52 BLE companion service implementation (Adafruit Bluefruit52Lib).
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

    // Callback handlers (static, as Bluefruit52Lib uses C callbacks)
    static void onConnect(uint16_t conn_handle);
    static void onDisconnect(uint16_t conn_handle, uint8_t reason);
    static void onConfigWrite(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void onMessageWrite(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);

    void handleConfigWrite(const uint8_t* data, uint16_t len);
    void handleMessageWrite(const uint8_t* data, uint16_t len);
    void buildStatusPayload(uint8_t* buf, size_t& len);

    // BLE objects (Adafruit Bluefruit52Lib API)
    BLEService        _service;
    BLECharacteristic _cfgReadChar;
    BLECharacteristic _cfgWriteChar;
    BLECharacteristic _msgInChar;
    BLECharacteristic _msgOutChar;
    BLECharacteristic _statusChar;

    bool _connected = false;
    bool _enabled   = true;
    bool _begun     = false;

    // Constants (must match ESP32 implementation for companion app compat)
    static constexpr const char* BLE_DEVICE_PREFIX = "OpenMesh-";

    // Using 128-bit UUIDs for nRF52 (Bluefruit52Lib supports both 16-bit
    // and 128-bit UUIDs; we use 128-bit for custom services to avoid
    // collisions with standard Bluetooth SIG services).
    // Base UUID: 0000XXXX-0000-1000-8000-00805F9B34FB (standard base)
    // with XXXX = our 16-bit IDs from the ESP32 implementation.
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

    // Active connection handle (for notify targeting)
    uint16_t _connHandle = BLE_CONN_HANDLE_INVALID;

    // Device name buffer
    char _deviceName[24] = {0};
};

}  // namespace oms

#endif  // ARDUINO_ARCH_NRF52840