// OpenMeshOS — Config: persistent settings on SPIFFS
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Uses a lightweight JSON parser with escape handling for flat config.
// No ArduinoJson dependency needed (saves ~16KB flash on ESP32-S3).
// No Arduino String usage, all parsing uses fixed-size char buffers
// and strstr/memchr to avoid DRAM fragmentation.
// Config is encrypted with AES-128-CTR (key derived from device MAC
// via SHA-256) and stored on SPIFFS at /oms.cfg.

#include "Config.h"
#include "Log.h"
#include <SPIFFS.h>
#include "../hardware/PlatformCompat.h"
#include <cstring>
#include <cstdlib>

// Crypto library includes MUST be at file scope (outside namespace oms).
// The rweather/Crypto library defines classes (AES128, SHA256, AESCommon) in
// the global namespace and uses #define macros (AES128 -> AES128_ESP on ESP32)
// to rename classes. If included inside a namespace, the class definitions
// end up in that namespace, causing linker errors (undefined vtable, etc).
#include <Crypto.h>
#include <AES.h>
#include <SHA256.h>

namespace oms {

static const char* CONFIG_PATH     = "/oms.cfg";
static const char* CONFIG_RAW_PATH = "/oms.cfg.raw";  // plaintext backup during migration

// ── Config encryption (AES-128-CTR) ─────────────────────────────────
// Encrypts the config file with AES-128-CTR using a device-unique key
// derived from the MAC address via SHA-256. This replaces the old XOR
// obfuscation with cryptographic-grade encryption.
//
// Key derivation: SHA-256(mac || "oms-cfg-key") -> first 16 bytes = AES-128 key
// IV: random per write, stored alongside ciphertext
//
// File format: [4-byte magic "OMS2"] [16-byte IV] [AES-CTR ciphertext]
//
// Migration: old XOR-obfuscated and plaintext configs are auto-detected
// and re-saved in the new AES format on first load.

static constexpr size_t AES_KEY_LEN = 16;   // AES-128
static constexpr size_t AES_IV_LEN  = 16;   // CTR nonce/counter
static constexpr size_t MAGIC_LEN   = 4;    // "OMS2" header
static const uint8_t CONFIG_MAGIC[MAGIC_LEN] = {'O', 'M', 'S', '2'};

static uint8_t s_aesKey[AES_KEY_LEN];
static bool s_aesKeyReady = false;

/// Derive AES-128 key from device MAC address via SHA-256.
static void deriveAesKey()
{
    uint8_t mac[6];
    platform::readMacAddress(mac);

    ::SHA256 sha256;
    sha256.reset();
    sha256.update(mac, 6);
    sha256.update((const uint8_t*)"oms-cfg-key", 11);
    uint8_t hash[32];
    sha256.finalize(hash, 32);

    // Use first 16 bytes of SHA-256 hash as AES-128 key
    memcpy(s_aesKey, hash, AES_KEY_LEN);
    s_aesKeyReady = true;
}

/// Generate a random IV using the platform RNG.
static void generateIv(uint8_t iv[AES_IV_LEN])
{
    platform::fillRandom(iv, AES_IV_LEN);
}

/// Encrypt a plaintext buffer with AES-128-CTR.
/// ciphertext must be at least plaintextLen bytes.
/// iv is the 16-byte initialisation vector (random per write).
static void aesCtrCrypt(const uint8_t* input, size_t len,
                         const uint8_t* key, const uint8_t* iv,
                         uint8_t* output)
{
    ::AES128 aes;
    aes.setKey(key, AES_KEY_LEN);

    // CTR mode: encrypt IV counter blocks, XOR with input
    uint8_t counter[AES_IV_LEN];
    uint8_t keystream[AES_IV_LEN];
    memcpy(counter, iv, AES_IV_LEN);

    size_t offset = 0;
    while (offset < len)
    {
        aes.encryptBlock(keystream, counter);
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

/// Check if a buffer starts with the AES config magic header.
static bool isAesEncrypted(const uint8_t* buf, size_t len)
{
    if (len < MAGIC_LEN + AES_IV_LEN) return false;
    return memcmp(buf, CONFIG_MAGIC, MAGIC_LEN) == 0;
}

/// Check if a buffer looks like valid JSON (starts with '{')
static bool looksLikeJson(const uint8_t* buf, size_t len)
{
    // Skip leading whitespace
    for (size_t i = 0; i < len && i < 8; i++)
    {
        if (buf[i] == '{') return true;
        if (buf[i] > ' ') return false;  // non-whitespace, non-JSON
    }
    return false;
}

// ── Legacy XOR obfuscation (for migration only) ──────────────────────
static constexpr size_t OBFUSCATION_KEY_LEN = 16;

static void legacyXorBuffer(uint8_t* buf, size_t len)
{
    uint8_t mac[6];
    platform::readMacAddress(mac);
    uint8_t key[OBFUSCATION_KEY_LEN];
    for (int i = 0; i < 16; i++)
    {
        key[i] = mac[i % 6] ^ (mac[(i + 3) % 6] + i) ^ 0xA5;
    }
    for (size_t i = 0; i < len; i++)
    {
        buf[i] ^= key[i % OBFUSCATION_KEY_LEN];
    }
}

// ── Defaults ────────────────────────────────────────────────────────
static Config s_cfg;
static bool s_dirty = false;     // true if config changed but not yet persisted
static uint32_t s_dirtyTime = 0; // millis() when dirty flag was set
static constexpr uint32_t DEFERRED_SAVE_MS = 5000; // 5s debounce before writing to SPIFFS

// Initialise defaults
static void initDefaults(Config& c)
{
    strncpy(c.radioRegion, "EU868", sizeof(c.radioRegion));
    strncpy(c.callsign, "OMS-0001", sizeof(c.callsign));
    c.channel          = 0;
    c.brightness       = 200;
    c.screenTimeoutSec = 30;
    c.notifySound      = true;
    strncpy(c.mapTileDir, "/map", sizeof(c.mapTileDir));
    c.theme            = 0;
    c.txPower          = 17;   // default 17 dBm (~50mW, legal in most regions)
}

static struct ConfigInit {
    ConfigInit() { initDefaults(s_cfg); }
} s_cfgInit;

const Config& config::get() { return s_cfg; }

// ── Fixed-size JSON string finder ───────────────────────────────────
// Finds "key":"value" in a flat JSON string, extracts value with
// escape handling. Returns length of extracted value, 0 if not found.
// dest is always null-terminated on return.
static size_t findJsonString(const char* json, size_t jsonLen,
                              const char* key, char* dest, size_t maxLen)
{
    // Build search pattern: "key":"
    char pattern[48];
    int plen = snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    if (plen <= 0 || (size_t)plen >= sizeof(pattern)) return 0;

    const char* pos = std::strstr(json, pattern);
    if (!pos) return 0;
    pos += plen;  // skip past opening quote

    // Extract value with escape handling
    size_t outLen = 0;
    for (const char* p = pos; *p && outLen < maxLen - 1; p++)
    {
        if (*p == '\\')
        {
            // Escape sequence
            p++;
            if (!*p) break;
            switch (*p)
            {
                case '"':  dest[outLen++] = '"';  break;
                case '\\': dest[outLen++] = '\\'; break;
                case 'n':  dest[outLen++] = '\n'; break;
                case 'r':  dest[outLen++] = '\r'; break;
                case 't':  dest[outLen++] = '\t'; break;
                default:   dest[outLen++] = *p;   break;
            }
        }
        else if (*p == '"')
        {
            // End of string value
            break;
        }
        else
        {
            dest[outLen++] = *p;
        }
    }
    dest[outLen] = '\0';
    return outLen;
}

// ── Fixed-size JSON integer finder ──────────────────────────────────
// Finds "key":<integer> in a flat JSON string. Returns defaultValue if not found.
static int findJsonInt(const char* json, size_t jsonLen,
                       const char* key, int defaultVal)
{
    char pattern[48];
    int plen = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if (plen <= 0 || (size_t)plen >= sizeof(pattern)) return defaultVal;

    const char* pos = std::strstr(json, pattern);
    if (!pos) return defaultVal;
    pos += plen;

    // Skip whitespace
    while (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t') pos++;

    // Parse integer (allow negative)
    bool negative = false;
    if (*pos == '-') { negative = true; pos++; }
    int val = 0;
    while (*pos >= '0' && *pos <= '9')
    {
        val = val * 10 + (*pos - '0');
        pos++;
    }
    return negative ? -val : val;
}

// ── Fixed-size JSON boolean finder ──────────────────────────────────
static bool findJsonBool(const char* json, size_t jsonLen,
                          const char* key, bool defaultVal)
{
    char pattern[48];
    int plen = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if (plen <= 0 || (size_t)plen >= sizeof(pattern)) return defaultVal;

    const char* pos = std::strstr(json, pattern);
    if (!pos) return defaultVal;
    pos += plen;

    // Skip whitespace
    while (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t') pos++;

    if (std::strncmp(pos, "true", 4) == 0) return true;
    if (std::strncmp(pos, "false", 5) == 0) return false;
    return defaultVal;
}

void config::init()
{
    OMS_LOG("Config", "Loading config from %s", CONFIG_PATH);

    if (!s_aesKeyReady) deriveAesKey();

    if (!SPIFFS.exists(CONFIG_PATH))
    {
        OMS_LOG("Config", "No config file, using defaults");
        save();
        return;
    }

    File f = SPIFFS.open(CONFIG_PATH, "r");
    if (!f)
    {
        OMS_LOG("Config", "Failed to open config, using defaults");
        return;
    }

    // Read config file into fixed-size buffer, no Arduino String allocation
    // Max file size: MAGIC(4) + IV(16) + JSON(512) = 532 bytes
    uint8_t buf[536];
    size_t len = f.readBytes((char*)buf, sizeof(buf) - 1);
    buf[len] = '\0';
    f.close();

    char json[512];
    bool migrated = false;

    if (isAesEncrypted(buf, len))
    {
        // ── AES-encrypted format (current) ──────────────────────
        const uint8_t* iv = buf + MAGIC_LEN;
        const uint8_t* ct = buf + MAGIC_LEN + AES_IV_LEN;
        size_t ctLen = len - MAGIC_LEN - AES_IV_LEN;

        if (ctLen >= sizeof(json)) ctLen = sizeof(json) - 1;

        aesCtrCrypt(ct, ctLen, s_aesKey, iv, (uint8_t*)json);
        json[ctLen] = '\0';

        if (!looksLikeJson((const uint8_t*)json, ctLen))
        {
            OMS_LOG("Config", "AES decryption failed (invalid JSON), using defaults");
            return;
        }
        OMS_LOG("Config", "Decrypted AES config (%u bytes)", (unsigned)ctLen);
    }
    else if (len > 0 && !looksLikeJson(buf, len))
    {
        // ── Legacy XOR-obfuscated format (migration) ────────────
        legacyXorBuffer(buf, len);
        memcpy(json, buf, len + 1);
        json[len] = '\0';

        if (looksLikeJson((const uint8_t*)json, len))
        {
            OMS_LOG("Config", "Migrating XOR-obfuscated config to AES");
            migrated = true;
        }
        else
        {
            OMS_LOG("Config", "Legacy decode failed, trying plaintext");
            memcpy(json, buf, len + 1);
            json[len] = '\0';
        }
    }
    else
    {
        // ── Plaintext JSON (very old firmware) ──────────────────
        memcpy(json, buf, len + 1);
        json[len] = '\0';
        migrated = true;
        OMS_LOG("Config", "Migrating plaintext config to AES");
    }

    // Parse config values using fixed-size string operations
    size_t jlen = strlen(json);
    findJsonString(json, jlen, "radioRegion", s_cfg.radioRegion, sizeof(s_cfg.radioRegion));
    findJsonString(json, jlen, "callsign", s_cfg.callsign, sizeof(s_cfg.callsign));
    findJsonString(json, jlen, "mapTileDir", s_cfg.mapTileDir, sizeof(s_cfg.mapTileDir));
    s_cfg.channel          = findJsonInt(json, jlen, "channel", 0);
    s_cfg.brightness       = findJsonInt(json, jlen, "brightness", 200);
    s_cfg.screenTimeoutSec = findJsonInt(json, jlen, "screenTimeoutSec", 30);
    s_cfg.theme            = findJsonInt(json, jlen, "theme", 0);
    s_cfg.notifySound      = findJsonBool(json, jlen, "notifySound", true);
    s_cfg.txPower          = findJsonInt(json, jlen, "txPower", 17);

    OMS_LOG("Config", "Loaded: callsign=%s region=%s encrypted=%s",
            s_cfg.callsign, s_cfg.radioRegion, migrated ? "migrated" : "yes");

    // If we migrated from an old format, immediately re-save as AES-encrypted
    if (migrated)
    {
        save();
    }
}

/// Write a JSON-safe escaped string to file.
/// Escapes ", \, and control characters to prevent JSON injection.
static void writeJsonString(File& f, const char* str)
{
    f.print('"');
    while (*str)
    {
        char c = *str++;
        if (c == '"')       { f.print("\\\""); }
        else if (c == '\\') { f.print("\\\\"); }
        else if (c == '\n') { f.print("\\n"); }
        else if (c == '\r') { f.print("\\r"); }
        else if (c == '\t') { f.print("\\t"); }
        else if ((uint8_t)c < 0x20)
        {
            // Skip other control characters
        }
        else { f.print(c); }
    }
    f.print('"');
}

void config::save()
{
    if (!s_aesKeyReady) deriveAesKey();

    // Build JSON plaintext into a buffer first, then encrypt and write.
    char json[512];
    int pos = 0;

    // Format config as JSON into local buffer
    pos += snprintf(json + pos, sizeof(json) - pos, "{\n");
    pos += snprintf(json + pos, sizeof(json) - pos, "  \"radioRegion\": ");
    // Inline writeJsonString for char buffer
    {
        json[pos++] = '"';
        const char* s = s_cfg.radioRegion;
        while (*s && pos < (int)sizeof(json) - 4)
        {
            char c = *s++;
            if (c == '"')       { json[pos++] = '\\'; json[pos++] = '"'; }
            else if (c == '\\') { json[pos++] = '\\'; json[pos++] = '\\'; }
            else if (c == '\n')  { json[pos++] = '\\'; json[pos++] = 'n'; }
            else if ((uint8_t)c >= 0x20) { json[pos++] = c; }
        }
        json[pos++] = '"';
    }
    pos += snprintf(json + pos, sizeof(json) - pos, ",\n  \"callsign\": ");
    {
        json[pos++] = '"';
        const char* s = s_cfg.callsign;
        while (*s && pos < (int)sizeof(json) - 4)
        {
            char c = *s++;
            if (c == '"')       { json[pos++] = '\\'; json[pos++] = '"'; }
            else if (c == '\\') { json[pos++] = '\\'; json[pos++] = '\\'; }
            else if (c == '\n')  { json[pos++] = '\\'; json[pos++] = 'n'; }
            else if ((uint8_t)c >= 0x20) { json[pos++] = c; }
        }
        json[pos++] = '"';
    }
    pos += snprintf(json + pos, sizeof(json) - pos,
        ",\n  \"channel\": %d,\n"
        "  \"brightness\": %d,\n"
        "  \"screenTimeoutSec\": %d,\n"
        "  \"notifySound\": %s,\n",
        s_cfg.channel,
        s_cfg.brightness,
        s_cfg.screenTimeoutSec,
        s_cfg.notifySound ? "true" : "false");
    pos += snprintf(json + pos, sizeof(json) - pos, "  \"mapTileDir\": ");
    {
        json[pos++] = '"';
        const char* s = s_cfg.mapTileDir;
        while (*s && pos < (int)sizeof(json) - 4)
        {
            char c = *s++;
            if (c == '"')       { json[pos++] = '\\'; json[pos++] = '"'; }
            else if (c == '\\') { json[pos++] = '\\'; json[pos++] = '\\'; }
            else if (c == '\n')  { json[pos++] = '\\'; json[pos++] = 'n'; }
            else if ((uint8_t)c >= 0x20) { json[pos++] = c; }
        }
        json[pos++] = '"';
    }
    pos += snprintf(json + pos, sizeof(json) - pos,
        ",\n  \"theme\": %d,\n"
        "  \"txPower\": %d\n"
        "}\n",
        s_cfg.theme,
        s_cfg.txPower
    );

    // Generate a fresh random IV for each save
    uint8_t iv[AES_IV_LEN];
    generateIv(iv);

    // Encrypt the JSON plaintext with AES-128-CTR
    size_t dataLen = (size_t)pos;
    uint8_t ciphertext[512];
    if (dataLen > sizeof(ciphertext)) dataLen = sizeof(ciphertext);
    aesCtrCrypt((const uint8_t*)json, dataLen, s_aesKey, iv, ciphertext);

    // Write file: [magic] [IV] [ciphertext]
    File f = SPIFFS.open(CONFIG_PATH, "w");
    if (!f)
    {
        OMS_LOG("Config", "Failed to write config!");
        return;
    }

    size_t written = 0;
    written += f.write(CONFIG_MAGIC, MAGIC_LEN);
    written += f.write(iv, AES_IV_LEN);
    written += f.write(ciphertext, dataLen);
    f.close();

    if (written != MAGIC_LEN + AES_IV_LEN + dataLen)
    {
        OMS_LOG("Config", "WARNING: partial write %u/%u bytes",
                (unsigned)written, (unsigned)(MAGIC_LEN + AES_IV_LEN + dataLen));
    }
    OMS_LOG("Config", "Config saved (AES-encrypted, %u bytes)", (unsigned)written);
}

void config::setCallsign(const char* cs)
{
    // Validate: allow alphanumeric, dash, underscore only
    // Prevents JSON injection and BLE config abuse
    char safe[16];
    size_t j = 0;
    for (size_t i = 0; cs[i] && j < sizeof(safe) - 1; i++)
    {
        char c = cs[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '_')
        {
            safe[j++] = c;
        }
    }
    safe[j] = '\0';
    if (j == 0)
    {
        strncpy(safe, "OMS-0001", sizeof(safe) - 1);
        safe[sizeof(safe) - 1] = '\0';
    }
    strncpy(s_cfg.callsign, safe, sizeof(s_cfg.callsign) - 1);
    s_cfg.callsign[sizeof(s_cfg.callsign) - 1] = '\0';
    markDirty();
}

void config::setRegion(const char* reg)
{
    // Validate: only known region strings accepted
    static const char* validRegions[] = {
        "EU868", "US915", "AU915", "AS923", "KR920", "IN865"
    };
    bool valid = false;
    for (auto& r : validRegions)
    {
        if (strncmp(reg, r, sizeof(s_cfg.radioRegion)) == 0)
        {
            valid = true;
            break;
        }
    }
    if (!valid)
    {
        OMS_LOG("Config", "Invalid region '%s', keeping current", reg);
        return;
    }
    strncpy(s_cfg.radioRegion, reg, sizeof(s_cfg.radioRegion) - 1);
    s_cfg.radioRegion[sizeof(s_cfg.radioRegion) - 1] = '\0';
    markDirty();
}

void config::setTxPower(int dBm)
{
    if (dBm < 5) dBm = 5;
    if (dBm > 22) dBm = 22;
    s_cfg.txPower = dBm;
    markDirty();
}

void config::setTheme(int t)
{
    s_cfg.theme = t;
    markDirty();
}

void config::markDirty()
{
    s_dirty = true;
    s_dirtyTime = millis();
    OMS_LOG("Config", "Config marked dirty (deferred save)");
}

void config::tick()
{
    if (!s_dirty) return;
    if (millis() - s_dirtyTime >= DEFERRED_SAVE_MS)
    {
        save();
        s_dirty = false;
    }
}

void config::saveNow()
{
    save();
    s_dirty = false;
}

}  // namespace oms