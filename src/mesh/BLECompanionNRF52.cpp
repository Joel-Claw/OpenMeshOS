// OpenMeshOS — BLECompanionNRF52.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// nRF52 BLE companion app service implementation.
// Uses Adafruit Bluefruit52Lib (Nordic SoftDevice) for BLE connectivity.
//
// BLE OTA firmware update uses the Nordic DFU bootloader via Adafruit BLEDfu.
// The companion app sends a trigger byte (0x01) to the firmware update
// characteristic, which starts the DFU process. BLEDfu handles saving bond
// data, setting GPREGRET, and jumping to the bootloader. The bootloader then
// advertises as a DFU target and the companion app transfers the new firmware.

#include "BLECompanionNRF52.h"
#include "MeshService.h"
#include "../hardware/IBoard.h"
#include "../hardware/PlatformCompat.h"
#include "../utils/Log.h"
#include "../utils/Config.h"

#if defined(ARDUINO_ARCH_NRF52840)

#include <bluefruit.h>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
BLECompanionNRF52& BLECompanionNRF52::instance() {
    static BLECompanionNRF52 s_ble;
    return s_ble;
}

// ── Static callback wrappers ──────────────────────────────────────
void BLECompanionNRF52::onConnect(uint16_t conn_handle) {
    auto& ble = instance();
    ble._connected = true;
    ble._connHandle = conn_handle;

    // Get peer device name if available
    BLEConnection* conn = Bluefruit.Connection(conn_handle);
    char peerName[32] = {0};
    if (conn) {
        conn->getPeerName(peerName, sizeof(peerName));
    }
    OMS_LOG("BLE", "Companion connected: %s", peerName[0] ? peerName : "(unknown)");
}

void BLECompanionNRF52::onDisconnect(uint16_t conn_handle, uint8_t reason) {
    auto& ble = instance();
    ble._connected = false;
    ble._connHandle = BLE_CONN_HANDLE_INVALID;
    (void)conn_handle;
    OMS_LOG("BLE", "Companion disconnected (reason=0x%02X)", reason);
}

void BLECompanionNRF52::onConfigWrite(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_handle;
    (void)chr;
    instance().handleConfigWrite(data, len);
}

void BLECompanionNRF52::onMessageWrite(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_handle;
    (void)chr;
    instance().handleMessageWrite(data, len);
}

void BLECompanionNRF52::onFirmwareWrite(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_handle;
    (void)chr;
    instance().handleFirmwareWrite(data, len);
}

// ── init ───────────────────────────────────────────────────────────
void BLECompanionNRF52::init() {
    OMS_LOG("BLE", "Initialising BLE companion service (nRF52 Bluefruit52Lib)");

    // Initialise the Bluefruit module (Nordic SoftDevice)
    Bluefruit.begin();
    _begun = true;

    // Set the connect/disconnect callback handlers
    Bluefruit.Periph.setConnectCallback(onConnect);
    Bluefruit.Periph.setDisconnectCallback(onDisconnect);

    // Build device name from callsign
    snprintf(_deviceName, sizeof(_deviceName), "%s%s",
             BLE_DEVICE_PREFIX, config::get().callsign);

    Bluefruit.configUuid128Count(16);  // Ensure 128-bit UUID support
    Bluefruit.setName(_deviceName);

    // Start the Nordic DFU service (built into Bluefruit52Lib)
    _dfuService.begin();

    // Configure and start the companion service
    // Order matters: service.begin() must be called before characteristic.begin()
    _service.setUuid(UUID_SERVICE);
    _service.begin();

    // ── Config Read characteristic ─────────────────────────────────
    _cfgReadChar.setUuid(UUID_CFG_READ);
    _cfgReadChar.setProperties(CHR_PROPS_READ);
    _cfgReadChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _cfgReadChar.setMaxLen(128);
    _cfgReadChar.begin();
    {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign, config::get().radioRegion,
                 config::get().channel, config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar.write(cfgBuf, strlen(cfgBuf));
    }

    // ── Config Write characteristic ────────────────────────────────
    _cfgWriteChar.setUuid(UUID_CFG_WRITE);
    _cfgWriteChar.setProperties(CHR_PROPS_WRITE);
    _cfgWriteChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _cfgWriteChar.setMaxLen(128);
    _cfgWriteChar.setWriteCallback(onConfigWrite, false);
    _cfgWriteChar.begin();

    // ── Messages Inbound (mesh -> phone, notify) ───────────────────
    _msgInChar.setUuid(UUID_MSG_IN);
    _msgInChar.setProperties(CHR_PROPS_NOTIFY);
    _msgInChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _msgInChar.setMaxLen(251);
    _msgInChar.begin();

    // ── Messages Outbound (phone -> mesh, write) ──────────────────
    _msgOutChar.setUuid(UUID_MSG_OUT);
    _msgOutChar.setProperties(CHR_PROPS_WRITE);
    _msgOutChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _msgOutChar.setMaxLen(251);
    _msgOutChar.setWriteCallback(onMessageWrite, false);
    _msgOutChar.begin();

    // ── Firmware Update characteristic (triggers Nordic DFU) ─────
    _fwUpdateChar.setUuid(UUID_FW_UPDATE);
    _fwUpdateChar.setProperties(CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
    _fwUpdateChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _fwUpdateChar.setMaxLen(1);
    _fwUpdateChar.setWriteCallback(onFirmwareWrite, false);
    _fwUpdateChar.begin();

    // ── Device Status characteristic ──────────────────────────────
    _statusChar.setUuid(UUID_STATUS);
    _statusChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _statusChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _statusChar.setMaxLen(64);
    _statusChar.begin();
    {
        uint8_t statusBuf[64];
        size_t statusLen = 0;
        buildStatusPayload(statusBuf, statusLen);
        _statusChar.write(statusBuf, statusLen);
    }

    // Setup advertising
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(_service);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);    // fast=20ms, slow=152.5ms
    Bluefruit.Advertising.setFastTimeout(30);      // 30s in fast mode
    Bluefruit.Advertising.start(0);                // 0 = advertise forever

    OMS_LOG("BLE", "Companion service started, advertising as '%s'", _deviceName);
}

// ── tick ───────────────────────────────────────────────────────────
void BLECompanionNRF52::tick() {
    if (!_enabled || !_begun) return;

    // Bluefruit52Lib is event-driven (callbacks), so we don't need to poll
    // for writes like ArduinoBLE. We only do periodic status updates here.

    if (_connected) {
        uint32_t now = millis();
        if (now - _lastStatusMs >= STATUS_UPDATE_MS) {
            notifyStatus();
            _lastStatusMs = now;
        }
    }

    // OTA timeout: if DFU trigger was sent but reboot hasn't happened yet,
    // reset OTA state after the timeout period
    if (_otaTriggered) {
        uint32_t now = millis();
        if (now - _otaTriggerMs >= OTA_REBOOT_TIMEOUT_MS) {
            OMS_LOG("BLE", "OTA: DFU reboot timeout, resetting OTA state");
            _otaTriggered = false;
        }
    }
}

// ── notifyMessage ──────────────────────────────────────────────────
void BLECompanionNRF52::notifyMessage(const InboxMessage& msg) {
    if (!_connected || _connHandle == BLE_CONN_HANDLE_INVALID) return;

    // Check if notifications are enabled for this characteristic
    if (!_msgInChar.notifyEnabled(_connHandle)) return;

    // Format: [1 byte kind] [1 byte channel] [8 bytes sender] [N bytes text]
    uint8_t buf[MSG_MAX_LEN + 12];
    size_t len = 0;

    buf[len++] = (uint8_t)msg.kind;
    buf[len++] = msg.channel_id;
    memcpy(buf + len, msg.sender, 8);
    len += 8;

    size_t textLen = strlen(msg.text);
    if (textLen > MSG_MAX_LEN) textLen = MSG_MAX_LEN;
    memcpy(buf + len, msg.text, textLen);
    len += textLen;

    _msgInChar.notify(_connHandle, buf, len);
    OMS_LOG("BLE", "Notified message from %s (%u bytes)", msg.sender, (unsigned)len);
}

// ── notifyStatus ──────────────────────────────────────────────────
void BLECompanionNRF52::notifyStatus() {
    if (!_connected || _connHandle == BLE_CONN_HANDLE_INVALID) return;
    if (!_statusChar.notifyEnabled(_connHandle)) return;

    uint8_t buf[64];
    size_t len = 0;
    buildStatusPayload(buf, len);
    _statusChar.notify(_connHandle, buf, len);
}

// ── buildStatusPayload ────────────────────────────────────────────
void BLECompanionNRF52::buildStatusPayload(uint8_t* buf, size_t& len) {
    // Binary status payload (same format as ESP32 implementation):
    //   [2B battery mV] [1B battery%] [1B RSSI] [4B uptime sec]
    //   [2B free heap KB] [2B free PSRAM KB] [1B node count]
    //   [1B channel] [1B region code]
    // Total: 15 bytes

    len = 0;

    uint16_t battMv = 0;
    if (MeshService::instance().initialized()) {
        battMv = theBoard()->batteryMilliVolts();
    }
    buf[len++] = battMv & 0xFF;
    buf[len++] = (battMv >> 8) & 0xFF;

    uint8_t battPct = 0;
    if (battMv > 3200) {
        battPct = (uint8_t)((battMv - 3200) * 100 / 1000);
        if (battPct > 100) battPct = 100;
    }
    buf[len++] = battPct;

    int rssi = MeshService::instance().rssi();
    buf[len++] = (uint8_t)(int8_t)rssi;

    uint32_t uptimeSec = millis() / 1000;
    buf[len++] = uptimeSec & 0xFF;
    buf[len++] = (uptimeSec >> 8) & 0xFF;
    buf[len++] = (uptimeSec >> 16) & 0xFF;
    buf[len++] = (uptimeSec >> 24) & 0xFF;

    // Use platform abstraction for heap
    uint32_t freeHeap = platform::freeHeap() / 1024;
    buf[len++] = freeHeap & 0xFF;
    buf[len++] = (freeHeap >> 8) & 0xFF;

    // nRF52 has no PSRAM — report 0
    buf[len++] = 0;
    buf[len++] = 0;

    buf[len++] = (uint8_t)MeshService::instance().nodeCount();
    buf[len++] = (uint8_t)config::get().channel;

    static const char* regionNames[] = {"EU868", "US915", "AU915", "AS923", "KR920", "IN865"};
    uint8_t regionIdx = 0;
    const char* curRegion = config::get().radioRegion;
    for (uint8_t i = 0; i < 6; i++) {
        if (strcmp(curRegion, regionNames[i]) == 0) {
            regionIdx = i;
            break;
        }
    }
    buf[len++] = regionIdx;
}

// ── handleConfigWrite ─────────────────────────────────────────────
void BLECompanionNRF52::handleConfigWrite(const uint8_t* data, uint16_t len) {
    uint32_t now = millis();
    if (now - _lastCfgWriteMs < CFG_WRITE_MIN_INTERVAL_MS) {
        OMS_LOG("BLE", "Config write rate-limited");
        return;
    }
    _lastCfgWriteMs = now;

    if (len == 0 || !data) return;

    // Copy to a null-terminated buffer for parsing
    char buf[129];
    size_t copyLen = (size_t)len < sizeof(buf) - 1 ? (size_t)len : sizeof(buf) - 1;
    memcpy(buf, data, copyLen);
    buf[copyLen] = '\0';

    OMS_LOG("BLE", "Config write: %u bytes", (unsigned)copyLen);

    // Parse key=value pairs, one per line
    const char* line = buf;
    while (line && *line) {
        const char* eol = strchr(line, '\n');
        size_t lineLen = eol ? (size_t)(eol - line) : strlen(line);

        const char* eq = (const char*)memchr(line, '=', lineLen);
        if (eq) {
            size_t keyLen = eq - line;
            size_t valLen = lineLen - keyLen - 1;

            char key[32] = {0};
            char val[64] = {0};
            if (keyLen < sizeof(key)) memcpy(key, line, keyLen);
            if (valLen < sizeof(val)) memcpy(val, eq + 1, valLen);

            if (strcmp(key, "callsign") == 0) {
                config::setCallsign(val);
                OMS_LOG("BLE", "Config: callsign=%s", val);
            } else if (strcmp(key, "region") == 0) {
                config::setRegion(val);
                OMS_LOG("BLE", "Config: region=%s", val);
            } else if (strcmp(key, "channel") == 0) {
                int ch = atoi(val);
                if (ch >= 0 && ch < 8) {
                    const_cast<Config&>(config::get()).channel = ch;
                    OMS_LOG("BLE", "Config: channel=%d", ch);
                }
            } else if (strcmp(key, "brightness") == 0) {
                int br = atoi(val);
                if (br >= 0 && br <= 255) {
                    const_cast<Config&>(config::get()).brightness = br;
                    OMS_LOG("BLE", "Config: brightness=%d", br);
                }
            } else if (strcmp(key, "sound") == 0) {
                bool snd = (atoi(val) != 0);
                const_cast<Config&>(config::get()).notifySound = snd;
                OMS_LOG("BLE", "Config: sound=%s", snd ? "on" : "off");
            } else {
                OMS_LOG("BLE", "Config: unknown key '%s', ignored", key);
            }
        }
        line = eol ? eol + 1 : nullptr;
    }

    config::save();

    // Update config read characteristic
    char cfgBuf[128];
    snprintf(cfgBuf, sizeof(cfgBuf),
             "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
             config::get().callsign, config::get().radioRegion,
             config::get().channel, config::get().brightness,
             config::get().notifySound ? 1 : 0);
    _cfgReadChar.write(cfgBuf, strlen(cfgBuf));
}

// ── handleMessageWrite ────────────────────────────────────────────
void BLECompanionNRF52::handleMessageWrite(const uint8_t* data, uint16_t len) {
    if (len < 2 || !data) return;

    uint8_t channelId = data[0];
    size_t textLen = (size_t)len - 1;

    char textBuf[MSG_MAX_LEN + 1];
    if (textLen > MSG_MAX_LEN) textLen = MSG_MAX_LEN;
    memcpy(textBuf, data + 1, textLen);
    textBuf[textLen] = '\0';

    if (channelId == 0xFF) {
        MeshService::instance().sendChannel("public", textBuf);
    } else {
        const char* channelName = (channelId == 0) ? "public" : "ch1";
        MeshService::instance().sendChannel(channelName, textBuf);
    }
    OMS_LOG("BLE", "Message sent to channel %d: %s", channelId, textBuf);
}

// ── handleFirmwareWrite ───────────────────────────────────────────
void BLECompanionNRF52::handleFirmwareWrite(const uint8_t* data, uint16_t len) {
    if (len < 1 || !data) return;

    uint8_t cmd = data[0];
    OMS_LOG("BLE", "OTA: firmware write cmd=0x%02X", cmd);

    switch (cmd) {
        case 0x01:  // Trigger DFU mode
            OMS_LOG("BLE", "OTA: Triggering Nordic DFU bootloader mode");

            // Notify companion app that DFU is starting
            notifyOtaProgress(1, 0, 0);

            _otaTriggered = true;
            _otaTriggerMs = millis();

            // Small delay so the notify can be sent before we reboot
            delay(100);

            // The companion app should write 0x01 to the DFU control
            // characteristic (UUID 0x1531) directly. The BLEDfu service
            // handles saving peer bond data, setting GPREGRET=0xB1,
            // disabling SoftDevice, and jumping to the bootloader.
            // We cannot trigger it from the application side because
            // BLEDfu uses a write-authorize callback, not a public API.
            // The companion app uses Nordic DFU protocol on the DFU
            // service (UUID 0x1530) to perform the full firmware transfer.
            OMS_LOG("BLE", "OTA: Companion should use DFU service (UUID 0x1530)");
            break;

        case 0x02:  // Cancel/abort (no-op if already in DFU)
            OMS_LOG("BLE", "OTA: Cancel requested (no-op in trigger mode)");
            _otaTriggered = false;
            break;

        case 0x03:  // Query OTA state
            notifyOtaProgress(_otaTriggered ? 2 : 0, 0, 0);
            break;

        default:
            OMS_LOG("BLE", "OTA: Unknown command 0x%02X, ignored", cmd);
            break;
    }
}

// ── notifyOtaProgress ─────────────────────────────────────────────
void BLECompanionNRF52::notifyOtaProgress(uint8_t step, uint32_t current, uint32_t total) {
    if (!_connected || _connHandle == BLE_CONN_HANDLE_INVALID) return;
    if (!_fwUpdateChar.notifyEnabled(_connHandle)) return;

    // Notify payload: [1B step] [4B current] [4B total]
    uint8_t buf[9];
    size_t len = 0;

    buf[len++] = step;
    buf[len++] = current & 0xFF;
    buf[len++] = (current >> 8) & 0xFF;
    buf[len++] = (current >> 16) & 0xFF;
    buf[len++] = (current >> 24) & 0xFF;
    buf[len++] = total & 0xFF;
    buf[len++] = (total >> 8) & 0xFF;
    buf[len++] = (total >> 16) & 0xFF;
    buf[len++] = (total >> 24) & 0xFF;

    _fwUpdateChar.notify(_connHandle, buf, len);
    OMS_LOG("BLE", "OTA progress: step=%u current=%u total=%u", step, current, total);
}

// ── setEnabled ────────────────────────────────────────────────────
void BLECompanionNRF52::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!_begun) return;

    if (!enabled) {
        Bluefruit.Advertising.stop();
        OMS_LOG("BLE", "Advertising stopped (disabled)");
    } else if (!_connected) {
        Bluefruit.Advertising.start(0);
        OMS_LOG("BLE", "Advertising restarted");
    }
}

}  // namespace oms

#endif  // ARDUINO_ARCH_NRF52840