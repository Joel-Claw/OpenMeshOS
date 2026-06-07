// OpenMeshOS — NodeTracker host-side unit tests
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tests for the fixed-size node tracker: add, update, evict,
// find by prefix, count by type, whitelist toggle.
//
// Compile: g++ -std=c++14 -Wall -Wextra -I../src -o test_node_tracker test/test_node_tracker.cpp -lm
// Run:     ./test_node_tracker

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <algorithm>

// ── Mock Arduino types ──────────────────────────────────────────────
// NodeTracker uses millis() for timestamps. We provide a controllable
// mock so tests can advance time deterministically.

static uint32_t g_mock_millis = 0;
uint32_t millis() { return g_mock_millis; }

// Mock SPIFFS operations (whitelist persistence) — no-op on host
#define SPIFFS MockSPIFFS
struct MockSPIFFS {
    static bool exists(const char*) { return false; }
    struct File {
        operator bool() const { return false; }
        size_t read(void*, size_t) { return 0; }
        size_t write(const void*, size_t) { return 0; }
        void close() {}
        size_t size() { return 0; }
    };
    static File open(const char*, const char*) { return File(); }
};

// Mock OMS_LOG to stdout during tests
#define OMS_LOG(tag, fmt, ...) printf("  [%s] " fmt "\n", tag, ##__VA_ARGS__)

// ── Inline mock of AdvertDataHelpers ──────────────────────────────────
// NodeTracker.h includes helpers/AdvertDataHelpers.h. We provide
// minimal definitions here to avoid pulling in MeshCore on host.

constexpr uint8_t ADV_TYPE_CHAT     = 0;
constexpr uint8_t ADV_TYPE_REPEATER = 1;
constexpr uint8_t ADV_TYPE_ROOM     = 2;
constexpr uint8_t ADV_TYPE_SENSOR  = 3;

// ── Inline NodeTracker (from src/mesh/NodeTracker.h) ────────────────
// Copied and adapted for host testing (no Arduino SPIFFS).
// This MUST be kept in sync with the original.

static constexpr size_t MAX_TRACKED_NODES = 32;
static constexpr size_t MAX_WHITELIST_KEYS = 32;
static constexpr size_t MSG_MAX_LEN = 251;

struct TrackedNode {
    uint8_t  pub_key[32];
    char     name[24];
    uint8_t  type;
    int32_t  lat;
    int32_t  lon;
    int      rssi;
    uint32_t lastSeenMs;
    uint32_t firstSeenMs;
    bool     whitelisted;
};

class TestNodeTracker {
public:
    TestNodeTracker() : _count(0), _selfAdvertCount(0), _whitelistKeyCount(0) {}

    void onAdvert(const uint8_t pub_key[32],
                  uint8_t adv_type,
                  const char* name,
                  int32_t lat, int32_t lon,
                  int rssi)
    {
        // Sanitize name
        char safeName[24] = {0};
        if (name && name[0]) {
            size_t j = 0;
            for (size_t i = 0; name[i] && j < sizeof(safeName) - 1; i++) {
                if (name[i] >= 0x20 && name[i] < 0x7F) {
                    safeName[j++] = name[i];
                }
            }
            safeName[j] = '\0';
            name = safeName;
        }

        int idx = findExisting(pub_key);
        if (idx >= 0) {
            TrackedNode& node = _nodes[idx];
            if (name && name[0]) {
                strncpy(node.name, name, sizeof(node.name) - 1);
                node.name[sizeof(node.name) - 1] = '\0';
            }
            node.type = adv_type;
            if (lat != 0 || lon != 0) {
                node.lat = lat;
                node.lon = lon;
            }
            node.rssi = rssi;
            node.lastSeenMs = millis();
        } else {
            if (_count >= MAX_TRACKED_NODES) {
                // Evict oldest (by firstSeenMs)
                uint32_t oldest = _nodes[0].firstSeenMs;
                size_t oldestIdx = 0;
                for (size_t i = 1; i < _count; i++) {
                    if (_nodes[i].firstSeenMs < oldest) {
                        oldest = _nodes[i].firstSeenMs;
                        oldestIdx = i;
                    }
                }
                for (size_t i = oldestIdx; i < _count - 1; i++) {
                    _nodes[i] = _nodes[i + 1];
                }
                _count--;
            }

            TrackedNode& node = _nodes[_count];
            memset(&node, 0, sizeof(node));
            memcpy(node.pub_key, pub_key, 32);
            if (name && name[0]) {
                strncpy(node.name, name, sizeof(node.name) - 1);
                node.name[sizeof(node.name) - 1] = '\0';
            } else {
                snprintf(node.name, sizeof(node.name), "%02X%02X%02X%02X",
                         pub_key[0], pub_key[1], pub_key[2], pub_key[3]);
            }
            node.type = adv_type;
            node.lat = lat;
            node.lon = lon;
            node.rssi = rssi;
            node.lastSeenMs = millis();
            node.firstSeenMs = millis();
            node.whitelisted = isWhitelistedKey(pub_key);
            _count++;
        }
    }

    const TrackedNode* get(size_t idx) const {
        if (idx >= _count) return nullptr;
        return &_nodes[idx];
    }

    size_t count() const { return _count; }

    void toggleWhitelist(size_t idx) {
        if (idx < _count) {
            _nodes[idx].whitelisted = !_nodes[idx].whitelisted;
            // In real code, this calls saveWhitelist(). We skip SPIFFS in tests.
        }
    }

    int findByPrefix(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) const {
        for (size_t i = 0; i < _count; i++) {
            if (_nodes[i].pub_key[0] == b0 &&
                _nodes[i].pub_key[1] == b1 &&
                _nodes[i].pub_key[2] == b2 &&
                _nodes[i].pub_key[3] == b3) {
                return (int)i;
            }
        }
        return -1;
    }

    size_t countByType(uint8_t type) const {
        size_t n = 0;
        for (size_t i = 0; i < _count; i++) {
            if (_nodes[i].type == type) n++;
        }
        return n;
    }

    size_t countWhitelisted() const {
        size_t n = 0;
        for (size_t i = 0; i < _count; i++) {
            if (_nodes[i].whitelisted) n++;
        }
        return n;
    }

    void clear() { _count = 0; }

    // Whitelist key management (for test setup)
    void addWhitelistKey(const uint8_t key[32]) {
        if (_whitelistKeyCount < MAX_WHITELIST_KEYS) {
            memcpy(_whitelistKeys[_whitelistKeyCount], key, 32);
            _whitelistKeyCount++;
        }
    }

private:
    TrackedNode _nodes[MAX_TRACKED_NODES];
    size_t _count;
    uint32_t _selfAdvertCount;
    uint8_t _whitelistKeys[MAX_WHITELIST_KEYS][32];
    size_t _whitelistKeyCount;

    bool isWhitelistedKey(const uint8_t pub_key[32]) const {
        for (size_t i = 0; i < _whitelistKeyCount; i++) {
            if (memcmp(_whitelistKeys[i], pub_key, 32) == 0) return true;
        }
        return false;
    }

    int findExisting(const uint8_t pub_key[32]) const {
        for (size_t i = 0; i < _count; i++) {
            if (memcmp(_nodes[i].pub_key, pub_key, 32) == 0) return (int)i;
        }
        return -1;
    }
};

// ── Test framework ──────────────────────────────────────────────────
static size_t tests_run = 0;
static size_t tests_passed = 0;

#define ASSERT_TRUE(expr) do { \
    tests_run++; \
    if (expr) { tests_passed++; } \
    else { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { printf("FAIL %s:%d: %s != %s (got %lld vs %lld)\n", \
                   __FILE__, __LINE__, #a, #b, (long long)(a), (long long)(b)); } \
} while(0)

// ── Helper: make a test public key ──────────────────────────────────
static void makeKey(uint8_t key[32], uint8_t prefix) {
    memset(key, 0, 32);
    key[0] = prefix;
}

// ── Tests ────────────────────────────────────────────────────────────

void test_add_single_node() {
    printf("  test_add_single_node... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0xAA);
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Alice", 49611670, 6130000, -42);

    ASSERT_EQ((long long)tracker.count(), 1LL);

    const TrackedNode* node = tracker.get(0);
    ASSERT_TRUE(node != nullptr);
    ASSERT_EQ((long long)node->pub_key[0], 0xAALL);
    ASSERT_EQ(strcmp(node->name, "Alice"), 0);
    ASSERT_EQ((long long)node->type, (long long)ADV_TYPE_CHAT);
    ASSERT_EQ((long long)node->rssi, -42LL);
    ASSERT_EQ((long long)node->lat, 49611670LL);
    ASSERT_EQ((long long)node->lon, 6130000LL);

    printf("OK\n");
}

void test_update_existing_node() {
    printf("  test_update_existing_node... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0xBB);

    // First advert
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Bob", 0, 0, -60);
    ASSERT_EQ((long long)tracker.count(), 1LL);
    ASSERT_EQ((long long)tracker.get(0)->rssi, -60LL);

    // Second advert from same node — updates RSSI
    g_mock_millis = 2000;
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Bob", 50000000, 6000000, -55);
    ASSERT_EQ((long long)tracker.count(), 1LL);  // still 1 node
    ASSERT_EQ((long long)tracker.get(0)->rssi, -55LL);  // RSSI updated
    ASSERT_EQ((long long)tracker.get(0)->lat, 50000000LL);  // location updated
    ASSERT_EQ((long long)tracker.get(0)->lastSeenMs, 2000LL);  // timestamp updated
    ASSERT_EQ((long long)tracker.get(0)->firstSeenMs, 1000LL);  // first seen unchanged

    printf("OK\n");
}

void test_multiple_types() {
    printf("  test_multiple_types... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t chatKey[32], rpKey[32], sensorKey[32];
    makeKey(chatKey, 0x01);
    makeKey(rpKey, 0x02);
    makeKey(sensorKey, 0x03);

    tracker.onAdvert(chatKey, ADV_TYPE_CHAT, "Chat1", 0, 0, -40);
    tracker.onAdvert(rpKey, ADV_TYPE_REPEATER, "RP1", 0, 0, -70);
    tracker.onAdvert(sensorKey, ADV_TYPE_SENSOR, "Sensor1", 0, 0, -90);

    ASSERT_EQ((long long)tracker.count(), 3LL);
    ASSERT_EQ((long long)tracker.countByType(ADV_TYPE_CHAT), 1LL);
    ASSERT_EQ((long long)tracker.countByType(ADV_TYPE_REPEATER), 1LL);
    ASSERT_EQ((long long)tracker.countByType(ADV_TYPE_SENSOR), 1LL);
    ASSERT_EQ((long long)tracker.countByType(ADV_TYPE_ROOM), 0LL);

    printf("OK\n");
}

void test_find_by_prefix() {
    printf("  test_find_by_prefix... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0xAB);
    key[1] = 0xCD;
    key[2] = 0xEF;
    key[3] = 0x01;
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Finder", 0, 0, -50);

    int idx = tracker.findByPrefix(0xAB, 0xCD, 0xEF, 0x01);
    ASSERT_TRUE(idx >= 0);
    ASSERT_EQ(strcmp(tracker.get(idx)->name, "Finder"), 0);

    // Not found
    int missing = tracker.findByPrefix(0xFF, 0xFF, 0xFF, 0xFF);
    ASSERT_EQ((long long)missing, -1LL);

    printf("OK\n");
}

void test_eviction_when_full() {
    printf("  test_eviction_when_full... ");
    TestNodeTracker tracker;

    // Add MAX_TRACKED_NODES (32) nodes
    for (size_t i = 0; i < MAX_TRACKED_NODES; i++) {
        g_mock_millis = (uint32_t)(1000 + i * 100);
        uint8_t key[32];
        makeKey(key, (uint8_t)(i + 1));
        char name[24];
        snprintf(name, sizeof(name), "Node%zu", i);
        tracker.onAdvert(key, ADV_TYPE_CHAT, name, 0, 0, -50);
    }

    ASSERT_EQ((long long)tracker.count(), (long long)MAX_TRACKED_NODES);

    // Add one more — oldest (Node0 at firstSeenMs=1000) should be evicted
    g_mock_millis = 5000;
    uint8_t overflowKey[32];
    makeKey(overflowKey, 0xFF);
    tracker.onAdvert(overflowKey, ADV_TYPE_CHAT, "Overflow", 0, 0, -45);

    ASSERT_EQ((long long)tracker.count(), (long long)MAX_TRACKED_NODES);  // still 32

    // Node0 should no longer be findable by its key prefix
    int idx = tracker.findByPrefix(0x01, 0, 0, 0);
    ASSERT_EQ((long long)idx, -1LL);  // evicted

    // Overflow node should be findable
    idx = tracker.findByPrefix(0xFF, 0, 0, 0);
    ASSERT_TRUE(idx >= 0);
    ASSERT_EQ(strcmp(tracker.get(idx)->name, "Overflow"), 0);

    printf("OK\n");
}

void test_whitelist_toggle() {
    printf("  test_whitelist_toggle... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0x10);
    tracker.onAdvert(key, ADV_TYPE_CHAT, "WListed", 0, 0, -50);

    ASSERT_EQ((long long)tracker.countWhitelisted(), 0LL);

    // Whitelist the node
    tracker.toggleWhitelist(0);
    ASSERT_EQ((long long)tracker.countWhitelisted(), 1LL);
    ASSERT_TRUE(tracker.get(0)->whitelisted);

    // Toggle again — remove from whitelist
    tracker.toggleWhitelist(0);
    ASSERT_EQ((long long)tracker.countWhitelisted(), 0LL);
    ASSERT_TRUE(!tracker.get(0)->whitelisted);

    printf("OK\n");
}

void test_preloaded_whitelist() {
    printf("  test_preloaded_whitelist... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    // Pre-load a whitelist key
    uint8_t wlKey[32];
    makeKey(wlKey, 0x20);
    tracker.addWhitelistKey(wlKey);

    // When this node appears, it should auto-whitelist
    tracker.onAdvert(wlKey, ADV_TYPE_CHAT, "PreWhitelisted", 0, 0, -55);
    ASSERT_EQ((long long)tracker.countWhitelisted(), 1LL);
    ASSERT_TRUE(tracker.get(0)->whitelisted);

    printf("OK\n");
}

void test_name_sanitization() {
    printf("  test_name_sanitization... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0x30);

    // Name with control characters and non-ASCII
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Hel\x01lo\x7FW\xC3rld", 0, 0, -50);

    const TrackedNode* node = tracker.get(0);
    ASSERT_TRUE(node != nullptr);
    // Control chars (\x01, \x7F) and non-ASCII (\xC3) should be stripped
    ASSERT_EQ(strcmp(node->name, "HelloWrld"), 0);

    printf("OK\n");
}

void test_null_name_uses_hex_prefix() {
    printf("  test_null_name_uses_hex_prefix... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0xAB);
    key[1] = 0xCD;
    key[2] = 0xEF;
    key[3] = 0x12;

    tracker.onAdvert(key, ADV_TYPE_CHAT, nullptr, 0, 0, -50);
    ASSERT_EQ(strcmp(tracker.get(0)->name, "ABCDEF12"), 0);

    // Empty name also uses hex
    uint8_t key2[32];
    makeKey(key2, 0x11);
    tracker.onAdvert(key2, ADV_TYPE_CHAT, "", 0, 0, -50);
    ASSERT_EQ(strcmp(tracker.get(1)->name, "11000000"), 0);

    printf("OK\n");
}

void test_get_out_of_range_returns_null() {
    printf("  test_get_out_of_range_returns_null... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    ASSERT_TRUE(tracker.get(0) == nullptr);
    ASSERT_TRUE(tracker.get(100) == nullptr);

    uint8_t key[32];
    makeKey(key, 0x01);
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Solo", 0, 0, -50);

    ASSERT_TRUE(tracker.get(0) != nullptr);
    ASSERT_TRUE(tracker.get(1) == nullptr);

    printf("OK\n");
}

void test_location_update_only_when_present() {
    printf("  test_location_update_only_when_present... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0x40);

    // First advert with location
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Loc", 50000000, 6000000, -50);
    ASSERT_EQ((long long)tracker.get(0)->lat, 50000000LL);
    ASSERT_EQ((long long)tracker.get(0)->lon, 6000000LL);

    // Second advert without location (0, 0) — should NOT clear location
    g_mock_millis = 2000;
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Loc", 0, 0, -45);
    ASSERT_EQ((long long)tracker.get(0)->lat, 50000000LL);  // preserved
    ASSERT_EQ((long long)tracker.get(0)->lon, 6000000LL);  // preserved
    ASSERT_EQ((long long)tracker.get(0)->rssi, -45LL);     // updated

    printf("OK\n");
}

void test_clear() {
    printf("  test_clear... ");
    g_mock_millis = 1000;
    TestNodeTracker tracker;

    uint8_t key[32];
    makeKey(key, 0x50);
    tracker.onAdvert(key, ADV_TYPE_CHAT, "Tmp", 0, 0, -50);
    ASSERT_EQ((long long)tracker.count(), 1LL);

    tracker.clear();
    ASSERT_EQ((long long)tracker.count(), 0LL);
    ASSERT_TRUE(tracker.get(0) == nullptr);

    printf("OK\n");
}

// ── Main ─────────────────────────────────────────────────────────────
int main() {
    printf("NodeTracker unit tests\n");
    printf("========================\n\n");

    test_add_single_node();
    test_update_existing_node();
    test_multiple_types();
    test_find_by_prefix();
    test_eviction_when_full();
    test_whitelist_toggle();
    test_preloaded_whitelist();
    test_name_sanitization();
    test_null_name_uses_hex_prefix();
    test_get_out_of_range_returns_null();
    test_location_update_only_when_present();
    test_clear();

    printf("\n");
    printf("Results: %zu/%zu passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("ALL PASSED ✅\n");
        return 0;
    } else {
        printf("SOME FAILED ❌\n");
        return 1;
    }
}