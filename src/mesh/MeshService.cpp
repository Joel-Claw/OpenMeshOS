// OpenMeshOS — MeshService: bridges MeshCore library to our app
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// MeshService wraps the MeshCore C++ library into a friendly API.
// It owns the radio, identity, and message dispatch loop.
//
// Radio pin configuration comes from IBoard::loraConfig(), not hardcoded
// defines. This allows different devices (T-Deck, Heltec V3, etc.)
// to provide their own pin mappings and radio parameters.
//
// Uses OpenMeshChat (BaseChatMesh subclass) for higher-level mesh features:
//   - Automatic contact discovery and persistence
//   - Group channel support with PSK encryption
//   - Direct message ACKs and timeout handling
//   - Cleaner separation: chat logic in OpenMeshChat, UI in screens

#include "MeshService.h"
#include "TDeckBoard.h"
#include "TDeckClock.h"
#include "NodeTracker.h"
#include "../hardware/IBoard.h"
#include "../utils/Config.h"
#include "../utils/Log.h"

#include <SPIFFS.h>
#include <esp_system.h>  // esp_random()

// LoRa radio config macros — these are now fallback defaults only.
// MeshService::init() uses IBoard::loraConfig() for actual values,
// which are resolved at runtime from the board implementation.
// The macros are kept here as documentation of the default values
// and as fallbacks if a board doesn't override them.
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
#include "OpenMeshChat.h"
#include "MessageBus.h"
#include "NodeTracker.h"
#include "../hardware/Notification.h"
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/IdentityStore.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/radiolib/CustomSX1262.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/radiolib/SX126xReset.h>

namespace oms {

// ── Static instance ────────────────────────────────────────────────
static MeshService s_mesh;

MeshService& MeshService::instance() {
    return s_mesh;
}

// ── MeshCore components ─────────────────────────────────────────────
static TDeckBoard*      s_meshBoard  = nullptr;
static TDeckClock*       s_clock     = nullptr;
static SimpleMeshTables*  s_tables   = nullptr;
static StaticPoolPacketManager* s_pktMgr = nullptr;
static OpenMeshChat*      s_chat     = nullptr;
static CustomSX1262Wrapper* s_radio  = nullptr;
static ArduinoMillis*     s_millis   = nullptr;
static RadioNoiseListener* s_rng    = nullptr;

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

// ── Chat message callback bridge ────────────────────────────────────
// OpenMeshChat calls this when a message arrives. We bridge it into
// the existing MessageBus so UI screens don't need to change.
static void onChatMessage(const oms::ChatMessage& msg)
{
    InboxMessage inbox = {};
    inbox.kind = msg.isDirect ? MsgKind::DirectMessage : MsgKind::GroupChannel;
    inbox.channel_id = 0;  // public channel
    strncpy(inbox.sender, msg.sender, sizeof(inbox.sender) - 1);
    inbox.sender[sizeof(inbox.sender) - 1] = '\0';
    strncpy(inbox.text, msg.text, sizeof(inbox.text) - 1);
    inbox.text[sizeof(inbox.text) - 1] = '\0';
    inbox.timestamp = msg.timestamp;
    inbox.rssi = 0;  // filled by UI from MeshService::rssi()
    MessageBus::inbox().push(inbox);

    // Notification: beep and wake screen
    Notification::instance().playTone(NotifyTone::MessageIn);
    Notification::instance().wakeScreen();
}

// ── init ───────────────────────────────────────────────────────────
void MeshService::init() {
    OMS_LOG("Mesh", "Initialising MeshCore chat stack");

    // ── Board and clock ───────────────────────────────────────────
    // IBoard is created by BoardFactory::create() via theBoard(),
    // which selects the correct implementation based on build flags.
    // MeshCore's MainBoard is a separate interface (TDeckBoard) that
    // provides battery/temp/reboot to the mesh protocol stack.
    IBoard* hwBoard = theBoard();

    _meshBoard = new TDeckBoard();
    _clock = new TDeckClock();

    OMS_LOG("Mesh", "Board: %s", _meshBoard->getManufacturerName());
    OMS_LOG("Mesh", "ADC multiplier: %.2f", _meshBoard->getAdcMultiplier());

    // ── SPI and Radio ────────────────────────────────────────────
    // Get LoRa pin configuration from the board abstraction.
    // This replaces the old hardcoded #define SX1262_CS etc.
    const LoRaConfig lora = hwBoard->loraConfig();

    OMS_LOG("Mesh", "LoRa pins: CS=%d DIO1=%d RST=%d BUSY=%d SCK=%d MISO=%d MOSI=%d",
            lora.csPin, lora.dio1Pin, lora.rstPin, lora.busyPin,
            lora.sckPin, lora.misoPin, lora.mosiPin);

    // Use HSPI for LoRa (VSPI is used by TFT)
    s_loraSpi = new SPIClass(HSPI);
    s_loraSpi->begin(lora.sckPin, lora.misoPin, lora.mosiPin, lora.csPin);

    // Create CustomSX1262 with board-provided pin config
    auto* mod = new Module(lora.csPin, lora.dio1Pin, lora.rstPin, lora.busyPin, *s_loraSpi);
    s_sx1262 = new CustomSX1262(mod);

    // Get region config (from user settings, with board defaults as fallback)
    const RadioRegion* region = findRegion(config::get().radioRegion);

    // Determine radio parameters: region overrides board defaults
    float freq = region->freqMHz;
    float bw = region->bwMHz;
    uint8_t sf = region->sf;
    uint8_t cr = region->cr;
    int8_t txPower = (int8_t)config::get().txPower;  // from config, clamped 5-22

    OMS_LOG("Mesh", "Region: %s, freq: %.1f MHz, TX: %d dBm", region->name, freq, txPower);

    // Set DIO2 as RF switch (required for most SX1262 boards)
    s_sx1262->setDio2AsRfSwitch(true);

    // Init radio via std_init (uses LORA_* macros as compile-time defaults,
    // then we override with runtime region config)
    bool radioOk = s_sx1262->std_init(s_loraSpi);

    if (!radioOk) {
        OMS_LOG("Mesh", "ERROR: SX1262 std_init failed");
        _initialized = false;
        return;
    }

    // Apply runtime region config on top of std_init defaults.
    // std_init sets up the radio with LORA_* macro values, but the actual
    // region may differ (e.g. US915 instead of EU868).
    if (freq != LORA_FREQ || bw != LORA_BW || sf != LORA_SF || cr != LORA_CR) {
        OMS_LOG("Mesh", "Applying region override: %.1f MHz, BW=%.1f, SF=%d, CR=%d",
                freq, bw, sf, cr);
        int status = s_sx1262->begin(freq, bw, sf, cr,
                                      RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                                      txPower, 16, 1.6f);
        if (status != RADIOLIB_ERR_NONE) {
            OMS_LOG("Mesh", "WARNING: region override failed (err=%d), using defaults", status);
        }
    }

    // Apply TX power from config (may differ from std_init default)
    if (txPower != LORA_TX_POWER) {
        s_sx1262->setOutputPower(txPower);
    }

    s_sx1262->setCRC(1);
    OMS_LOG("Mesh", "SX1262 radio initialised on %s", region->name);

    // ── CustomSX1262Wrapper (concrete RadioLibWrapper) ─────────────
    s_radio = new CustomSX1262Wrapper(*s_sx1262, *_meshBoard);

    // ── RNG (radio noise for key generation) ──────────────────────
    s_rng = new RadioNoiseListener(*s_sx1262);

    // ── MillisecondClock ─────────────────────────────────────────
    s_millis = new ArduinoMillis();

    // ── Packet manager & dedup tables ─────────────────────────────
    s_pktMgr = new StaticPoolPacketManager(64);   // 64-packet pool
    s_tables  = new SimpleMeshTables();

    // ── Create OpenMeshChat (BaseChatMesh subclass) ────────────────
    // OpenMeshChat provides higher-level features than raw OpenMesh:
    //   - Automatic contact discovery and persistence
    //   - Group channel support with PSK encryption
    //   - Direct message ACKs and timeout handling
    // Identity loading/generation and contact persistence are handled
    // internally by OpenMeshChat::begin().
    s_chat = new OpenMeshChat(
        *s_radio,
        *s_millis,
        *s_rng,
        *_clock,
        *s_pktMgr,
        *s_tables
    );

    // Register message callback before begin so we catch early messages
    s_chat->onMessageReceived(onChatMessage);

    // begin() loads identity from SPIFFS, loads contacts, sets up public channel
    s_chat->begin(SPIFFS);

    _initialized = true;
    OMS_LOG("Mesh", "OpenMeshChat ready (region: %s, id: %s)", region->name, config::get().callsign);
}

// ── tick ───────────────────────────────────────────────────────────
void MeshService::tick() {
    if (!_initialized) return;

    _clock->tick();
    s_chat->loop();

    // Periodic advert: let other nodes discover us.
    // Base interval is 5 minutes. On each cycle we pick a random jitter
    // (+/- 60s) so multiple nodes booting together don't synchronise
    // their adverts into a storm.
    uint32_t now = millis();
    if (now - _lastAdvertMs >= _advertDeadline) {
        if (s_chat->sendAdvert()) {
            _lastAdvertMs = now;
            // Next deadline = base interval + random jitter in [-60s, +60s]
            uint32_t jitter = (esp_random() % (2 * ADVERT_JITTER_MS));
            _advertDeadline = ADVERT_INTERVAL_MS + jitter - ADVERT_JITTER_MS;
        }
    }
}

// ── Message API ────────────────────────────────────────────────────
bool MeshService::sendChannel(const char* channel, const char* text) {
    if (!_initialized || !s_chat) return false;

    // OpenMeshChat handles channel messaging with PSK encryption
    // via the public group channel. The channel parameter is currently
    // ignored (all messages go to the Public channel), but the API is
    // preserved for future multi-channel support.
    (void)channel;  // will be used when we add multiple channels
    return s_chat->sendChannelMessage(text);
}

bool MeshService::sendDirect(const uint8_t* pubkey, const char* text) {
    if (!_initialized || !pubkey || !s_chat) return false;

    // Look up the contact by public key prefix. OpenMeshChat maintains
    // a contact table from received adverts. We search by key prefix.
    // For now, we search all contacts for a matching key prefix.
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "%02X%02X%02X%02X",
             pubkey[0], pubkey[1], pubkey[2], pubkey[3]);
    ContactInfo* contact = s_chat->findContact(prefix);
    if (!contact) {
        OMS_LOG("Mesh", "DM send failed: contact not found for prefix %s", prefix);
        return false;
    }

    int result = s_chat->sendDirectMessage(*contact, text);
    return result != MSG_SEND_FAILED;
}

uint16_t MeshService::hopCount() const {
    // Hop count = number of known mesh nodes (from adverts)
    // This represents the reachability of the mesh from our perspective
    return static_cast<uint16_t>(NodeTracker::instance().count());
}

uint16_t MeshService::nodeCount() const {
    return static_cast<uint16_t>(NodeTracker::instance().count());
}

int MeshService::rssi() const {
    if (s_radio) {
        return (int)s_radio->getLastRSSI();
    }
    return 0;
}

}  // namespace oms