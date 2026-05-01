// OpenMeshOS — MeshService: bridges MeshCore library to our app
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshService wraps the MeshCore C++ library into a friendly API.
// It owns the radio, identity, and message dispatch loop.

#include "MeshService.h"
#include "TDeckBoard.h"
#include "TDeckClock.h"
#include "../hardware/Board.h"
#include "../utils/Config.h"
#include "../utils/Log.h"

#include <SPIFFS.h>

// LoRa radio config macros (needed by CustomSX1262.h std_init)
#ifndef LORA_FREQ
#define LORA_FREQ         868.0f
#endif
#ifndef LORA_BW
#define LORA_BW           125.0f
#endif
#ifndef LORA_SF
#define LORA_SF           9
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER     22
#endif
#ifndef LORA_CR
#define LORA_CR           5
#endif

// Ensure ed25519 is linked (MeshCore depends on it but LDF can't trace through library archives)
#define ED25519_NO_SEED 1
#include <ed_25519.h>

// MeshCore includes (must come after LORA_* macros)
#include "OpenMesh.h"
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/IdentityStore.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/radiolib/CustomSX1262.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/radiolib/SX126xReset.h>

// T-Deck SX1262 pin config (from LilyGo schematic)
#define SX1262_CS    9
#define SX1262_DIO1  14
#define SX1262_RST   12
#define SX1262_BUSY  13

namespace oms {

// ── Static instance ────────────────────────────────────────────────
static MeshService s_mesh;

MeshService& MeshService::instance() {
    return s_mesh;
}

// ── MeshCore components ─────────────────────────────────────────────
static TDeckBoard*      s_board     = nullptr;
static TDeckClock*      s_clock     = nullptr;
static mesh::LocalIdentity* s_identity = nullptr;
static IdentityStore*   s_idStore  = nullptr;
static SimpleMeshTables* s_tables   = nullptr;
static StaticPoolPacketManager* s_pktMgr = nullptr;
static OpenMesh*        s_meshCore = nullptr;
static CustomSX1262Wrapper* s_radio = nullptr;
static ArduinoMillis*    s_millis   = nullptr;
static RadioNoiseListener* s_rng   = nullptr;

// SX1262 radio (MeshCore's CustomSX1262 wraps RadioLib's SX1262)
static SPIClass* s_loraSpi = nullptr;
static CustomSX1262* s_sx1262 = nullptr;

// ── Region frequency config ────────────────────────────────────────
struct RadioRegion {
    const char* name;
    float freqMHz;
    float bwMHz;
    uint8_t sf;
    uint8_t cr;
};

static const RadioRegion s_regions[] = {
    {"EU868",  868.0f, 125.0f, 9, 5},
    {"US915",  915.0f, 125.0f, 9, 5},
    {"AU915",  915.0f, 125.0f, 9, 5},
    {"AS923",  923.0f, 125.0f, 9, 5},
    {"KR920",  920.0f, 125.0f, 9, 5},
    {"IN865",  865.0f, 125.0f, 9, 5},
};

static const int s_numRegions = sizeof(s_regions) / sizeof(s_regions[0]);

static const RadioRegion* findRegion(const char* name) {
    for (int i = 0; i < s_numRegions; i++) {
        if (strncmp(s_regions[i].name, name, sizeof(s_regions[i].name)) == 0) {
            return &s_regions[i];
        }
    }
    return &s_regions[0];  // default EU868
}

// ── init ───────────────────────────────────────────────────────────
void MeshService::init() {
    OMS_LOG("Mesh", "Initialising MeshCore stack");

    // ── Board and clock ───────────────────────────────────────────
    _board = new TDeckBoard();
    _clock = new TDeckClock();

    OMS_LOG("Mesh", "Board: %s", _board->getManufacturerName());
    OMS_LOG("Mesh", "ADC multiplier: %.2f", _board->getAdcMultiplier());

    // ── SPI and Radio ────────────────────────────────────────────
    // Use HSPI for LoRa (VSPI is used by TFT)
    s_loraSpi = new SPIClass(HSPI);
    s_loraSpi->begin(SX1262_CS, SX1262_DIO1, -1, SX1262_RST);

    // Create CustomSX1262 with T-Deck pin config
    auto* mod = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, *s_loraSpi);
    s_sx1262 = new CustomSX1262(mod);

    // Get region config
    const RadioRegion* region = findRegion(config::get().radioRegion);
    OMS_LOG("Mesh", "Region: %s, freq: %.1f MHz", region->name, region->freqMHz);

    // Set DIO2 as RF switch (T-Deck hardware config)
    s_sx1262->setDio2AsRfSwitch(true);

    // Init radio via std_init (uses LORA_* macros defined above)
    bool radioOk = s_sx1262->std_init(s_loraSpi);

    if (!radioOk) {
        OMS_LOG("Mesh", "ERROR: SX1262 std_init failed");
        _initialized = false;
        return;
    }

    s_sx1262->setCRC(1);
    OMS_LOG("Mesh", "SX1262 radio initialised on %s", region->name);

    // ── CustomSX1262Wrapper (concrete RadioLibWrapper) ─────────────
    s_radio = new CustomSX1262Wrapper(*s_sx1262, *_board);

    // ── RNG (radio noise for key generation) ──────────────────────
    s_rng = new RadioNoiseListener(*s_sx1262);

    // ── MillisecondClock ─────────────────────────────────────────
    s_millis = new ArduinoMillis();

    // ── Identity ──────────────────────────────────────────────────
    // SPIFFS must be initialised before this (done in setup())
    s_idStore = new IdentityStore(SPIFFS, "/mesh");
    s_idStore->begin();

    s_identity = new mesh::LocalIdentity();
    const char* idName = config::get().callsign;

    // Try loading existing identity
    char displayName[32] = {0};
    bool loaded = s_idStore->load(idName, *s_identity, displayName, sizeof(displayName));

    if (!loaded) {
        OMS_LOG("Mesh", "No identity found for '%s', generating new keypair", idName);
        mesh::LocalIdentity newId(s_rng);
        *s_identity = newId;
        s_idStore->save(idName, *s_identity, idName);
        OMS_LOG("Mesh", "New identity saved as '%s'", idName);
    } else {
        OMS_LOG("Mesh", "Identity loaded: '%s'", displayName);
    }

    // ── Packet manager & dedup tables ─────────────────────────────
    s_pktMgr = new StaticPoolPacketManager(64);   // 64-packet pool
    s_tables  = new SimpleMeshTables();

    // ── Create MeshCore Mesh object ───────────────────────────────
    s_meshCore = new OpenMesh(
        *s_radio,
        *s_millis,
        *s_rng,
        *_clock,
        *s_pktMgr,
        *s_tables
    );

    s_meshCore->self_id = *s_identity;
    s_meshCore->begin();

    _initialized = true;
    OMS_LOG("Mesh", "MeshCore ready (region: %s, id: %s)", region->name, config::get().callsign);
}

// ── tick ───────────────────────────────────────────────────────────
void MeshService::tick() {
    if (!_initialized) return;

    _clock->tick();
    s_meshCore->loop();
}

// ── Message API ────────────────────────────────────────────────────
bool MeshService::sendChannel(const char* channel, const char* text) {
    if (!_initialized) return false;

    // Create a flood-routed text datagram for public channel
    mesh::Identity broadcast;  // zero-initialised = broadcast
    auto* pkt = s_meshCore->createDatagram(
        PAYLOAD_TYPE_TXT_MSG,
        broadcast,
        nullptr,
        (const uint8_t*)text,
        strlen(text)
    );

    if (!pkt) return false;

    s_meshCore->sendFlood(pkt);
    return true;
}

bool MeshService::sendDirect(const uint8_t* pubkey, const char* text) {
    if (!_initialized || !pubkey) return false;

    mesh::Identity dest(pubkey);

    // Calculate shared secret for encrypted DM
    uint8_t secret[PUB_KEY_SIZE];
    s_identity->calcSharedSecret(secret, dest);

    auto* pkt = s_meshCore->createDatagram(
        PAYLOAD_TYPE_TXT_MSG,
        dest,
        secret,
        (const uint8_t*)text,
        strlen(text)
    );

    if (!pkt) return false;

    // Use flood routing for now (path discovery comes later)
    s_meshCore->sendFlood(pkt);
    return true;
}

uint16_t MeshService::hopCount() const {
    // TODO: track from MeshCore advert timestamps
    return 0;
}

int MeshService::rssi() const {
    if (s_radio) {
        return (int)s_radio->getLastRSSI();
    }
    return 0;
}

}  // namespace oms