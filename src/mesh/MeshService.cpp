// OpenMeshOS — MeshService: bridges MeshCore library to our app
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// MeshService wraps the MeshCore C++ library into a friendly API.
// It owns the radio, identity, and message dispatch loop.

#include "MeshService.h"
#include "../hardware/Board.h"
#include "../utils/Config.h"
#include "../utils/Log.h"
#include <Mesh.h>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
static MeshService s_mesh;

MeshService& MeshService::instance() {
    return s_mesh;
}

// ── init ───────────────────────────────────────────────────────────
void MeshService::init() {
    OMS_LOG("Mesh", "Initialising MeshCore stack");

    // TODO: Create our MainBoard subclass that implements mesh::MainBoard
    //       with T-Deck specific battery/temperature/reboot callbacks.
    // TODO: Create RTCClock subclass synced from GPS or NTP.
    // TODO: Load identity (private key) from SPIFFS or generate new.
    // TODO: Configure radio region (EU868 / US915 etc) from Config.
    // TODO: Start MeshCore loop.

    _initialized = true;
    OMS_LOG("Mesh", "MeshCore ready");
}

// ── tick ───────────────────────────────────────────────────────────
void MeshService::tick() {
    if (!_initialized) return;

    // Process incoming packets, housekeeping, advert handling etc.
    // TODO: call mesh->loop() equivalent
}

// ── Message API ────────────────────────────────────────────────────
bool MeshService::sendChannel(const char* channel, const char* text) {
    // TODO: route through MeshCore
    (void)channel; (void)text;
    return false;
}

bool MeshService::sendDirect(const uint8_t* pubkey, const char* text) {
    // TODO: route through MeshCore encrypted DM
    (void)pubkey; (void)text;
    return false;
}

uint16_t MeshService::hopCount() const {
    // TODO: return from MeshCore state
    return 0;
}

int MeshService::rssi() const {
    // TODO: return last RSSI from SX1262
    return 0;
}

}  // namespace oms