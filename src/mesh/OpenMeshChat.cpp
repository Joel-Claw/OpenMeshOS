// OpenMeshOS — OpenMeshChat.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Full MeshCore integration: identity, contacts, channels, messaging.
// This is the bridge between MeshCore's protocol stack and OpenMeshOS UI.

#include "OpenMeshChat.h"
#include "../utils/Config.h"
#include "../utils/Log.h"
#include <SPIFFS.h>

namespace oms {

// ── Constructor ────────────────────────────────────────────────────
OpenMeshChat::OpenMeshChat(mesh::Radio& radio, mesh::MillisecondClock& ms, StdRNG& rng,
                           mesh::RTCClock& rtc, StaticPoolPacketManager& pktMgr,
                           SimpleMeshTables& tables)
    : BaseChatMesh(radio, ms, rng, rtc, pktMgr, tables)
{
}

// ── begin — called once after radio init ───────────────────────────
void OpenMeshChat::begin(fs::FS& fs) {
    _fs = &fs;

    BaseChatMesh::begin();

    // Load or generate identity
    IdentityStore store(*_fs, "/identity");
    store.begin();

    const Config& cfg = config::get();
    strncpy(_callsign, cfg.callsign, sizeof(_callsign) - 1);
    _callsign[sizeof(_callsign) - 1] = '\0';

    if (!store.load("_main", self_id, _callsign, sizeof(_callsign))) {
        OMS_LOG("Mesh", "No identity found, generating new keypair");

        // Generate identity using RNG
        self_id = mesh::LocalIdentity(getRNG());

        // Avoid reserved key prefixes (0x00 and 0xFF)
        int count = 0;
        while (count < 10 && (self_id.pub_key[0] == 0x00 || self_id.pub_key[0] == 0xFF)) {
            self_id = mesh::LocalIdentity(getRNG());
            count++;
        }

        store.save("_main", self_id, _callsign);
        OMS_LOG("Mesh", "New identity created: %s", _callsign);
    } else {
        OMS_LOG("Mesh", "Identity loaded: %s", _callsign);
    }

    // Load persisted contacts
    loadContacts();

    // Set up public group channel
    _publicChannel = addChannel("Public", PUBLIC_GROUP_PSK);

    OMS_LOG("Mesh", "Chat ready (callsign=%s)", _callsign);
}

// ── loop — called every main loop iteration ───────────────────────
void OpenMeshChat::loop() {
    BaseChatMesh::loop();
}

// ── Identity ───────────────────────────────────────────────────────
void OpenMeshChat::setCallsign(const char* name) {
    strncpy(_callsign, name, sizeof(_callsign) - 1);
    _callsign[sizeof(_callsign) - 1] = '\0';
    config::setCallsign(name);
    OMS_LOG("Mesh", "Callsign set to: %s", _callsign);
}

// ── Messaging ─────────────────────────────────────────────────────
int OpenMeshChat::sendDirectMessage(const ContactInfo& recipient, const char* text) {
    uint32_t est_timeout;
    int result = sendMessage(recipient, getRTCClock()->getCurrentTime(), 0, text, _expectedAckCrc, est_timeout);
    if (result != MSG_SEND_FAILED) {
        _lastMsgSent = _ms->getMillis();
        OMS_LOG("Mesh", "DM sent to %s (%s)", recipient.name,
                result == MSG_SEND_SENT_DIRECT ? "DIRECT" : "FLOOD");
    } else {
        OMS_LOG("Mesh", "DM send failed to %s", recipient.name);
    }
    return result;
}

bool OpenMeshChat::sendChannelMessage(const char* text) {
    if (!_publicChannel) return false;

    uint32_t timestamp = getRTCClock()->getCurrentTime();
    uint8_t temp[5 + MAX_TEXT_LEN + 32];

    memcpy(temp, &timestamp, 4);
    temp[4] = 0;  // attempt and flags

    // Format: "Callsign: message"
    snprintf((char*)&temp[5], sizeof(temp) - 5, "%s: %s", _callsign, text);

    int len = strlen((char*)&temp[5]);
    auto pkt = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, _publicChannel->channel, temp, 5 + len);
    if (pkt) {
        sendFlood(pkt);
        OMS_LOG("Mesh", "Channel msg sent: %s", text);
        return true;
    }
    OMS_LOG("Mesh", "Channel msg send failed");
    return false;
}

bool OpenMeshChat::sendAdvert(int delay_millis) {
    auto pkt = createSelfAdvert(_callsign);
    if (pkt) {
        sendFlood(pkt, delay_millis);
        OMS_LOG("Mesh", "Advert sent");
        return true;
    }
    return false;
}

// ── Contacts ───────────────────────────────────────────────────────
int OpenMeshChat::getContactCount() const {
    return getNumContacts();
}

bool OpenMeshChat::getContactByIdx(int idx, ContactInfo& out) {
    ContactsIterator iter;
    return iter.hasNext(const_cast<OpenMeshChat*>(this), out);
}

ContactInfo* OpenMeshChat::findContact(const char* namePrefix) {
    return searchContactsByPrefix(namePrefix);
}

// ── BaseChatMesh overrides ──────────────────────────────────────────

void OpenMeshChat::onDiscoveredContact(ContactInfo& contact, bool is_new, uint8_t path_len, const uint8_t* path) {
    OMS_LOG("Mesh", "Advert from %s (type=%d, path_len=%d, %s)",
            contact.name, contact.type, path_len, is_new ? "NEW" : "known");
    saveContacts();
}

void OpenMeshChat::onContactPathUpdated(const ContactInfo& contact) {
    OMS_LOG("Mesh", "Path updated: %s (len=%d)", contact.name, contact.out_path_len);
    saveContacts();
}

ContactInfo* OpenMeshChat::processAck(const uint8_t* data) {
    if (memcmp(data, &_expectedAckCrc, 4) == 0) {
        uint32_t rtt = _ms->getMillis() - _lastMsgSent;
        OMS_LOG("Mesh", "ACK received (RTT: %lu ms)", rtt);
        _expectedAckCrc = 0;
        return nullptr;
    }
    return nullptr;
}

void OpenMeshChat::onMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char* text) {
    OMS_LOG("Mesh", "DM from %s: %s", from.name, text);

    if (_msgCallback) {
        ChatMessage msg;
        strncpy(msg.sender, from.name, sizeof(msg.sender) - 1);
        msg.sender[sizeof(msg.sender) - 1] = '\0';
        strncpy(msg.text, text, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        msg.timestamp = sender_timestamp;
        msg.isDirect = true;
        _msgCallback(msg);
    }
}

void OpenMeshChat::onCommandDataRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char* text) {
    // Not used yet — future command processing
}

void OpenMeshChat::onSignedMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const uint8_t* sender_prefix, const char* text) {
    OMS_LOG("Mesh", "Signed msg from %s: %s", from.name, text);
}

void OpenMeshChat::onChannelMessageRecv(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t timestamp, const char* text) {
    OMS_LOG("Mesh", "Channel msg: %s", text);

    if (_msgCallback) {
        ChatMessage msg;
        // Channel messages formatted as "Sender: text" — try to extract name
        const char* colon = strchr(text, ':');
        if (colon && (colon - text) < 32) {
            size_t nameLen = colon - text;
            memcpy(msg.sender, text, nameLen);
            msg.sender[nameLen] = '\0';
            const char* msgText = colon + 1;
            while (*msgText == ' ') msgText++;
            strncpy(msg.text, msgText, sizeof(msg.text) - 1);
        } else {
            strncpy(msg.sender, "Unknown", sizeof(msg.sender) - 1);
            strncpy(msg.text, text, sizeof(msg.text) - 1);
        }
        msg.sender[sizeof(msg.sender) - 1] = '\0';
        msg.text[sizeof(msg.text) - 1] = '\0';
        msg.timestamp = timestamp;
        msg.isDirect = false;
        _msgCallback(msg);
    }
}

uint8_t OpenMeshChat::onContactRequest(const ContactInfo& contact, uint32_t sender_timestamp, const uint8_t* data, uint8_t len, uint8_t* reply) {
    return 0;  // not handling contact requests yet
}

void OpenMeshChat::onContactResponse(const ContactInfo& contact, const uint8_t* data, uint8_t len) {
    // not supported yet
}

void OpenMeshChat::onSendTimeout() {
    OMS_LOG("Mesh", "Send timeout, no ACK received");
}

uint32_t OpenMeshChat::calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const {
    return SEND_TIMEOUT_BASE_MILLIS + (uint32_t)(FLOOD_SEND_TIMEOUT_FACTOR * pkt_airtime_millis);
}

uint32_t OpenMeshChat::calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const {
    uint8_t path_hash_count = path_len & 63;
    return SEND_TIMEOUT_BASE_MILLIS +
           ((uint32_t)(pkt_airtime_millis * DIRECT_SEND_PERHOP_FACTOR + DIRECT_SEND_PERHOP_EXTRA_MILLIS) * (path_hash_count + 1));
}

// ── Contact persistence ────────────────────────────────────────────
void OpenMeshChat::saveContacts() {
    if (!_fs) return;

    File file = _fs->open("/contacts", "w", true);
    if (!file) {
        OMS_LOG("Mesh", "Failed to save contacts");
        return;
    }

    ContactsIterator iter;
    ContactInfo c;
    uint8_t unused = 0;
    uint32_t reserved = 0;

    while (iter.hasNext(this, c)) {
        bool ok = (file.write(c.id.pub_key, 32) == 32);
        ok = ok && (file.write((uint8_t*)&c.name, 32) == 32);
        ok = ok && (file.write(&c.type, 1) == 1);
        ok = ok && (file.write(&c.flags, 1) == 1);
        ok = ok && (file.write(&unused, 1) == 1);
        ok = ok && (file.write((uint8_t*)&reserved, 4) == 4);
        ok = ok && (file.write((uint8_t*)&c.out_path_len, 1) == 1);
        ok = ok && (file.write((uint8_t*)&c.last_advert_timestamp, 4) == 4);
        ok = ok && (file.write(c.out_path, 64) == 64);
        if (!ok) break;
    }
    file.close();
}

void OpenMeshChat::loadContacts() {
    if (!_fs) return;
    if (!_fs->exists("/contacts")) return;

    File file = _fs->open("/contacts");
    if (!file) return;

    while (true) {
        ContactInfo c;
        uint8_t pub_key[32];
        uint8_t unused;
        uint32_t reserved;

        bool ok = (file.read(pub_key, 32) == 32);
        ok = ok && (file.read((uint8_t*)&c.name, 32) == 32);
        ok = ok && (file.read(&c.type, 1) == 1);
        ok = ok && (file.read(&c.flags, 1) == 1);
        ok = ok && (file.read(&unused, 1) == 1);
        ok = ok && (file.read((uint8_t*)&reserved, 4) == 4);
        ok = ok && (file.read((uint8_t*)&c.out_path_len, 1) == 1);
        ok = ok && (file.read((uint8_t*)&c.last_advert_timestamp, 4) == 4);
        ok = ok && (file.read(c.out_path, 64) == 64);

        c.gps_lat = c.gps_lon = 0;

        if (!ok) break;
        c.id = mesh::Identity(pub_key);
        c.lastmod = 0;
        if (!addContact(c)) break;  // table full
    }
    file.close();
    OMS_LOG("Mesh", "Loaded %d contacts", getNumContacts());
}

}  // namespace oms