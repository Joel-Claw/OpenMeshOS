// OpenMeshOS — BLECompanion.h (ESP32 implementation)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// ESP32 BLE companion app connectivity using the ESP32 Arduino BLE stack.
// Implements IBLECompanion for ESP32-S3 targets (T-Deck, Heltec V3).

#pragma once

#include "IBLECompanion.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>
#include "../utils/Config.h"

namespace oms {

/// ESP32 BLE companion service implementation.
class BLECompanionESP32 : public IBLECompanion {
public:
    static BLECompanionESP32& instance();

    void init() override;
    void tick() override;
    void notifyMessage(const InboxMessage& msg) override;
    void notifyStatus() override;
    bool isConnected() const override { return _connected; }
    void setEnabled(bool enabled) override;
    bool enabled() const override { return _enabled; }

private:
    BLECompanionESP32() = default;

    class ServerCallbacks;
    friend class ServerCallbacks;

    class ConfigWriteCallback;
    class MessageWriteCallback;
    class FirmwareWriteCallback;
    friend class ConfigWriteCallback;
    friend class MessageWriteCallback;
    friend class FirmwareWriteCallback;

    void buildStatusPayload(uint8_t* buf, size_t& len);
    void handleConfigWrite(BLECharacteristic* pChar);
    void handleMessageWrite(BLECharacteristic* pChar);
    void handleFirmwareWrite(BLECharacteristic* pChar);
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
    bool _enabled   = true;
    bool _advertising = false;

    // OTA state
    bool    _otaInProgress = false;
    size_t  _otaWritten = 0;
    size_t  _otaTotalSize = 0;
    uint8_t _otaStep = 0;
    uint32_t _otaLastDataMs = 0;  // Last OTA data packet timestamp

    // OTA abort timeout: if no data for this duration, auto-abort
    static constexpr uint32_t OTA_TIMEOUT_MS = 30000;  // 30 seconds

    // Constants
    static constexpr const char* BLE_DEVICE_PREFIX = "OpenMesh-";
    static constexpr uint16_t UUID_SERVICE         = 0xFEDC;
    static constexpr uint16_t UUID_CFG_READ        = 0x0001;
    static constexpr uint16_t UUID_CFG_WRITE       = 0x0002;
    static constexpr uint16_t UUID_MSG_IN          = 0x0003;
    static constexpr uint16_t UUID_MSG_OUT         = 0x0004;
    static constexpr uint16_t UUID_STATUS          = 0x0005;
    static constexpr uint16_t UUID_FW_UPDATE       = 0x0006;

    static constexpr uint16_t ADV_INTERVAL_MIN = 100;
    static constexpr uint16_t ADV_INTERVAL_MAX = 500;
    static constexpr uint32_t STATUS_UPDATE_MS = 5000;
    uint32_t _lastStatusMs = 0;

    static constexpr uint32_t CFG_WRITE_MIN_INTERVAL_MS = 1000;
    uint32_t _lastCfgWriteMs = 0;
};

}  // namespace oms

#endif  // ARDUINO_ARCH_ESP32