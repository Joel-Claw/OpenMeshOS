// OpenMeshOS — BLECompanionNRF52.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// nRF52 BLE companion app service implementation.
// Uses ArduinoBLE library (Nordic SoftDevice) for BLE connectivity.
//
// NOTE: BLE OTA firmware update is NOT implemented on nRF52 in this version.
// The nRF52 uses a different OTA mechanism (nRF52 DFU / bootloader-based).
// OTA via BLE would require integrating with the Nordic bootloader, which
// is a separate effort. The firmware update characteristic is omitted here.

#include "BLECompanionNRF52.h"
#include "MeshService.h"
#include "../hardware/IBoard.h"
#include "../hardware/PlatformCompat.h"
#include "../utils/Log.h"
#include "../utils/Config.h"

#if defined(ARDUINO_ARCH_NRF52840)

#include <ArduinoBLE.h>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
BLECompanionNRF52& BLECompanionNRF52::instance() {
    static BLECompanionNRF52 s_ble;
    return s_ble;
}

// ── init ───────────────────────────────────────────────────────────
void BLECompanionNRF52::init() {
    OMS_LOG("BLE", "Initialising BLE companion service (nRF52)");

    if (!BLE.begin()) {
        OMS_LOG("BLE", "ERROR: BLE.begin() failed — SoftDevice not available?");
        return;
    }
    _begun = true;

    // Build device name from callsign
    snprintf(_deviceName, sizeof(_deviceName), "%s%s",
             BLE_DEVICE_PREFIX, config::get().callsign);

    BLE.setLocalName(_deviceName);
    BLE.setDeviceName(_deviceName);

    // Require bonding/encryption (nRF52 SoftDevice pairing)
    // BLE.setAuthorization(true);  // uncomment when companion app supports pairing

    // Advertise our custom service
    BLE.setAdvertisedServiceUuid(UUID_SERVICE);

    // Create the companion service
    _service = new BLEService(UUID_SERVICE);
    if (!_service) {
        OMS_LOG("BLE", "ERROR: BLEService alloc failed");
        return;
    }

    // ── Config Read characteristic ─────────────────────────────────
    _cfgReadChar = new BLECharacteristic(UUID_CFG_READ, BLERead, 128);
    if (_cfgReadChar) {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign, config::get().radioRegion,
                 config::get().channel, config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar->writeValue(cfgBuf, strlen(cfgBuf));
        _service->addCharacteristic(_cfgReadChar);
    }

    // ── Config Write characteristic ────────────────────────────────
    _cfgWriteChar = new BLECharacteristic(UUID_CFG_WRITE, BLEWrite, 128);
    if (_cfgWriteChar) {
        _service->addCharacteristic(_cfgWriteChar);
    }

    // ── Messages Inbound (mesh -> phone) ───────────────────────────
    _msgInChar = new BLECharacteristic(UUID_MSG_IN, BLENotify, 251);
    if (_msgInChar) {
        _service->addCharacteristic(_msgInChar);
    }

    // ── Messages Outbound (phone -> mesh) ──────────────────────────
    _msgOutChar = new BLECharacteristic(UUID_MSG_OUT, BLEWrite, 251);
    if (_msgOutChar) {
        _service->addCharacteristic(_msgOutChar);
    }

    // ── Device Status characteristic ──────────────────────────────
    _statusChar = new BLECharacteristic(UUID_STATUS, BLERead | BLENotify, 64);
    if (_statusChar) {
        uint8_t statusBuf[64];
        size_t statusLen = 0;
        buildStatusPayload(statusBuf, statusLen);
        _statusChar->writeValue(statusBuf, statusLen);
        _service->addCharacteristic(_statusChar);
    }

    // Add the service
    BLE.addService(*_service);

    // Start advertising
    BLE.advertise();

    OMS_LOG("BLE", "Companion service started, advertising as '%s'", _deviceName);
}

// ── tick ───────────────────────────────────────────────────────────
void BLECompanionNRF52::tick() {
    if (!_enabled || !_begun) return;

    // Poll BLE events (ArduinoBLE requires polling)
    BLE.poll();

    // Check connection state
    BLEDevice central = BLE.central();
    bool wasConnected = _connected;
    _connected = central && central.connected();

    if (_connected && !wasConnected) {
        OMS_LOG("BLE", "Companion connected: %s", central.address().c_str());
    } else if (!_connected && wasConnected) {
        OMS_LOG("BLE", "Companion disconnected");
    }

    // Handle config writes
    if (_connected && _cfgWriteChar && _cfgWriteChar->written()) {
        handleConfigWrite(*_cfgWriteChar);
    }

    // Handle message writes
    if (_connected && _msgOutChar && _msgOutChar->written()) {
        handleMessageWrite(*_msgOutChar);
    }

    // Periodic status update
    if (_connected && _statusChar) {
        uint32_t now = millis();
        if (now - _lastStatusMs >= STATUS_UPDATE_MS) {
            notifyStatus();
            _lastStatusMs = now;
        }
    }
}

// ── notifyMessage ──────────────────────────────────────────────────
void BLECompanionNRF52::notifyMessage(const InboxMessage& msg) {
    if (!_connected || !_msgInChar) return;

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

    _msgInChar->setValue(buf, len);
    OMS_LOG("BLE", "Notified message from %s (%u bytes)", msg.sender, (unsigned)len);
}

// ── notifyStatus ──────────────────────────────────────────────────
void BLECompanionNRF52::notifyStatus() {
    if (!_statusChar) return;
    uint8_t buf[64];
    size_t len = 0;
    buildStatusPayload(buf, len);
    _statusChar->setValue(buf, len);
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
void BLECompanionNRF52::handleConfigWrite(BLECharacteristic& charRef) {
    uint32_t now = millis();
    if (now - _lastCfgWriteMs < CFG_WRITE_MIN_INTERVAL_MS) {
        OMS_LOG("BLE", "Config write rate-limited");
        return;
    }
    _lastCfgWriteMs = now;

    // Read the written value
    int valueLen = charRef.valueLength();
    if (valueLen <= 0) return;

    const uint8_t* data = charRef.value();
    if (!data || valueLen == 0) return;

    // Copy to a null-terminated buffer for parsing
    char buf[129];
    size_t copyLen = (size_t)valueLen < sizeof(buf) - 1 ? (size_t)valueLen : sizeof(buf) - 1;
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
    if (_cfgReadChar) {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign, config::get().radioRegion,
                 config::get().channel, config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar->writeValue(cfgBuf, strlen(cfgBuf));
    }
}

// ── handleMessageWrite ────────────────────────────────────────────
void BLECompanionNRF52::handleMessageWrite(BLECharacteristic& charRef) {
    int valueLen = charRef.valueLength();
    if (valueLen < 2) return;

    const uint8_t* data = charRef.value();
    if (!data) return;

    uint8_t channelId = data[0];
    size_t textLen = (size_t)valueLen - 1;

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

// ── setEnabled ────────────────────────────────────────────────────
void BLECompanionNRF52::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!_begun) return;

    if (!enabled) {
        BLE.stopAdvertise();
        OMS_LOG("BLE", "Advertising stopped (disabled)");
    } else if (!_connected) {
        BLE.advertise();
        OMS_LOG("BLE", "Advertising restarted");
    }
}

}  // namespace oms

#endif  // ARDUINO_ARCH_NRF52840