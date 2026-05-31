// OpenMeshOS — SPIFFS stress test (host-side)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Simulates rapid, repeated, and concurrent-style writes to a mock
// filesystem, validating:
//   1. Data integrity under rapid sequential writes
//   2. No corruption after many overwrite cycles
//   3. Large payload round-trip (whitelist binary data)
//   4. Interrupted write recovery (partial write -> reload)
//   5. Config + whitelist interleaved writes
//
// This cannot test actual SPIFFS wear, but validates our serialization
// logic holds up under load.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

// ── Mock Config struct (mirrors src/utils/Config.h) ──────────────
struct MockConfig {
    char radioRegion[8];
    char callsign[16];
    int  channel;
    int  brightness;
    int  screenTimeoutSec;
    bool notifySound;
    char mapTileDir[32];
    int  theme;
};

static MockConfig defaultConfig() {
    MockConfig c = {};
    strncpy(c.radioRegion, "EU868", sizeof(c.radioRegion));
    strncpy(c.callsign, "OpenMesh", sizeof(c.callsign));
    c.channel = 0;
    c.brightness = 200;
    c.screenTimeoutSec = 30;
    c.notifySound = true;
    strncpy(c.mapTileDir, "/map", sizeof(c.mapTileDir));
    c.theme = 0;
    return c;
}

// ── Mock 32-byte key (for whitelist) ──────────────────────────────
struct PubKey {
    uint8_t bytes[32];

    bool operator==(const PubKey& other) const {
        return memcmp(bytes, other.bytes, 32) == 0;
    }
    bool operator!=(const PubKey& other) const {
        return !(*this == other);
    }
    uint8_t& operator[](size_t i) { return bytes[i]; }
    const uint8_t& operator[](size_t i) const { return bytes[i]; }
};

// ── Mock SPIFFS file paths ────────────────────────────────────────
static const char* CONFIG_PATH = "/tmp/openmesh_stress_config.txt";
static const char* WHITELIST_PATH = "/tmp/openmesh_stress_whitelist.bin";

// ── Config I/O (key=value format) ─────────────────────────────────
static bool saveConfig(const MockConfig& c, const char* path = CONFIG_PATH) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "radioRegion=%s\n", c.radioRegion);
    fprintf(f, "callsign=%s\n", c.callsign);
    fprintf(f, "channel=%d\n", c.channel);
    fprintf(f, "brightness=%d\n", c.brightness);
    fprintf(f, "screenTimeoutSec=%d\n", c.screenTimeoutSec);
    fprintf(f, "notifySound=%d\n", c.notifySound ? 1 : 0);
    fprintf(f, "mapTileDir=%s\n", c.mapTileDir);
    fprintf(f, "theme=%d\n", c.theme);
    fclose(f);
    return true;
}

static bool loadConfig(MockConfig& c, const char* path = CONFIG_PATH) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;
        if (strcmp(key, "radioRegion") == 0) strncpy(c.radioRegion, val, sizeof(c.radioRegion) - 1);
        else if (strcmp(key, "callsign") == 0) strncpy(c.callsign, val, sizeof(c.callsign) - 1);
        else if (strcmp(key, "channel") == 0) c.channel = atoi(val);
        else if (strcmp(key, "brightness") == 0) c.brightness = atoi(val);
        else if (strcmp(key, "screenTimeoutSec") == 0) c.screenTimeoutSec = atoi(val);
        else if (strcmp(key, "notifySound") == 0) c.notifySound = (atoi(val) != 0);
        else if (strcmp(key, "mapTileDir") == 0) strncpy(c.mapTileDir, val, sizeof(c.mapTileDir) - 1);
        else if (strcmp(key, "theme") == 0) c.theme = atoi(val);
    }
    fclose(f);
    return true;
}

// ── Whitelist I/O (binary: 32 bytes per entry = raw pub_key) ──────
static bool saveWhitelist(const std::vector<PubKey>& keys, const char* path = WHITELIST_PATH) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    for (size_t i = 0; i < keys.size(); i++) {
        if (fwrite(keys[i].bytes, 1, 32, f) != 32) {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    return true;
}

static bool loadWhitelist(std::vector<PubKey>& keys, const char* path = WHITELIST_PATH) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    keys.clear();
    PubKey key;
    while (fread(key.bytes, 1, 32, f) == 32) {
        keys.push_back(key);
    }
    fclose(f);
    return true;
}

// ── Test helpers ────────────────────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s — expected %d, got %d\n", msg, (int)(b), (int)(a)); } \
} while(0)

#define ASSERT_TRUE(a, msg) do { \
    tests_run++; \
    if ((a)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

// ── Tests ────────────────────────────────────────────────────────────

void test_rapid_sequential_writes() {
    printf("  rapid sequential writes (100 cycles)\n");
    MockConfig c = defaultConfig();

    for (int i = 0; i < 100; i++) {
        snprintf(c.callsign, sizeof(c.callsign), "Node%03d", i);
        c.channel = i % 8;
        c.brightness = 50 + (i * 2) % 206;
        c.theme = i % 3;
        ASSERT_TRUE(saveConfig(c), "save succeeds");
    }

    MockConfig loaded = {};
    ASSERT_TRUE(loadConfig(loaded), "load succeeds after 100 writes");
    ASSERT_EQ(strcmp(loaded.callsign, "Node099"), 0, "callsign is Node099");
    ASSERT_EQ(loaded.channel, 99 % 8, "channel is 99%%8");
    ASSERT_EQ(loaded.brightness, 50 + (99 * 2) % 206, "brightness correct");
    ASSERT_EQ(loaded.theme, 99 % 3, "theme correct");
}

void test_whitelist_roundtrip() {
    printf("  whitelist binary round-trip\n");

    std::vector<PubKey> keys;
    for (int i = 0; i < 10; i++) {
        PubKey key;
        for (int j = 0; j < 32; j++) {
            key[j] = (uint8_t)(i * 32 + j);
        }
        keys.push_back(key);
    }

    ASSERT_TRUE(saveWhitelist(keys), "save whitelist succeeds");

    std::vector<PubKey> loaded;
    ASSERT_TRUE(loadWhitelist(loaded), "load whitelist succeeds");
    ASSERT_EQ((int)loaded.size(), 10, "10 keys loaded");

    for (int i = 0; i < 10; i++) {
        bool match = true;
        for (int j = 0; j < 32; j++) {
            if (loaded[i][j] != (uint8_t)(i * 32 + j)) {
                match = false;
                break;
            }
        }
        ASSERT_TRUE(match, "key content matches");
    }
}

void test_whitelist_many_entries() {
    printf("  whitelist with 32 entries (max capacity)\n");

    std::vector<PubKey> keys;
    for (int i = 0; i < 32; i++) {
        PubKey key;
        memset(key.bytes, 0xAA, 32);
        key[0] = (uint8_t)i;
        keys.push_back(key);
    }

    ASSERT_TRUE(saveWhitelist(keys), "save 32 keys succeeds");

    std::vector<PubKey> loaded;
    ASSERT_TRUE(loadWhitelist(loaded), "load 32 keys succeeds");
    ASSERT_EQ((int)loaded.size(), 32, "32 keys loaded");

    for (int i = 0; i < 32; i++) {
        ASSERT_EQ(loaded[i][0], (uint8_t)i, "key[0] is index");
        ASSERT_EQ(loaded[i][1], 0xAA, "key[1] is 0xAA");
    }
}

void test_config_whitelist_interleaved() {
    printf("  config + whitelist interleaved writes\n");

    MockConfig c = defaultConfig();
    strncpy(c.callsign, "Interleave", sizeof(c.callsign));
    c.channel = 5;
    ASSERT_TRUE(saveConfig(c), "config save #1");

    std::vector<PubKey> wl;
    PubKey k1;
    memset(k1.bytes, 0xBB, 32);
    k1[0] = 0x01;
    wl.push_back(k1);
    ASSERT_TRUE(saveWhitelist(wl), "whitelist save #1");

    strncpy(c.callsign, "SecondPass", sizeof(c.callsign));
    c.channel = 7;
    ASSERT_TRUE(saveConfig(c), "config save #2");

    MockConfig loaded = {};
    ASSERT_TRUE(loadConfig(loaded), "config load after interleaved");
    ASSERT_EQ(strcmp(loaded.callsign, "SecondPass"), 0, "callsign is SecondPass");
    ASSERT_EQ(loaded.channel, 7, "channel is 7");

    std::vector<PubKey> loaded_wl;
    ASSERT_TRUE(loadWhitelist(loaded_wl), "whitelist load after interleaved");
    ASSERT_EQ((int)loaded_wl.size(), 1, "1 whitelist key");
    ASSERT_EQ(loaded_wl[0][0], 0x01, "key[0] is 0x01");
    ASSERT_EQ(loaded_wl[0][1], 0xBB, "key[1] is 0xBB");
}

void test_interrupted_write_recovery() {
    printf("  interrupted write recovery (partial file)\n");

    MockConfig c = defaultConfig();
    strncpy(c.callsign, "BeforeCrash", sizeof(c.callsign));
    ASSERT_TRUE(saveConfig(c), "save before crash");

    // Simulate a crash: write partial data (truncated file)
    FILE* f = fopen(CONFIG_PATH, "w");
    ASSERT_TRUE(f != nullptr, "open for partial write");
    fprintf(f, "radioRegion=EU868\n");
    fprintf(f, "callsign=Pa");  // truncated!
    fclose(f);

    MockConfig loaded = defaultConfig();
    ASSERT_TRUE(loadConfig(loaded), "load partial config succeeds");
    ASSERT_EQ(strcmp(loaded.radioRegion, "EU868"), 0, "partial: region recovered");
    ASSERT_EQ(strcmp(loaded.callsign, "Pa"), 0, "partial: callsign truncated to Pa");

    // Overwrite with full valid config
    strncpy(c.callsign, "AfterRecovery", sizeof(c.callsign));
    c.channel = 3;
    ASSERT_TRUE(saveConfig(c), "save after recovery");
    ASSERT_TRUE(loadConfig(loaded), "load after recovery");
    ASSERT_EQ(strcmp(loaded.callsign, "AfterRecovery"), 0, "recovered callsign");
    ASSERT_EQ(loaded.channel, 3, "recovered channel");
}

void test_empty_whitelist() {
    printf("  empty whitelist round-trip\n");

    std::vector<PubKey> empty;
    ASSERT_TRUE(saveWhitelist(empty), "save empty whitelist");

    std::vector<PubKey> loaded;
    ASSERT_TRUE(loadWhitelist(loaded), "load empty whitelist");
    ASSERT_EQ((int)loaded.size(), 0, "empty whitelist has 0 entries");
}

void test_corrupt_whitelist_recovery() {
    printf("  corrupt whitelist recovery (short file)\n");

    std::vector<PubKey> keys;
    PubKey k1;
    memset(k1.bytes, 0xCC, 32);
    keys.push_back(k1);
    ASSERT_TRUE(saveWhitelist(keys), "save 1 key whitelist");

    // Corrupt: truncate the file to 20 bytes (not a full 32-byte entry)
    FILE* f = fopen(WHITELIST_PATH, "wb");
    uint8_t partial[20];
    memset(partial, 0xDD, 20);
    fwrite(partial, 1, 20, f);
    fclose(f);

    // Loading should return 0 complete entries (partial key is discarded)
    std::vector<PubKey> loaded;
    loadWhitelist(loaded);
    ASSERT_EQ((int)loaded.size(), 0, "corrupt whitelist: 0 complete entries");
}

void test_large_callsign_edge_chars() {
    printf("  large callsign with edge characters\n");

    MockConfig c = defaultConfig();
    memset(c.callsign, 'A', sizeof(c.callsign) - 1);
    c.callsign[sizeof(c.callsign) - 1] = '\0';
    ASSERT_EQ((int)strlen(c.callsign), 15, "max-length callsign");

    ASSERT_TRUE(saveConfig(c), "save max callsign");
    MockConfig loaded = {};
    ASSERT_TRUE(loadConfig(loaded), "load max callsign");
    ASSERT_EQ(strcmp(loaded.callsign, "AAAAAAAAAAAAAAA"), 0, "15 A's round-trip");
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    printf("OpenMeshOS SPIFFS Stress Tests\n");
    printf("==============================\n\n");

    printf("Config stress:\n");
    test_rapid_sequential_writes();
    test_interrupted_write_recovery();
    test_large_callsign_edge_chars();

    printf("\nWhitelist stress:\n");
    test_whitelist_roundtrip();
    test_whitelist_many_entries();
    test_empty_whitelist();
    test_corrupt_whitelist_recovery();

    printf("\nInterleaved:\n");
    test_config_whitelist_interleaved();

    // Cleanup
    remove(CONFIG_PATH);
    remove(WHITELIST_PATH);

    printf("\n==============================\n");
    printf("Results: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}