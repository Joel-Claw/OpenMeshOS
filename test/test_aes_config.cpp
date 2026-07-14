// OpenMeshOS — AES-128-CTR config encryption unit tests (host-side)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tests the AES-128-CTR encryption used for config file protection.
// Validates key derivation, encrypt/decrypt round-trip, CTR counter
// increment logic, magic header detection, and edge cases.
//
// These tests inline the encryption logic from Config.cpp because the
// original uses Arduino/Crypto library types not available on host.
// We use a minimal AES-128 implementation to validate the CTR mode
// wrapping and key derivation logic independently.
//
// Compile: g++ -std=c++14 -Wall -Wextra -I../src -o test_aes_config test/test_aes_config.cpp -lm
// Run:     ./test_aes_config

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

// ── Minimal AES-128 implementation (host-side test only) ──────────
// We only need AES-128 encrypt block for CTR mode. This is a compact
// implementation sufficient for testing, NOT for production use.

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// GF(2^8) multiplication helper for MixColumns
static uint8_t gf_mul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

// AES-128 round key expansion
static void aes128ExpandKey(const uint8_t key[16], uint8_t roundKeys[176]) {
    memcpy(roundKeys, key, 16);
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4];
        memcpy(t, &roundKeys[i - 4], 4);
        if (i % 16 == 0) {
            // Rotate and SubBytes
            uint8_t tmp = t[0];
            t[0] = sbox[t[1]] ^ rcon[i / 16];
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[tmp];
        }
        for (int j = 0; j < 4; j++) {
            roundKeys[i + j] = roundKeys[i - 16 + j] ^ t[j];
        }
    }
}

static void aes128EncryptBlock(const uint8_t in[16], uint8_t out[16], const uint8_t roundKeys[176]) {
    uint8_t state[16];
    memcpy(state, in, 16);

    // AddRoundKey (round 0)
    for (int i = 0; i < 16; i++) state[i] ^= roundKeys[i];

    for (int round = 1; round < 10; round++) {
        // SubBytes
        for (int i = 0; i < 16; i++) state[i] = sbox[state[i]];
        // ShiftRows
        uint8_t tmp;
        tmp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
        tmp = state[2]; state[2] = state[10]; state[10] = tmp; tmp = state[6]; state[6] = state[14]; state[14] = tmp;
        tmp = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = tmp;
        // MixColumns
        for (int c = 0; c < 4; c++) {
            uint8_t a0 = state[c*4], a1 = state[c*4+1], a2 = state[c*4+2], a3 = state[c*4+3];
            uint8_t t = a0 ^ a1 ^ a2 ^ a3;
            state[c*4]   = a0 ^ t ^ gf_mul(a0 ^ a1, 2);
            state[c*4+1] = a1 ^ t ^ gf_mul(a1 ^ a2, 2);
            state[c*4+2] = a2 ^ t ^ gf_mul(a2 ^ a3, 2);
            state[c*4+3] = a3 ^ t ^ gf_mul(a3 ^ a0, 2);
        }
        // AddRoundKey
        for (int i = 0; i < 16; i++) state[i] ^= roundKeys[round * 16 + i];
    }

    // Final round (no MixColumns)
    for (int i = 0; i < 16; i++) state[i] = sbox[state[i]];
    uint8_t tmp;
    tmp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
    tmp = state[2]; state[2] = state[10]; state[10] = tmp; tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    tmp = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = tmp;
    for (int i = 0; i < 16; i++) state[i] ^= roundKeys[160 + i];

    memcpy(out, state, 16);
}

// ── CTR mode encryption (mirrors Config.cpp aesCtrCrypt) ───────────
// This is the exact same logic as Config.cpp::aesCtrCrypt but using
// our host-side AES implementation instead of the rweather/Crypto lib.

static constexpr size_t AES_KEY_LEN = 16;
static constexpr size_t AES_IV_LEN  = 16;
static constexpr size_t MAGIC_LEN   = 4;
static const uint8_t CONFIG_MAGIC[4] = {'O', 'M', 'S', '2'};

static void aesCtrCrypt(const uint8_t* input, size_t len,
                         const uint8_t* key, const uint8_t* iv,
                         uint8_t* output)
{
    uint8_t roundKeys[176];
    aes128ExpandKey(key, roundKeys);

    uint8_t counter[AES_IV_LEN];
    uint8_t keystream[AES_IV_LEN];
    memcpy(counter, iv, AES_IV_LEN);

    size_t offset = 0;
    while (offset < len)
    {
        // Encrypt counter block to get keystream
        aes128EncryptBlock(counter, keystream, roundKeys);

        size_t blockLen = (len - offset < AES_IV_LEN) ? (len - offset) : AES_IV_LEN;
        for (size_t i = 0; i < blockLen; i++)
        {
            output[offset + i] = input[offset + i] ^ keystream[i];
        }
        offset += blockLen;

        // Increment counter (big-endian, increment last byte with carry)
        for (int i = AES_IV_LEN - 1; i >= 0; i--)
        {
            if (++counter[i] != 0) break;
        }
    }
}

// ── Minimal SHA-256 for key derivation testing ─────────────────────
// We test that the key derivation concept works: SHA-256(mac || salt)
// produces a deterministic 32-byte hash, first 16 bytes used as key.

static void sha256Simple(const uint8_t* data, size_t len, uint8_t out[32]) {
    // Use a simple approach: we won't implement full SHA-256 here.
    // Instead, we test key derivation determinism by using a fixed
    // pseudo-key derived from MAC bytes. The real Config.cpp uses
    // the rweather/Crypto SHA256 class. We verify the concept holds
    // by testing that different MACs produce different keys.
    memset(out, 0, 32);
    for (size_t i = 0; i < len; i++) {
        out[i % 32] ^= data[i];
        out[(i + 7) % 32] = (out[(i + 7) % 32] + data[i] * 31) & 0xFF;
    }
    // Mix with salt
    const char* salt = "oms-cfg-key";
    for (size_t i = 0; i < 11; i++) {
        out[i % 32] ^= salt[i];
    }
}

// ── Test framework ─────────────────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name()
#define ASSERT_TRUE(expr) do { \
    tests_run++; \
    if (expr) { tests_passed++; } \
    else { tests_failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { tests_failed++; printf("FAIL %s:%d: %s != %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); } \
} while(0)

#define ASSERT_MEM_EQ(a, b, len) do { \
    tests_run++; \
    if (memcmp((a), (b), (len)) == 0) { tests_passed++; } \
    else { tests_failed++; printf("FAIL %s:%d: memcmp(%s, %s, %d)\n", __FILE__, __LINE__, #a, #b, (int)(len)); } \
} while(0)

// ── Tests ──────────────────────────────────────────────────────────

// Test 1: AES-128-CTR encrypt/decrypt round-trip (basic)
TEST(test_ctr_roundtrip_basic) {
    printf("\n=== CTR Round-trip (basic) ===\n");

    uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                       0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    uint8_t iv[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                      0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

    const char* plaintext = "Hello, OpenMeshOS!";
    size_t ptLen = strlen(plaintext);

    uint8_t ciphertext[256];
    uint8_t decrypted[256];

    aesCtrCrypt((const uint8_t*)plaintext, ptLen, key, iv, ciphertext);
    aesCtrCrypt(ciphertext, ptLen, key, iv, decrypted);

    // CTR mode is symmetric: encrypting ciphertext with same key/IV gives plaintext
    ASSERT_MEM_EQ(plaintext, decrypted, ptLen);
    printf("  Plaintext: \"%s\"\n", plaintext);
    printf("  Ciphertext differs: %s\n", memcmp(plaintext, ciphertext, ptLen) != 0 ? "yes" : "NO!");
}

// Test 2: CTR round-trip with empty input
TEST(test_ctr_roundtrip_empty) {
    printf("\n=== CTR Round-trip (empty) ===\n");
    uint8_t key[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t out[16] = {0};

    aesCtrCrypt(nullptr, 0, key, iv, out);  // should not crash
    ASSERT_TRUE(true);  // if we get here, no crash
    printf("  Empty input handled OK\n");
}

// Test 3: CTR round-trip with exactly one block (16 bytes)
TEST(test_ctr_roundtrip_one_block) {
    printf("\n=== CTR Round-trip (one block) ===\n");
    uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    uint8_t iv[16] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
                      0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00};

    uint8_t pt[16] = "OpenMeshOS-test";
    uint8_t ct[16], dec[16];

    aesCtrCrypt(pt, 16, key, iv, ct);
    aesCtrCrypt(ct, 16, key, iv, dec);

    ASSERT_MEM_EQ(pt, dec, 16);
    printf("  16-byte block round-trip OK\n");
}

// Test 4: CTR round-trip with non-block-aligned size (23 bytes)
TEST(test_ctr_roundtrip_partial_block) {
    printf("\n=== CTR Round-trip (partial block) ===\n");
    uint8_t key[16] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
                       0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
    uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    const char* pt = "OpenMeshOS config test!";  // 23 bytes
    size_t ptLen = strlen(pt);
    uint8_t ct[64], dec[64];

    aesCtrCrypt((const uint8_t*)pt, ptLen, key, iv, ct);
    aesCtrCrypt(ct, ptLen, key, iv, dec);

    ASSERT_MEM_EQ(pt, dec, ptLen);
    printf("  23-byte partial block round-trip OK\n");
}

// Test 5: CTR round-trip with large input (500 bytes, multiple blocks)
TEST(test_ctr_roundtrip_large) {
    printf("\n=== CTR Round-trip (large, 500 bytes) ===\n");
    uint8_t key[16] = {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
                       0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42};
    uint8_t iv[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8_t pt[500];
    for (int i = 0; i < 500; i++) pt[i] = (uint8_t)(i * 7 + 3);

    uint8_t ct[500], dec[500];
    aesCtrCrypt(pt, 500, key, iv, ct);
    aesCtrCrypt(ct, 500, key, iv, dec);

    ASSERT_MEM_EQ(pt, dec, 500);
    printf("  500-byte multi-block round-trip OK\n");
}

// Test 6: Same plaintext with different IVs produces different ciphertext
TEST(test_ctr_different_iv_different_ciphertext) {
    printf("\n=== CTR Different IV = Different Ciphertext ===\n");
    uint8_t key[16] = {0x01};
    uint8_t iv1[16] = {0};
    uint8_t iv2[16] = {0};
    iv2[15] = 1;  // differ by 1 in last byte

    const char* pt = "OpenMeshOS AES-128-CTR Test";
    size_t ptLen = strlen(pt);

    uint8_t ct1[64], ct2[64];
    aesCtrCrypt((const uint8_t*)pt, ptLen, key, iv1, ct1);
    aesCtrCrypt((const uint8_t*)pt, ptLen, key, iv2, ct2);

    // Ciphertexts should differ
    bool differ = memcmp(ct1, ct2, ptLen) != 0;
    ASSERT_TRUE(differ);
    printf("  Different IVs produce different ciphertext: %s\n", differ ? "yes" : "NO");
}

// Test 7: Same plaintext with different keys produces different ciphertext
TEST(test_ctr_different_key_different_ciphertext) {
    printf("\n=== CTR Different Key = Different Ciphertext ===\n");
    uint8_t key1[16] = {0x01};
    uint8_t key2[16] = {0x02};
    uint8_t iv[16] = {0};

    const char* pt = "OpenMeshOS AES-128-CTR Test";
    size_t ptLen = strlen(pt);

    uint8_t ct1[64], ct2[64];
    aesCtrCrypt((const uint8_t*)pt, ptLen, key1, iv, ct1);
    aesCtrCrypt((const uint8_t*)pt, ptLen, key2, iv, ct2);

    bool differ = memcmp(ct1, ct2, ptLen) != 0;
    ASSERT_TRUE(differ);
    printf("  Different keys produce different ciphertext: %s\n", differ ? "yes" : "NO");
}

// Test 8: Counter increment wraps correctly at boundary
TEST(test_ctr_counter_wrap) {
    printf("\n=== CTR Counter Wrap ===\n");
    uint8_t key[16] = {0x33};
    uint8_t iv[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    // With IV at max, counter wraps to 0 after first block.
    // Encrypt 32 bytes (2 blocks) and verify round-trip still works.
    uint8_t pt[32];
    for (int i = 0; i < 32; i++) pt[i] = (uint8_t)i;
    uint8_t ct[32], dec[32];

    aesCtrCrypt(pt, 32, key, iv, ct);
    aesCtrCrypt(ct, 32, key, iv, dec);

    ASSERT_MEM_EQ(pt, dec, 32);
    printf("  Counter wrap at 0xFF..FF -> 0x00..00 round-trip OK\n");
}

// Test 9: Config magic header detection
TEST(test_magic_header_detection) {
    printf("\n=== Config Magic Header ===\n");

    // Valid AES config: starts with "OMS2"
    uint8_t validBuf[24] = {'O', 'M', 'S', '2', 0x01, 0x02};
    bool isAes = (memcmp(validBuf, CONFIG_MAGIC, MAGIC_LEN) == 0);
    ASSERT_TRUE(isAes);

    // Invalid: starts with something else
    uint8_t invalidBuf[24] = {'O', 'M', 'S', '1', 0x01, 0x02};
    bool isNotAes = (memcmp(invalidBuf, CONFIG_MAGIC, MAGIC_LEN) == 0);
    ASSERT_TRUE(!isNotAes);

    // Plaintext JSON: starts with '{'
    uint8_t jsonBuf[24] = {'{', '"', 'r', 'a'};
    bool isNotAes2 = (memcmp(jsonBuf, CONFIG_MAGIC, MAGIC_LEN) == 0);
    ASSERT_TRUE(!isNotAes2);

    printf("  Magic header \"OMS2\" detection OK\n");
}

// Test 10: Full config file format simulation
TEST(test_config_file_format) {
    printf("\n=== Config File Format Simulation ===\n");

    // Simulate what Config.cpp::save() does:
    // 1. Build JSON plaintext
    // 2. Generate random IV
    // 3. Encrypt with AES-128-CTR
    // 4. Write: [magic][IV][ciphertext]

    const char* json = "{\"callsign\":\"OMS-0001\",\"radioRegion\":\"EU868\"}";
    size_t jsonLen = strlen(json);

    uint8_t key[16] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
                       0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t iv[16] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
                      0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00};

    // Build file buffer
    size_t fileLen = MAGIC_LEN + AES_IV_LEN + jsonLen;
    uint8_t fileBuf[256];
    memcpy(fileBuf, CONFIG_MAGIC, MAGIC_LEN);
    memcpy(fileBuf + MAGIC_LEN, iv, AES_IV_LEN);

    // Encrypt JSON into file buffer
    aesCtrCrypt((const uint8_t*)json, jsonLen, key, iv,
                fileBuf + MAGIC_LEN + AES_IV_LEN);

    // Now simulate loading (Config.cpp::init):
    // 1. Check magic
    ASSERT_TRUE(memcmp(fileBuf, CONFIG_MAGIC, MAGIC_LEN) == 0);

    // 2. Extract IV
    uint8_t loadedIv[16];
    memcpy(loadedIv, fileBuf + MAGIC_LEN, AES_IV_LEN);
    ASSERT_MEM_EQ(loadedIv, iv, 16);

    // 3. Decrypt ciphertext
    size_t ctLen = fileLen - MAGIC_LEN - AES_IV_LEN;
    char decrypted[256];
    aesCtrCrypt(fileBuf + MAGIC_LEN + AES_IV_LEN, ctLen, key, loadedIv,
                (uint8_t*)decrypted);
    decrypted[ctLen] = '\0';

    // 4. Verify decrypted JSON matches original
    ASSERT_TRUE(strcmp(decrypted, json) == 0);
    printf("  Full save/load round-trip OK: \"%s\"\n", decrypted);
}

// Test 11: Key derivation determinism (same MAC = same key)
TEST(test_key_derivation_determinism) {
    printf("\n=== Key Derivation Determinism ===\n");

    uint8_t mac1[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t mac2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t mac3[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    uint8_t key1[32], key2[32], key3[32];
    sha256Simple(mac1, 6, key1);  // not real SHA-256, but tests determinism
    sha256Simple(mac2, 6, key2);
    sha256Simple(mac3, 6, key3);

    // Same MAC should produce same key
    ASSERT_MEM_EQ(key1, key2, 32);

    // Different MAC should produce different key
    bool differ = memcmp(key1, key3, 32) != 0;
    ASSERT_TRUE(differ);

    printf("  Same MAC -> same key: yes\n");
    printf("  Different MAC -> different key: %s\n", differ ? "yes" : "NO");
}

// Test 12: CTR mode is symmetric (encrypt twice = original)
TEST(test_ctr_symmetric) {
    printf("\n=== CTR Symmetric Property ===\n");
    uint8_t key[16] = {0x55};
    uint8_t iv[16] = {0x77};

    const char* pt = "CTR mode is symmetric!";
    size_t ptLen = strlen(pt);

    uint8_t ct[64], dec[64];
    aesCtrCrypt((const uint8_t*)pt, ptLen, key, iv, ct);
    aesCtrCrypt(ct, ptLen, key, iv, dec);

    // CTR mode: E(E(P)) = P (symmetric)
    ASSERT_MEM_EQ(pt, dec, ptLen);
    printf("  E(E(P)) = P verified\n");
}

// Test 13: Ciphertext differs from plaintext (not identity)
TEST(test_ctr_not_identity) {
    printf("\n=== CTR Not Identity ===\n");
    uint8_t key[16] = {0x42};
    uint8_t iv[16] = {0x00};

    const char* pt = "OpenMeshOS Security Test 12345";
    size_t ptLen = strlen(pt);

    uint8_t ct[64];
    aesCtrCrypt((const uint8_t*)pt, ptLen, key, iv, ct);

    // Ciphertext should be different from plaintext
    bool differ = memcmp(pt, ct, ptLen) != 0;
    ASSERT_TRUE(differ);
    printf("  Ciphertext != plaintext: %s\n", differ ? "yes" : "NO");
}

// Test 14: All-zero input still gets encrypted (keystream visible)
TEST(test_ctr_zero_input_keystream) {
    printf("\n=== CTR Zero Input = Keystream ===\n");
    uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    uint8_t iv[16] = {0};
    uint8_t zeros[16] = {0};
    uint8_t ct[16];

    aesCtrCrypt(zeros, 16, key, iv, ct);

    // With all-zero input, ciphertext = keystream (should be non-zero)
    bool nonZero = false;
    for (int i = 0; i < 16; i++) {
        if (ct[i] != 0) { nonZero = true; break; }
    }
    ASSERT_TRUE(nonZero);
    printf("  Zero input produces non-zero keystream: %s\n", nonZero ? "yes" : "NO");
}

// ── Main ───────────────────────────────────────────────────────────
int main() {
    printf("=== OpenMeshOS AES-128-CTR Config Encryption Tests ===\n");

    test_ctr_roundtrip_basic();
    test_ctr_roundtrip_empty();
    test_ctr_roundtrip_one_block();
    test_ctr_roundtrip_partial_block();
    test_ctr_roundtrip_large();
    test_ctr_different_iv_different_ciphertext();
    test_ctr_different_key_different_ciphertext();
    test_ctr_counter_wrap();
    test_magic_header_detection();
    test_config_file_format();
    test_key_derivation_determinism();
    test_ctr_symmetric();
    test_ctr_not_identity();
    test_ctr_zero_input_keystream();

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_run);
    return tests_failed ? 1 : 0;
}