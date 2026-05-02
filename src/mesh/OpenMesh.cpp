// OpenMeshOS — OpenMesh.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshCore callback implementations for OpenMeshOS.
// Received messages are pushed into MessageBus for the UI to consume.

#include "OpenMesh.h"
#include "MessageBus.h"
#include "../utils/Log.h"

namespace oms {

void OpenMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                            uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) {
    OMS_LOG("Mesh", "Advert from %02X%02X%02X%02X (ts=%u, data=%u bytes)",
            id.pub_key[0], id.pub_key[1], id.pub_key[2], id.pub_key[3],
            timestamp, (unsigned)app_data_len);
}

void OpenMesh::onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx,
                              const uint8_t* secret, uint8_t* data, size_t len) {
    if (type == PAYLOAD_TYPE_TXT_MSG && data && len > 0) {
        // Ensure null-terminated for logging
        char text[256];
        size_t copyLen = len < sizeof(text) - 1 ? len : sizeof(text) - 1;
        memcpy(text, data, copyLen);
        text[copyLen] = '\0';
        OMS_LOG("Mesh", "DM: \"%s\" (%u bytes)", text, (unsigned)len);

        // Push to inbox for UI
        InboxMessage msg = {};
        msg.kind = MsgKind::DirectMessage;
        msg.channel_id = 0;
        // Use sender index as short identifier
        snprintf(msg.sender, sizeof(msg.sender), "DM-%d", sender_idx);
        copyLen = len < MSG_MAX_LEN ? len : MSG_MAX_LEN;
        memcpy(msg.text, data, copyLen);
        msg.text[copyLen] = '\0';
        msg.timestamp = millis();
        MessageBus::inbox().push(msg);
    }
}

void OpenMesh::onGroupDataRecv(mesh::Packet* packet, uint8_t type,
                               const mesh::GroupChannel& channel, uint8_t* data, size_t len) {
    if (data && len > 0) {
        char text[256];
        size_t copyLen = len < sizeof(text) - 1 ? len : sizeof(text) - 1;
        memcpy(text, data, copyLen);
        text[copyLen] = '\0';
        OMS_LOG("Mesh", "Group: \"%s\" (%u bytes, type=%u)", text, (unsigned)len, type);

        if (type == PAYLOAD_TYPE_TXT_MSG) {
            // Push to inbox for UI
            InboxMessage msg = {};
            msg.kind = MsgKind::GroupChannel;
            msg.channel_id = channel.hash[0];  // use first byte of hash as channel ID
            snprintf(msg.sender, sizeof(msg.sender), "CH%02X", channel.hash[0]);
            size_t msgCopyLen = len < MSG_MAX_LEN ? len : MSG_MAX_LEN;
            memcpy(msg.text, data, msgCopyLen);
            msg.text[msgCopyLen] = '\0';
            msg.timestamp = millis();
            MessageBus::inbox().push(msg);
        }
    }
}

void OpenMesh::onAckRecv(mesh::Packet* packet, uint32_t ack_crc) {
    OMS_LOG("Mesh", "ACK received (crc=%08X)", ack_crc);
}

bool OpenMesh::allowPacketForward(const mesh::Packet* packet) {
    // Enable packet forwarding (transport mode) by default
    // This makes our node act as a repeater for the mesh
    return true;
}

}  // namespace oms