// OpenMeshOS — BLECompanion.cpp (ESP32 implementation)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// ESP32 BLE companion app service implementation.
// Provides config, messaging, and status over BLE using ESP32 Arduino BLE.

#include "BLECompanion.h"
#include "MeshService.h"
#include "../hardware/IBoard.h"
#include "../hardware/Notification.h"
#include "../hardware/PlatformCompat.h"
#include "../utils/Log.h"
#include "../utils/Config.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_system.h>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
BLECompanionESP32& BLECompanionESP32::instance() {
    static BLECompanionESP32 s_ble;
    return s_ble;
}

// ── Server callbacks (connection/disconnection) ──────────────────
class BLECompanionESP32::ServerCallbacks : public BLEServerCallbacks {
public:
    ServerCallbacks(BLECompanionESP32& parent) : _parent(parent) {}

    void onConnect(BLEServer* pServer) override {
        OMS_LOG("BLE", "Companion connected");
        _parent._connected = true;
        pServer->getAdvertising()->stop();
        _parent._advertising = false;
    }

    void onDisconnect(BLEServer* pServer) override {
        OMS_LOG("BLE", "Companion disconnected");
        _parent._connected = false;
        pServer->startAdvertising();
        _parent._advertising = true;
    }

private:
    BLECompanionESP32& _parent;
};

// ── Config write callback ────────────────────────────────────────
class BLECompanionESP32::ConfigWriteCallback : public BLECharacteristicCallbacks {
public:
    ConfigWriteCallback(BLECompanionESP32& parent) : _parent(parent) {}
    void onWrite(BLECharacteristic* pChar) override {
        _parent.handleConfigWrite(pChar);
    }
private:
    BLECompanionESP32& _parent;
};

// ── Message write callback ───────────────────────────────────────
class BLECompanionESP32::MessageWriteCallback : public BLECharacteristicCallbacks {
public:
    MessageWriteCallback(BLECompanionESP32& parent) : _parent(parent) {}
    void onWrite(BLECharacteristic* pChar) override {
        _parent.handleMessageWrite(pChar);
    }
private:
    BLECompanionESP32& _parent;
};

// ── Firmware update write callback ─────────────────────────────────
class BLECompanionESP32::FirmwareWriteCallback : public BLECharacteristicCallbacks {
public:
    FirmwareWriteCallback(BLECompanionESP32& parent) : _parent(parent) {}
    void onWrite(BLECharacteristic* pChar) override {
        _parent.handleFirmwareWrite(pChar);
    }
private:
    BLECompanionESP32& _parent;
};

// ── init ───────────────────────────────────────────────────────────
void BLECompanionESP32::init() {
    OMS_LOG("BLE", "Initialising BLE companion service (ESP32)");

    char deviceName[24];
    snprintf(deviceName, sizeof(deviceName), "%s%s",
             BLE_DEVICE_PREFIX, config::get().callsign);

    BLEDevice::init(deviceName);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);

    _server = BLEDevice::createServer();
    _server->setCallbacks(new ServerCallbacks(*this));

    _service = _server->createService(BLEUUID(UUID_SERVICE));

    // Config Read
    _cfgReadChar = _service->createCharacteristic(
        BLEUUID(UUID_CFG_READ), BLECharacteristic::PROPERTY_READ);
    _cfgReadChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign, config::get().radioRegion,
                 config::get().channel, config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar->setValue((uint8_t*)cfgBuf, strlen(cfgBuf));
    }

    // Config Write
    _cfgWriteChar = _service->createCharacteristic(
        BLEUUID(UUID_CFG_WRITE), BLECharacteristic::PROPERTY_WRITE);
    _cfgWriteChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    _cfgWriteChar->setCallbacks(new ConfigWriteCallback(*this));

    // Messages Inbound (mesh -> phone)
    _msgInChar = _service->createCharacteristic(
        BLEUUID(UUID_MSG_IN), BLECharacteristic::PROPERTY_NOTIFY);
    _msgInChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    _msgInChar->addDescriptor(new BLE2902());

    // Messages Outbound (phone -> mesh)
    _msgOutChar = _service->createCharacteristic(
        BLEUUID(UUID_MSG_OUT), BLECharacteristic::PROPERTY_WRITE);
    _msgOutChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    _msgOutChar->setCallbacks(new MessageWriteCallback(*this));

    // Device Status
    _statusChar = _service->createCharacteristic(
        BLEUUID(UUID_STATUS),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    _statusChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    _statusChar->addDescriptor(new BLE2902());
    {
        uint8_t statusBuf[64];
        size_t statusLen = 0;
        buildStatusPayload(statusBuf, statusLen);
        _statusChar->setValue(statusBuf, statusLen);
    }

    // Firmware Update (BLE OTA)
    _fwUpdateChar = _service->createCharacteristic(
        BLEUUID(UUID_FW_UPDATE),
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    _fwUpdateChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    _fwUpdateChar->setCallbacks(new FirmwareWriteCallback(*this));
    _fwUpdateChar->addDescriptor(new BLE2902());

    _service->start();

    _server->getAdvertising()->addServiceUUID(BLEUUID(UUID_SERVICE));
    _server->getAdvertising()->setMinPreferred(0x06);
    _server->getAdvertising()->setMinPreferred(0x12);
    _server->startAdvertising();
    _advertising = true;

    OMS_LOG("BLE", "Companion service started, advertising as '%s'", deviceName);
}

// ── tick ───────────────────────────────────────────────────────────
void BLECompanionESP32::tick() {
    if (!_enabled) return;
    if (_connected && _statusChar) {
        uint32_t now = millis();
        if (now - _lastStatusMs >= STATUS_UPDATE_MS) {
            notifyStatus();
            _lastStatusMs = now;
        }
    }
}

// ── notifyMessage ──────────────────────────────────────────────────
void BLECompanionESP32::notifyMessage(const InboxMessage& msg) {
    if (!_connected || !_msgInChar) return;

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
void BLECompanionESP32::notifyStatus() {
    if (!_statusChar) return;
    uint8_t buf[64];
    size_t len = 0;
    buildStatusPayload(buf, len);
    _statusChar->setValue(buf, len);
    _statusChar->notify();
}

// ── buildStatusPayload ────────────────────────────────────────────
void BLECompanionESP32::buildStatusPayload(uint8_t* buf, size_t& len) {
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

    // Use platform abstraction for heap (works on both ESP32 and nRF52)
    uint32_t freeHeap = platform::freeHeap() / 1024;
    buf[len++] = freeHeap & 0xFF;
    buf[len++] = (freeHeap >> 8) & 0xFF;

    // PSRAM is ESP32-only; report 0 on nRF52 (platform::largestFreeBlock as approx)
    uint32_t freePsram = 0;
#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM)
    freePsram = ESP.getFreePsram() / 1024;
#endif
    buf[len++] = freePsram & 0xFF;
    buf[len++] = (freePsram >> 8) & 0xFF;

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
void BLECompanionESP32::handleConfigWrite(BLECharacteristic* pChar) {
    uint32_t now = millis();
    if (now - _lastCfgWriteMs < CFG_WRITE_MIN_INTERVAL_MS) {
        OMS_LOG("BLE", "Config write rate-limited");
        return;
    }
    _lastCfgWriteMs = now;

    std::string value = pChar->getValue();
    if (value.empty()) return;

    OMS_LOG("BLE", "Config write: %u bytes", (unsigned)value.length());

    const char* str = value.c_str();
    const char* line = str;

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
                    oms::theBoard()->setBacklight(true);
                    OMS_LOG("BLE", "Config: brightness=%d", br);
                }
            } else if (strcmp(key, "sound") == 0) {
                bool snd = (atoi(val) != 0);
                const_cast<Config&>(config::get()).notifySound = snd;
                oms::Notification::instance().setSoundEnabled(snd);
                OMS_LOG("BLE", "Config: sound=%s", snd ? "on" : "off");
            } else {
                OMS_LOG("BLE", "Config: unknown key '%s', ignored", key);
            }
        }
        line = eol ? eol + 1 : nullptr;
    }

    config::save();

    if (_cfgReadChar) {
        char cfgBuf[128];
        snprintf(cfgBuf, sizeof(cfgBuf),
                 "callsign=%s\nregion=%s\nchannel=%d\nbrightness=%d\nsound=%d\n",
                 config::get().callsign, config::get().radioRegion,
                 config::get().channel, config::get().brightness,
                 config::get().notifySound ? 1 : 0);
        _cfgReadChar->setValue((uint8_t*)cfgBuf, strlen(cfgBuf));
    }
}

// ── handleMessageWrite ────────────────────────────────────────────
void BLECompanionESP32::handleMessageWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.empty()) return;

    OMS_LOG("BLE", "Message write: %u bytes", (unsigned)value.length());
    if (value.length() < 2) return;

    uint8_t channelId = value[0];
    const char* text = value.c_str() + 1;
    size_t textLen = value.length() - 1;

    char textBuf[MSG_MAX_LEN + 1];
    if (textLen > MSG_MAX_LEN) textLen = MSG_MAX_LEN;
    memcpy(textBuf, text, textLen);
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
void BLECompanionESP32::setEnabled(bool enabled) {
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

// ── handleFirmwareWrite ────────────────────────────────────────────
void BLECompanionESP32::handleFirmwareWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    size_t len = value.length();
    if (len == 0) return;

    const uint8_t* data = (const uint8_t*)value.c_str();

    if (len == 1) {
        uint8_t cmd = data[0];
        if (cmd == 0x00 && _otaInProgress) {
            OMS_LOG("BLE", "OTA end command, applying update");
            if (Update.end(true)) {
                _otaStep = 4;
                notifyOtaProgress(_otaStep, _otaWritten, _otaTotalSize);
                OMS_LOG("BLE", "OTA success! Rebooting in 2s");
                delay(2000);
                ESP.restart();
            } else {
                _otaStep = 0xFF;
                notifyOtaProgress(_otaStep, Update.getError(), _otaTotalSize);
                OMS_LOG("BLE", "OTA end failed: error %u", Update.getError());
                _otaInProgress = false;
            }
        } else if (cmd == 0xFE && _otaInProgress) {
            OMS_LOG("BLE", "OTA aborted by companion");
            Update.abort();
            _otaInProgress = false;
            _otaStep = 0;
            notifyOtaProgress(0xFF, 0, 0);
        }
        return;
    }

    if (!_otaInProgress && len == 4) {
        _otaTotalSize = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        if (_otaTotalSize == 0 || _otaTotalSize > 6400000) {
            OMS_LOG("BLE", "OTA: invalid size %u", (unsigned)_otaTotalSize);
            notifyOtaProgress(0xFF, 0, _otaTotalSize);
            return;
        }
        OMS_LOG("BLE", "OTA starting: %u bytes", (unsigned)_otaTotalSize);
        if (!Update.begin(_otaTotalSize)) {
            OMS_LOG("BLE", "OTA: not enough space");
            notifyOtaProgress(0xFF, 0, _otaTotalSize);
            return;
        }
        _otaInProgress = true;
        _otaWritten = 0;
        _otaStep = 1;
        notifyOtaProgress(_otaStep, 0, _otaTotalSize);
        return;
    }

    if (_otaInProgress) {
        size_t written = Update.write(const_cast<uint8_t*>(data), len);
        _otaWritten += written;
        _otaStep = 2;
        notifyOtaProgress(_otaStep, _otaWritten, _otaTotalSize);
        if (written != len) {
            OMS_LOG("BLE", "OTA write error: wrote %u of %u", (unsigned)written, (unsigned)len);
            _otaStep = 0xFF;
            notifyOtaProgress(_otaStep, _otaWritten, _otaTotalSize);
            Update.abort();
            _otaInProgress = false;
        }
    }
}

// ── notifyOtaProgress ──────────────────────────────────────────────
void BLECompanionESP32::notifyOtaProgress(uint8_t step, uint32_t current, uint32_t total) {
    if (!_fwUpdateChar || !_connected) return;
    uint8_t buf[9];
    buf[0] = step;
    buf[1] = current & 0xFF;
    buf[2] = (current >> 8) & 0xFF;
    buf[3] = (current >> 16) & 0xFF;
    buf[4] = (current >> 24) & 0xFF;
    buf[5] = total & 0xFF;
    buf[6] = (total >> 8) & 0xFF;
    buf[7] = (total >> 16) & 0xFF;
    buf[8] = (total >> 24) & 0xFF;
    _fwUpdateChar->setValue(buf, sizeof(buf));
    _fwUpdateChar->notify();
    OMS_LOG("BLE", "OTA progress: step=%u current=%u total=%u",
            step, (unsigned)current, (unsigned)total);
}

}  // namespace oms

#endif  // ARDUINO_ARCH_ESP32