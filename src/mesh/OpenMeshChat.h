// OpenMeshOS — OpenMeshChat.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore chat layer for OpenMeshOS.  Extends BaseChatMesh to provide
// secure messaging, contact management, and channel support.
// Bridges MeshCore protocol events into the OpenMeshOS UI.
//
// NOTE: This is an alternative higher-level mesh integration.
// The current MeshService uses OpenMesh (Mesh subclass) with manual
// packet handling via MessageBus. OpenMeshChat uses BaseChatMesh
// which adds: contact discovery, group channels, DM with ACKs,
// and identity management on top of raw Mesh.
//
// To switch to OpenMeshChat, MeshService would need to be refactored
// to use this class instead of OpenMesh. Benefits:
//   - Automatic contact discovery and persistence
//   - Group channel support with PSK encryption
//   - Direct message ACKs and timeout handling
//   - Cleaner separation: chat logic here, UI in screens
//
// This code compiles but is NOT YET INTEGRATED into the main build.
// It will be used when we upgrade from raw Mesh to chat-level MeshCore.

#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/IdentityStore.h>

namespace oms {

// ── LoRa defaults (EU868) — overridden by MeshCore LORA_* defines ────
#ifndef OMS_LORA_FREQ
  #define OMS_LORA_FREQ   868.0
#endif
#ifndef OMS_LORA_TX_POWER
  #define OMS_LORA_TX_POWER  22
#endif

// ── Chat timeouts ─────────────────────────────────────────────────
#define SEND_TIMEOUT_BASE_MILLIS          500
#define FLOOD_SEND_TIMEOUT_FACTOR         16.0f
#define DIRECT_SEND_PERHOP_FACTOR         6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS  250

// ── Public group channel PSK (MeshCore community channel) ─────────
#define PUBLIC_GROUP_PSK  "izOH6cXN6mrJ5e26oRXNcg=="

// ── Message struct for UI callback ─────────────────────────────────
struct ChatMessage {
    char sender[33];        // sender name
    char text[161];         // message text
    uint32_t timestamp;     // unix timestamp
    bool isDirect;          // true = DM, false = channel
};

using MessageCallback = void(*)(const ChatMessage& msg);

class OpenMeshChat : public BaseChatMesh {
public:
    OpenMeshChat(mesh::Radio& radio, mesh::MillisecondClock& ms, StdRNG& rng,
                 mesh::RTCClock& rtc, StaticPoolPacketManager& pktMgr,
                 SimpleMeshTables& tables);

    void begin(fs::FS& fs);
    void loop();

    // ── Identity ──────────────────────────────────────────────────
    const char* getCallsign() const { return _callsign; }
    void setCallsign(const char* name);

    // ── Messaging ────────────────────────────────────────────────
    int sendDirectMessage(const ContactInfo& recipient, const char* text);
    bool sendChannelMessage(const char* text);
    bool sendAdvert(int delay_millis = 1200);

    // ── Contacts ──────────────────────────────────────────────────
    int getContactCount() const;
    bool getContactByIdx(int idx, ContactInfo& out);
    ContactInfo* findContact(const char* namePrefix);

    // ── Channel ───────────────────────────────────────────────────
    ChannelDetails* getPublicChannel() { return _publicChannel; }

    // ── UI callback ───────────────────────────────────────────────
    void onMessageReceived(MessageCallback cb) { _msgCallback = cb; }

    // ── Contact persistence ──────────────────────────────────────
    void saveContacts();
    void loadContacts();

protected:
    // ── BaseChatMesh virtual overrides ───────────────────────────
    void onDiscoveredContact(ContactInfo& contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
    void onContactPathUpdated(const ContactInfo& contact) override;
    ContactInfo* processAck(const uint8_t* data) override;
    void onMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char* text) override;
    void onCommandDataRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const char* text) override;
    void onSignedMessageRecv(const ContactInfo& from, mesh::Packet* pkt, uint32_t sender_timestamp, const uint8_t* sender_prefix, const char* text) override;
    void onChannelMessageRecv(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t timestamp, const char* text) override;
    uint8_t onContactRequest(const ContactInfo& contact, uint32_t sender_timestamp, const uint8_t* data, uint8_t len, uint8_t* reply) override;
    void onContactResponse(const ContactInfo& contact, const uint8_t* data, uint8_t len) override;
    void onSendTimeout() override;
    uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
    uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;

    // Allow all packets to be forwarded (we're a chat node, not a repeater)
    bool allowPacketForward(const mesh::Packet* packet) override { return true; }

private:
    fs::FS* _fs = nullptr;
    char _callsign[33] = "OMS-0001";
    ChannelDetails* _publicChannel = nullptr;
    uint32_t _expectedAckCrc = 0;
    unsigned long _lastMsgSent = 0;
    MessageCallback _msgCallback = nullptr;
};

}  // namespace oms