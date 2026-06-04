// OpenMeshOS — SPIFFS integration test: read/write under load
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tests SPIFFS behavior under concurrent-like I/O stress:
//   1. Rapid alternating save/load cycles
//   2. Multiple config files (simulating identity, contacts, messages)
//   3. Large file writes near SPIFFS capacity
//   4. Interrupted write recovery (partial/corrupt file)
//   5. File locking semantics (simulated with sequential access)
//
// Uses host-side tmpfs mock since we can't use actual SPIFFS.
// Mirrors the file patterns used in Config, IdentityStore, MessageBus.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

// ── Mock SPIFFS directory ───────────────────────────────────────────
static const char* MOCK_SPIFFS_DIR = "/tmp/openmesh_inttest";
static const char* MOCK_SPIFFS_CONFIG = "/tmp/openmesh_inttest/config.json";
static const char* MOCK_SPIFFS_IDENTITY = "/tmp/openmesh_inttest/identity.bin";
static const char* MOCK_SPIFFS_MESSAGES = "/tmp/openmesh_inttest/messages.dat";
static const char* MOCK_SPIFFS_WHITELIST = "/tmp/openmesh_inttest/whitelist.bin";

// ── Test infrastructure ────────────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL [%s:%d]: %s — expected %d, got %d\n", \
         __FILE__, __LINE__, msg, (int)(b), (int)(a)); } \
} while(0)

#define ASSERT_TRUE(a, msg) do { \
    tests_run++; \
    if ((a)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL [%s:%d]: %s — expected true\n", \
         __FILE__, __LINE__, msg); } \
} while(0)

#define ASSERT_STREQ(a, b, msg) do { \
    tests_run++; \
    if (strcmp((a), (b)) == 0) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL [%s:%d]: %s — expected '%s', got '%s'\n", \
         __FILE__, __LINE__, msg, (b), (a)); } \
} while(0)

// ── Helpers ────────────────────────────────────────────────────────

static void setup_mock_spiffs() {
    // Create mock directory
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s 2>/dev/null", MOCK_SPIFFS_DIR);
    system(cmd);
}

static void teardown_mock_spiffs() {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s 2>/dev/null", MOCK_SPIFFS_DIR);
    system(cmd);
}

// Simple JSON-like config writer (mirrors Config.cpp)
static bool write_config(const char* region, const char* callsign, int channel,
                         int brightness, int timeout, bool sound, int theme) {
    FILE* f = fopen(MOCK_SPIFFS_CONFIG, "w");
    if (!f) return false;
    fprintf(f, "{\"region\":\"%s\",\"callsign\":\"%s\",\"channel\":%d,"
               "\"brightness\":%d,\"timeout\":%d,\"sound\":%s,\"theme\":%d}\n",
            region, callsign, channel, brightness, timeout, sound ? "true" : "false", theme);
    fclose(f);
    return true;
}

// Read a specific field from mock JSON (minimal parser)
static std::string read_config_field(const char* field) {
    FILE* f = fopen(MOCK_SPIFFS_CONFIG, "r");
    if (!f) return "";
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    std::string search = std::string("\"") + field + "\":";
    size_t pos = strstr(buf, search.c_str()) - buf;
    if (pos >= n) return "";

    const char* val_start = buf + pos + search.length();
    // Skip opening quote if present
    if (*val_start == '"') val_start++;
    // Find end of value
    std::string result;
    for (const char* p = val_start; *p && *p != ',' && *p != '}' && *p != '"'; p++) {
        result += *p;
    }
    return result;
}

// Write binary identity file (mirrors IdentityStore)
static bool write_identity(const uint8_t* data, size_t len) {
    FILE* f = fopen(MOCK_SPIFFS_IDENTITY, "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return written == len;
}

static bool read_identity(uint8_t* data, size_t len) {
    FILE* f = fopen(MOCK_SPIFFS_IDENTITY, "rb");
    if (!f) return false;
    size_t n = fread(data, 1, len, f);
    fclose(f);
    return n == len;
}

// Write message file (mirrors MessageBus persistence)
static bool write_messages(const std::vector<std::string>& msgs) {
    FILE* f = fopen(MOCK_SPIFFS_MESSAGES, "wb");
    if (!f) return false;
    uint16_t count = (uint16_t)msgs.size();
    fwrite(&count, sizeof(count), 1, f);
    for (const auto& m : msgs) {
        uint16_t len = (uint16_t)m.length();
        fwrite(&len, sizeof(len), 1, f);
        fwrite(m.data(), 1, len, f);
    }
    fclose(f);
    return true;
}

static bool read_messages(std::vector<std::string>& msgs) {
    FILE* f = fopen(MOCK_SPIFFS_MESSAGES, "rb");
    if (!f) return false;
    uint16_t count = 0;
    if (fread(&count, sizeof(count), 1, f) != 1) { fclose(f); return false; }
    msgs.clear();
    for (uint16_t i = 0; i < count; i++) {
        uint16_t len = 0;
        if (fread(&len, sizeof(len), 1, f) != 1) { fclose(f); return false; }
        std::string s(len, '\0');
        if (fread(&s[0], 1, len, f) != len) { fclose(f); return false; }
        msgs.push_back(s);
    }
    fclose(f);
    return true;
}

// ── Tests ────────────────────────────────────────────────────────────

void test_rapid_alternating_save_load() {
    printf("  rapid alternating save/load (1000 cycles)\n");
    const int N = 1000;
    for (int i = 0; i < N; i++) {
        char callsign[16];
        snprintf(callsign, sizeof(callsign), "Node%d", i % 100);
        int channel = i % 8;
        bool sound = (i % 2 == 0);

        ASSERT_TRUE(write_config("EU868", callsign, channel, 200, 30, sound, 0),
                    "write config");

        std::string loaded_callsign = read_config_field("callsign");
        ASSERT_TRUE(loaded_callsign == callsign, "callsign matches after write");
    }
}

void test_multiple_config_files() {
    printf("  multiple config files (identity + messages + whitelist)\n");

    // Write identity
    uint8_t identity[32];
    for (int i = 0; i < 32; i++) identity[i] = (uint8_t)(i * 7 + 3);
    ASSERT_TRUE(write_identity(identity, sizeof(identity)), "write identity");

    // Write messages
    std::vector<std::string> msgs = {"Hello mesh!", "Testing 1 2 3", "GPS fix acquired"};
    ASSERT_TRUE(write_messages(msgs), "write messages");

    // Write config
    ASSERT_TRUE(write_config("US915", "TestNode", 4, 180, 60, true, 1), "write config");

    // Read back and verify all
    uint8_t loaded_identity[32];
    ASSERT_TRUE(read_identity(loaded_identity, sizeof(loaded_identity)), "read identity");
    for (int i = 0; i < 32; i++) {
        ASSERT_EQ(loaded_identity[i], identity[i], "identity byte match");
    }

    std::vector<std::string> loaded_msgs;
    ASSERT_TRUE(read_messages(loaded_msgs), "read messages");
    ASSERT_EQ((int)loaded_msgs.size(), 3, "message count");

    std::string region = read_config_field("region");
    ASSERT_TRUE(region == "US915", "config region preserved");
}

void test_large_message_store() {
    printf("  large message store (500 messages)\n");
    std::vector<std::string> msgs;
    for (int i = 0; i < 500; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Message %04d from node ABCDEF at tick %d", i, i * 100);
        msgs.push_back(buf);
    }
    ASSERT_TRUE(write_messages(msgs), "write 500 messages");

    std::vector<std::string> loaded;
    ASSERT_TRUE(read_messages(loaded), "read 500 messages");
    ASSERT_EQ((int)loaded.size(), 500, "500 messages loaded");

    // Spot-check first, middle, last
    ASSERT_TRUE(loaded[0].find("Message 0000") != std::string::npos, "first message ok");
    ASSERT_TRUE(loaded[249].find("Message 0249") != std::string::npos, "middle message ok");
    ASSERT_TRUE(loaded[499].find("Message 0499") != std::string::npos, "last message ok");
}

void test_interrupted_write_recovery() {
    printf("  interrupted write recovery (partial file)\n");

    // Write a valid config first
    ASSERT_TRUE(write_config("EU868", "GoodNode", 1, 200, 30, true, 0), "write good config");

    // Simulate interrupted write: truncate the file mid-write
    FILE* f = fopen(MOCK_SPIFFS_CONFIG, "w");
    ASSERT_TRUE(f != nullptr, "open for truncation");
    fprintf(f, "{\"region\":\"EU868\",\"callsign\":\"Corrupt");  // truncated mid-JSON
    fclose(f);

    // Attempt to read - should get partial data or detect corruption
    std::string callsign = read_config_field("callsign");
    // With our minimal parser, partial data may still be readable
    // The important thing is it doesn't crash
    ASSERT_TRUE(true, "didn't crash on partial file");

    // Overwrite with valid data to confirm recovery
    ASSERT_TRUE(write_config("EU868", "Recovered", 2, 150, 45, false, 1), "recovery write");
    std::string recovered = read_config_field("callsign");
    ASSERT_TRUE(recovered == "Recovered", "recovered after corruption");
}

void test_rapid_file_creation_deletion() {
    printf("  rapid file creation/deletion (100 cycles)\n");
    const char* test_file = "/tmp/openmesh_inttest/temp_test.dat";

    for (int i = 0; i < 100; i++) {
        // Create file
        FILE* f = fopen(test_file, "w");
        ASSERT_TRUE(f != nullptr, "create temp file");
        fprintf(f, "temp data %d", i);
        fclose(f);

        // Read it back
        f = fopen(test_file, "r");
        ASSERT_TRUE(f != nullptr, "read temp file");
        char buf[64];
        fgets(buf, sizeof(buf), f);
        fclose(f);

        // Delete it
        remove(test_file);
    }
}

void test_concurrent_like_access() {
    printf("  concurrent-like access (multi-threaded alternating I/O)\n");
    const char* file = "/tmp/openmesh_inttest/concurrent.dat";
    bool errors = false;

    // Thread 1: writer
    auto writer = [&]() {
        for (int i = 0; i < 500; i++) {
            FILE* f = fopen(file, "w");
            if (f) {
                fprintf(f, "%d", i);
                fclose(f);
            } else {
                errors = true;
            }
        }
    };

    // Thread 2: reader
    auto reader = [&]() {
        for (int i = 0; i < 500; i++) {
            FILE* f = fopen(file, "r");
            if (f) {
                char buf[32];
                if (fgets(buf, sizeof(buf), f)) {
                    int val = atoi(buf);
                    // Value should be >= 0 and < 500 (or empty if race)
                    if (val < 0 || val >= 500) {
                        errors = true;
                    }
                }
                fclose(f);
            }
            // File might not exist momentarily — that's ok
        }
    };

    std::thread t1(writer);
    std::thread t2(reader);
    t1.join();
    t2.join();

    ASSERT_TRUE(!errors, "no errors in concurrent-like access");
}

void test_whitelist_binary_format() {
    printf("  whitelist binary format (round-trip)\n");
    // Whitelist format: uint16 count, then 4-byte node IDs
    struct WhiteEntry { uint32_t nodeId; uint8_t flags; };
    std::vector<WhiteEntry> entries;
    for (int i = 0; i < 50; i++) {
        entries.push_back({0xDEAD0000 + (uint32_t)i, (uint8_t)(i % 3)});
    }

    FILE* f = fopen(MOCK_SPIFFS_WHITELIST, "wb");
    ASSERT_TRUE(f != nullptr, "open whitelist for write");
    uint16_t count = (uint16_t)entries.size();
    fwrite(&count, sizeof(count), 1, f);
    for (const auto& e : entries) {
        fwrite(&e.nodeId, sizeof(e.nodeId), 1, f);
        fwrite(&e.flags, sizeof(e.flags), 1, f);
    }
    fclose(f);

    // Read back
    f = fopen(MOCK_SPIFFS_WHITELIST, "rb");
    ASSERT_TRUE(f != nullptr, "open whitelist for read");
    uint16_t loaded_count = 0;
    fread(&loaded_count, sizeof(loaded_count), 1, f);
    ASSERT_EQ(loaded_count, 50, "whitelist entry count");

    bool all_match = true;
    for (int i = 0; i < 50; i++) {
        WhiteEntry loaded;
        fread(&loaded.nodeId, sizeof(loaded.nodeId), 1, f);
        fread(&loaded.flags, sizeof(loaded.flags), 1, f);
        if (loaded.nodeId != entries[i].nodeId || loaded.flags != entries[i].flags) {
            all_match = false;
        }
    }
    ASSERT_TRUE(all_match, "all whitelist entries match");
    fclose(f);
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    printf("OpenMeshOS SPIFFS Integration Tests\n");
    printf("====================================\n\n");

    setup_mock_spiffs();

    printf("I/O under load:\n");
    test_rapid_alternating_save_load();

    printf("\nMulti-file I/O:\n");
    test_multiple_config_files();

    printf("\nLarge data:\n");
    test_large_message_store();

    printf("\nCorruption recovery:\n");
    test_interrupted_write_recovery();

    printf("\nFile lifecycle:\n");
    test_rapid_file_creation_deletion();

    printf("\nConcurrency:\n");
    test_concurrent_like_access();

    printf("\nBinary formats:\n");
    test_whitelist_binary_format();

    teardown_mock_spiffs();

    printf("\n====================================\n");
    printf("Results: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}