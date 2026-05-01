// OpenMeshOS — OpenMesh.h
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Thin subclass of MeshCore's Mesh so we can access the protected constructor.

#pragma once

#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>

namespace oms {

class OpenMesh : public mesh::Mesh {
public:
    OpenMesh(mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng,
             mesh::RTCClock& rtc, mesh::PacketManager& mgr, mesh::MeshTables& tables)
        : mesh::Mesh(radio, ms, rng, rtc, mgr, tables) {}

    // Expose callback overrides for our app
    void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                      uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) override;
    void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx,
                        const uint8_t* secret, uint8_t* data, size_t len) override;
    void onGroupDataRecv(mesh::Packet* packet, uint8_t type,
                         const mesh::GroupChannel& channel, uint8_t* data, size_t len) override;
    void onAckRecv(mesh::Packet* packet, uint32_t ack_crc) override;

    bool allowPacketForward(const mesh::Packet* packet) override;
};

}  // namespace oms