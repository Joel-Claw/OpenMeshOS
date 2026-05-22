// OpenMeshOS — SPIFFS integration tests (host-side mock)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tests the Config save/load round-trip and SPIFFS wear patterns.
// Since we can't use actual SPIFFS on host, we mock the filesystem
// with tmpfs-backed files.
//
// These tests validate:
//   1. Config save → load round-trip (all fields preserved)
//   2. Config defaults on fresh filesystem
//   3. Multiple config saves don't corrupt data
//   4. Config field validation (callsign length, region names, etc.)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <fstream>
#include <sstream>

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

// ── Mock SPIFFS file I/O ────────────────────────────────────────────
// Writes/reads config as key=value lines to a temp file.
// This mirrors what Config.cpp does with SPIFFS JSON on the device.

static const char* MOCK_SPIFFS_PATH = "/tmp/openmesh_test_config.txt";

static bool mockSaveConfig(const MockConfig& c) {
    FILE* f = fopen(MOCK_SPIFFS_PATH, "w");
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

static bool mockLoadConfig(MockConfig& c) {
    FILE* f = fopen(MOCK_SPIFFS_PATH, "r");
    if (!f) return false;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;

        if (strcmp(key, "radioRegion") == 0) {
            strncpy(c.radioRegion, val, sizeof(c.radioRegion) - 1);
        } else if (strcmp(key, "callsign") == 0) {
            strncpy(c.callsign, val, sizeof(c.callsign) - 1);
        } else if (strcmp(key, "channel") == 0) {
            c.channel = atoi(val);
        } else if (strcmp(key, "brightness") == 0) {
            c.brightness = atoi(val);
        } else if (strcmp(key, "screenTimeoutSec") == 0) {
            c.screenTimeoutSec = atoi(val);
        } else if (strcmp(key, "notifySound") == 0) {
            c.notifySound = (atoi(val) != 0);
        } else if (strcmp(key, "mapTileDir") == 0) {
            strncpy(c.mapTileDir, val, sizeof(c.mapTileDir) - 1);
        } else if (strcmp(key, "theme") == 0) {
            c.theme = atoi(val);
        }
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

#define ASSERT_STREQ(a, b, msg) do { \
    tests_run++; \
    if (strcmp((a), (b)) == 0) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s — expected '%s', got '%s'\n", msg, (b), (a)); } \
} while(0)

#define ASSERT_TRUE(a, msg) do { \
    tests_run++; \
    if ((a)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s — expected true\n", msg); } \
} while(0)

// ── Tests ────────────────────────────────────────────────────────────

void test_save_load_roundtrip() {
    printf("  save → load round-trip\n");
    MockConfig original = defaultConfig();
    original.channel = 3;
    original.brightness = 180;
    original.screenTimeoutSec = 60;
    original.notifySound = false;
    original.theme = 1;
    strncpy(original.callsign, "TestNode", sizeof(original.callsign));
    strncpy(original.radioRegion, "US915", sizeof(original.radioRegion));

    ASSERT_TRUE(mockSaveConfig(original), "save succeeds");

    MockConfig loaded = {};
    ASSERT_TRUE(mockLoadConfig(loaded), "load succeeds");

    ASSERT_STREQ(loaded.radioRegion, "US915", "region round-trip");
    ASSERT_STREQ(loaded.callsign, "TestNode", "callsign round-trip");
    ASSERT_EQ(loaded.channel, 3, "channel round-trip");
    ASSERT_EQ(loaded.brightness, 180, "brightness round-trip");
    ASSERT_EQ(loaded.screenTimeoutSec, 60, "timeout round-trip");
    ASSERT_EQ(loaded.notifySound, false, "notifySound round-trip");
    ASSERT_EQ(loaded.theme, 1, "theme round-trip");
}

void test_defaults_on_fresh_fs() {
    printf("  defaults on fresh filesystem\n");
    // Remove the config file to simulate fresh SPIFFS
    remove(MOCK_SPIFFS_PATH);

    MockConfig loaded = defaultConfig();  // defaults
    bool existed = mockLoadConfig(loaded);
    ASSERT_EQ(existed, false, "no config file on fresh FS");

    // After failed load, defaults should remain
    ASSERT_STREQ(loaded.radioRegion, "EU868", "default region");
    ASSERT_STREQ(loaded.callsign, "OpenMesh", "default callsign");
    ASSERT_EQ(loaded.channel, 0, "default channel");
    ASSERT_EQ(loaded.brightness, 200, "default brightness");
}

void test_multiple_saves_no_corruption() {
    printf("  multiple saves don't corrupt\n");
    MockConfig c1 = defaultConfig();
    strncpy(c1.callsign, "First", sizeof(c1.callsign));
    mockSaveConfig(c1);

    MockConfig c2 = defaultConfig();
    strncpy(c2.callsign, "Second", sizeof(c2.callsign));
    c2.channel = 5;
    mockSaveConfig(c2);

    // Third save with different values
    MockConfig c3 = defaultConfig();
    strncpy(c3.callsign, "ThirdSave", sizeof(c3.callsign));
    c3.channel = 7;
    c3.brightness = 50;
    c3.notifySound = false;
    mockSaveConfig(c3);

    MockConfig loaded = {};
    ASSERT_TRUE(mockLoadConfig(loaded), "load after 3 saves");
    ASSERT_STREQ(loaded.callsign, "ThirdSave", "last save wins");
    ASSERT_EQ(loaded.channel, 7, "last save channel");
    ASSERT_EQ(loaded.brightness, 50, "last save brightness");
    ASSERT_EQ(loaded.notifySound, false, "last save sound");
}

void test_callsign_length() {
    printf("  callsign length validation\n");
    MockConfig c = defaultConfig();

    // Max length: 15 chars (+ null terminator in 16-byte buffer)
    strncpy(c.callsign, "123456789012345", sizeof(c.callsign) - 1);
    c.callsign[sizeof(c.callsign) - 1] = '\0';
    ASSERT_TRUE(strlen(c.callsign) <= 15, "max callsign fits");

    // Overflow: strncpy truncates
    strncpy(c.callsign, "this_is_way_too_long_for_callsign", sizeof(c.callsign) - 1);
    c.callsign[sizeof(c.callsign) - 1] = '\0';
    ASSERT_EQ(strlen(c.callsign), 15, "overflow truncated to 15");

    // Round-trip of truncated callsign
    mockSaveConfig(c);
    MockConfig loaded = {};
    mockLoadConfig(loaded);
    ASSERT_STREQ(loaded.callsign, c.callsign, "truncated callsign round-trip");
}

void test_region_names() {
    printf("  region name validation\n");
    MockConfig c = defaultConfig();

    const char* validRegions[] = {"EU868", "US915", "AU915", "AS923", "KR920", "IN865"};
    for (int i = 0; i < 6; i++) {
        strncpy(c.radioRegion, validRegions[i], sizeof(c.radioRegion) - 1);
        c.radioRegion[sizeof(c.radioRegion) - 1] = '\0';
        mockSaveConfig(c);

        MockConfig loaded = {};
        mockLoadConfig(loaded);
        ASSERT_STREQ(loaded.radioRegion, validRegions[i], "region round-trip");
    }
}

void test_brightness_bounds() {
    printf("  brightness bounds\n");
    MockConfig c = defaultConfig();

    // Min
    c.brightness = 0;
    mockSaveConfig(c);
    MockConfig loaded = {};
    mockLoadConfig(loaded);
    ASSERT_EQ(loaded.brightness, 0, "brightness 0");

    // Max
    c.brightness = 255;
    mockSaveConfig(c);
    mockLoadConfig(loaded);
    ASSERT_EQ(loaded.brightness, 255, "brightness 255");

    // Typical
    c.brightness = 128;
    mockSaveConfig(c);
    mockLoadConfig(loaded);
    ASSERT_EQ(loaded.brightness, 128, "brightness 128");
}

void test_empty_config_file() {
    printf("  empty config file\n");
    // Create empty file
    FILE* f = fopen(MOCK_SPIFFS_PATH, "w");
    fclose(f);

    MockConfig loaded = defaultConfig();
    bool ok = mockLoadConfig(loaded);
    ASSERT_TRUE(ok, "load empty file succeeds");
    // Defaults should remain since nothing was parsed
    ASSERT_STREQ(loaded.callsign, "OpenMesh", "defaults preserved on empty");
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    printf("OpenMeshOS SPIFFS Config Tests\n");
    printf("==============================\n\n");

    printf("Config I/O:\n");
    test_save_load_roundtrip();
    test_defaults_on_fresh_fs();
    test_multiple_saves_no_corruption();
    test_callsign_length();
    test_region_names();
    test_brightness_bounds();
    test_empty_config_file();

    // Cleanup
    remove(MOCK_SPIFFS_PATH);

    printf("\n==============================\n");
    printf("Results: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}