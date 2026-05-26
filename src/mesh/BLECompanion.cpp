// OpenMeshOS — BLECompanion.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// BLE companion app service implementation.
// Provides config, messaging, and status over BLE.

#include "BLECompanion.h"
#include "MeshService.h"
#include "TDeckBoard.h"
#include "../hardware/Board.h"
#include "../hardware/Notification.h"
#include "../utils/Log.h"
#include "../utils/Config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_system.h>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
BLECompanion& BLECompanion::instance() {
    static BLECompanion s_ble;
    return s_ble;
}

// ── Server callbacks (connection/disconnection) ──────────────────
class BLECompanion::ServerCallbacks : public BLEServerCallbacks {
public:
    ServerCallbacks(BLECompanion& parent) : _parent(parent) {}

    void onConnect(BLEServer* pServer) override {
        OMS_LOG("BLE", "Companion connected");
        _parent._connected = true;
        // Stop advertising while connected
        pServer->getAdvertising()->stop();
        _parent._advertising = false;
    }

    void onDisconnect(BLEServer* pServer) override {
        OMS_LOG("BLE", "Companion disconnected");
        _parent._connected = false;
        // Restart advertising
        pServer->startAdvertising();
        _parent._advertising = true;
    }

private:
    BLECompanion& _parent;
};

// ── Config write callback ────────────────────────────────────────
class BLECompanion::ConfigWriteCallback : public BLECharacteristicCallbacks {
public:
    ConfigWriteCallback(BLECompanion& parent) : _parent(parent) {}

    void onWrite(BLECharacteristic* pChar) override {
        _parent.handleConfigWrite(pChar);
    }

private:
    BLECompanion& _parent;
};

// ── Message write callback ───────────────────────────────────────
class BLECompanion::MessageWriteCallback : public BLECharacteristicCallbacks {
public:
    MessageWriteCallback(BLECompanion& parent) : _parent(parent) {}

    void onWrite(BLECharacteristic* pChar) override {
        _parent.handleMessageWrite(pChar);
    }

private:
    BLECompanion& _parent;
};

// ── init ───────────────────────────────────────────────────────────
void BLECompanion::init() {
    OMS_LOG("BLE", "Initialising BLE companion service");

    // Build device name from callsign
    char deviceName[24];
    snprintf(deviceName, sizeof(deviceName), "%s%s",
             BLE_DEVICE_PREFIX, config::get().callsign);

    // Initialise BLE device
    BLEDevice::init(deviceName);

    // Set security: require pairing (encrypted link)
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);

    // Create BLE server
    _server = BLEDevice::createServer();
    _server->setCallbacks(new ServerCallbacks(*this));

    // Create companion service
    _service = _server->createService(BLEUUID(UUID_SERVICE));

    // ── Config Read characteristic ─────────────────────────────────
    _cfgReadChar = _service->createCharacteristic(
        BLEUUID(UUID_CFG_READ),
        BLECharacteristic::PROPERTY_READ
    );
    _cfgReadChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

    // Populate initial config as key=value pairs
    {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign,
                 config::get().radioRegion,
                 config::get().channel,
                 config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar->setValue((uint8_t*)cfgBuf, strlen(cfgBuf));
    }

    // ── Config Write characteristic ────────────────────────────────
    _cfgWriteChar = _service->createCharacteristic(
        BLEUUID(UUID_CFG_WRITE),
        BLECharacteristic::PROPERTY_WRITE
    );
    _cfgWriteChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    _cfgWriteChar->setCallbacks(new ConfigWriteCallback(*this));

    // ── Messages Inbound (mesh → phone) ────────────────────────────
    _msgInChar = _service->createCharacteristic(
        BLEUUID(UUID_MSG_IN),
        BLECharacteristic::PROPERTY_NOTIFY
    );
    _msgInChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    // Add CCCD descriptor for notifications
    BLE2902* pCCCD = new BLE2902();
    _msgInChar->addDescriptor(pCCCD);

    // ── Messages Outbound (phone → mesh) ──────────────────────────
    _msgOutChar = _service->createCharacteristic(
        BLEUUID(UUID_MSG_OUT),
        BLECharacteristic::PROPERTY_WRITE
    );
    _msgOutChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    _msgOutChar->setCallbacks(new MessageWriteCallback(*this));

    // ── Device Status characteristic ──────────────────────────────
    _statusChar = _service->createCharacteristic(
        BLEUUID(UUID_STATUS),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _statusChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    BLE2902* pStatusCCCD = new BLE2902();
    _statusChar->addDescriptor(pStatusCCCD);

    // Set initial status
    {
        uint8_t statusBuf[64];
        size_t statusLen = 0;
        buildStatusPayload(statusBuf, statusLen);
        _statusChar->setValue(statusBuf, statusLen);
    }

    // Start the service
    _service->start();

    // Start advertising
    _server->getAdvertising()->addServiceUUID(BLEUUID(UUID_SERVICE));
    _server->getAdvertising()->setMinPreferred(0x06);  // connection interval hint
    _server->getAdvertising()->setMinPreferred(0x12);
    _server->startAdvertising();
    _advertising = true;

    OMS_LOG("BLE", "Companion service started, advertising as '%s'", deviceName);
}

// ── tick ───────────────────────────────────────────────────────────
void BLECompanion::tick() {
    if (!_enabled) return;

    // Periodic status update to connected phone
    if (_connected && _statusChar) {
        uint32_t now = millis();
        if (now - _lastStatusMs >= STATUS_UPDATE_MS) {
            notifyStatus();
            _lastStatusMs = now;
        }
    }
}

// ── notifyMessage ──────────────────────────────────────────────────
void BLECompanion::notifyMessage(const InboxMessage& msg) {
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
    _msgInChar->notify();

    OMS_LOG("BLE", "Notified message from %s (%u bytes)", msg.sender, (unsigned)len);
}

// ── notifyStatus ──────────────────────────────────────────────────
void BLECompanion::notifyStatus() {
    if (!_statusChar) return;

    uint8_t buf[64];
    size_t len = 0;
    buildStatusPayload(buf, len);
    _statusChar->setValue(buf, len);
    _statusChar->notify();
}

// ── buildStatusPayload ────────────────────────────────────────────
void BLECompanion::buildStatusPayload(uint8_t* buf, size_t& len) {
    // Binary status payload:
    //   [2B battery mV] [1B battery%] [1B RSSI] [4B uptime sec]
    //   [2B free heap KB] [2B free PSRAM KB] [1B node count]
    //   [1B channel] [1B region code]
    // Total: 15 bytes

    len = 0;

    // Battery voltage (mV)
    uint16_t battMv = 0;
    if (MeshService::instance().initialized()) {
        battMv = (uint16_t)MeshService::instance().board().getBattMilliVolts();
    }
    buf[len++] = battMv & 0xFF;
    buf[len++] = (battMv >> 8) & 0xFF;

    // Battery percentage (rough: 4200mV=100%, 3200mV=0%)
    uint8_t battPct = 0;
    if (battMv > 3200) {
        battPct = (uint8_t)((battMv - 3200) * 100 / 1000);
        if (battPct > 100) battPct = 100;
    }
    buf[len++] = battPct;

    // RSSI (signed, cast to uint8_t)
    int rssi = MeshService::instance().rssi();
    buf[len++] = (uint8_t)(int8_t)rssi;

    // Uptime in seconds
    uint32_t uptimeSec = millis() / 1000;
    buf[len++] = uptimeSec & 0xFF;
    buf[len++] = (uptimeSec >> 8) & 0xFF;
    buf[len++] = (uptimeSec >> 16) & 0xFF;
    buf[len++] = (uptimeSec >> 24) & 0xFF;

    // Free heap (KB)
    uint32_t freeHeap = ESP.getFreeHeap() / 1024;
    buf[len++] = freeHeap & 0xFF;
    buf[len++] = (freeHeap >> 8) & 0xFF;

    // Free PSRAM (KB)
    uint32_t freePsram = ESP.getFreePsram() / 1024;
    buf[len++] = freePsram & 0xFF;
    buf[len++] = (freePsram >> 8) & 0xFF;

    // Node count (placeholder)
    buf[len++] = 0;

    // Current channel
    buf[len++] = (uint8_t)config::get().channel;

    // Region code (index into region table)
    buf[len++] = 0;  // TODO: encode region index
}

// ── handleConfigWrite ─────────────────────────────────────────────
void BLECompanion::handleConfigWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.empty()) return;

    OMS_LOG("BLE", "Config write: %u bytes", (unsigned)value.length());

    // Parse key=value pairs, one per line
    const char* str = value.c_str();
    const char* line = str;

    while (line && *line) {
        // Find end of line
        const char* eol = strchr(line, '\n');
        size_t lineLen = eol ? (eol - line) : strlen(line);

        // Find '=' separator
        const char* eq = (const char*)memchr(line, '=', lineLen);
        if (eq) {
            size_t keyLen = eq - line;
            size_t valLen = lineLen - keyLen - 1;

            char key[32] = {0};
            char val[64] = {0};
            if (keyLen < sizeof(key)) memcpy(key, line, keyLen);
            if (valLen < sizeof(val)) memcpy(val, eq + 1, valLen);

            // Apply known config keys
            if (strcmp(key, "callsign") == 0) {
                config::setCallsign(val);
                OMS_LOG("BLE", "Config: callsign=%s", val);
            } else if (strcmp(key, "region") == 0) {
                config::setRegion(val);
                OMS_LOG("BLE", "Config: region=%s", val);
            } else if (strcmp(key, "channel") == 0) {
                const_cast<Config&>(config::get()).channel = atoi(val);
                OMS_LOG("BLE", "Config: channel=%s", val);
            } else if (strcmp(key, "brightness") == 0) {
                int br = atoi(val);
                if (br >= 0 && br <= 255) {
                    const_cast<Config&>(config::get()).brightness = br;
                    Board::instance().setBacklight(true);
                    OMS_LOG("BLE", "Config: brightness=%d", br);
                }
            } else if (strcmp(key, "sound") == 0) {
                bool snd = (atoi(val) != 0);
                const_cast<Config&>(config::get()).notifySound = snd;
                oms::Notification::instance().setSoundEnabled(snd);
                OMS_LOG("BLE", "Config: sound=%s", snd ? "on" : "off");
            }
        }

        // Move to next line
        line = eol ? eol + 1 : nullptr;
    }

    // Save config to SPIFFS
    config::save();

    // Update the config read characteristic with new values
    if (_cfgReadChar) {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign,
                 config::get().radioRegion,
                 config::get().channel,
                 config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar->setValue((uint8_t*)cfgBuf, strlen(cfgBuf));
    }
}

// ── handleMessageWrite ────────────────────────────────────────────
void BLECompanion::handleMessageWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.empty()) return;

    OMS_LOG("BLE", "Message write: %u bytes", (unsigned)value.length());

    // Format: [1 byte channel_id] [N bytes text]
    // If first byte is 0xFF, it's a direct message (not yet implemented)
    if (value.length() < 2) return;

    uint8_t channelId = value[0];
    const char* text = value.c_str() + 1;
    size_t textLen = value.length() - 1;

    // Ensure null-terminated
    char textBuf[MSG_MAX_LEN + 1];
    if (textLen > MSG_MAX_LEN) textLen = MSG_MAX_LEN;
    memcpy(textBuf, text, textLen);
    textBuf[textLen] = '\0';

    // Send via MeshService
    if (channelId == 0xFF) {
        // Direct message — destination key would need to be encoded
        // For now, treat as channel message on channel 0
        MeshService::instance().sendChannel("public", textBuf);
    } else {
        // Channel message
        const char* channelName = (channelId == 0) ? "public" : "ch1";
        MeshService::instance().sendChannel(channelName, textBuf);
    }

    OMS_LOG("BLE", "Message sent to channel %d: %s", channelId, textBuf);
}

// ── setEnabled ────────────────────────────────────────────────────
void BLECompanion::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled && _advertising && _server) {
        _server->getAdvertising()->stop();
        _advertising = false;
        OMS_LOG("BLE", "Advertising stopped (disabled)");
    } else if (enabled && !_advertising && !_connected && _server) {
        _server->startAdvertising();
        _advertising = true;
        OMS_LOG("BLE", "Advertising restarted");
    }
}

}  // namespace oms