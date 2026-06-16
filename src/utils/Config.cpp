// OpenMeshOS — Config: persistent settings on SPIFFS
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Uses a lightweight JSON parser with escape handling for flat config.
// No ArduinoJson dependency needed (saves ~16KB flash on ESP32-S3).
// No Arduino String usage — all parsing uses fixed-size char buffers
// and strstr/memchr to avoid DRAM fragmentation.
// All config lives in /oms.cfg on SPIFFS.

#include "Config.h"
#include "Log.h"
#include <SPIFFS.h>
#include <cstring>
#include <cstdlib>

namespace oms {

static const char* CONFIG_PATH = "/oms.cfg";

// ── Defaults ────────────────────────────────────────────────────────
static Config s_cfg;
static bool s_dirty = false;     // true if config changed but not yet persisted
static uint32_t s_dirtyTime = 0; // millis() when dirty flag was set
static constexpr uint32_t DEFERRED_SAVE_MS = 5000; // 5s debounce before writing to SPIFFS

// Initialise defaults
static void initDefaults(Config& c) {
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
                              const char* key, char* dest, size_t maxLen) {
    // Build search pattern: "key":"
    char pattern[48];
    int plen = snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    if (plen <= 0 || (size_t)plen >= sizeof(pattern)) return 0;

    const char* pos = std::strstr(json, pattern);
    if (!pos) return 0;
    pos += plen;  // skip past opening quote

    // Extract value with escape handling
    size_t outLen = 0;
    for (const char* p = pos; *p && outLen < maxLen - 1; p++) {
        if (*p == '\\') {
            // Escape sequence
            p++;
            if (!*p) break;
            switch (*p) {
                case '"':  dest[outLen++] = '"';  break;
                case '\\': dest[outLen++] = '\\'; break;
                case 'n':  dest[outLen++] = '\n'; break;
                case 'r':  dest[outLen++] = '\r'; break;
                case 't':  dest[outLen++] = '\t'; break;
                default:   dest[outLen++] = *p;   break;
            }
        } else if (*p == '"') {
            // End of string value
            break;
        } else {
            dest[outLen++] = *p;
        }
    }
    dest[outLen] = '\0';
    return outLen;
}

// ── Fixed-size JSON integer finder ──────────────────────────────────
// Finds "key":<integer> in a flat JSON string. Returns defaultValue if not found.
static int findJsonInt(const char* json, size_t jsonLen,
                       const char* key, int defaultVal) {
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
    while (*pos >= '0' && *pos <= '9') {
        val = val * 10 + (*pos - '0');
        pos++;
    }
    return negative ? -val : val;
}

// ── Fixed-size JSON boolean finder ──────────────────────────────────
static bool findJsonBool(const char* json, size_t jsonLen,
                          const char* key, bool defaultVal) {
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

void config::init() {
    OMS_LOG("Config", "Loading config from %s", CONFIG_PATH);

    if (!SPIFFS.exists(CONFIG_PATH)) {
        OMS_LOG("Config", "No config file, using defaults");
        save();
        return;
    }

    File f = SPIFFS.open(CONFIG_PATH, "r");
    if (!f) {
        OMS_LOG("Config", "Failed to open config, using defaults");
        return;
    }

    // Read config file into fixed-size buffer — no Arduino String allocation
    char json[512];
    size_t len = f.readBytes(json, sizeof(json) - 1);
    json[len] = '\0';
    f.close();

    // Parse config values using fixed-size string operations
    findJsonString(json, len, "radioRegion", s_cfg.radioRegion, sizeof(s_cfg.radioRegion));
    findJsonString(json, len, "callsign", s_cfg.callsign, sizeof(s_cfg.callsign));
    findJsonString(json, len, "mapTileDir", s_cfg.mapTileDir, sizeof(s_cfg.mapTileDir));
    s_cfg.channel          = findJsonInt(json, len, "channel", 0);
    s_cfg.brightness       = findJsonInt(json, len, "brightness", 200);
    s_cfg.screenTimeoutSec = findJsonInt(json, len, "screenTimeoutSec", 30);
    s_cfg.theme            = findJsonInt(json, len, "theme", 0);
    s_cfg.notifySound      = findJsonBool(json, len, "notifySound", true);
    s_cfg.txPower          = findJsonInt(json, len, "txPower", 17);

    OMS_LOG("Config", "Loaded: callsign=%s region=%s",
            s_cfg.callsign, s_cfg.radioRegion);
}

/// Write a JSON-safe escaped string to file.
/// Escapes ", \, and control characters to prevent JSON injection.
static void writeJsonString(File& f, const char* str) {
    f.print('"');
    while (*str) {
        char c = *str++;
        if (c == '"')       { f.print("\\\""); }
        else if (c == '\\') { f.print("\\\\"); }
        else if (c == '\n') { f.print("\\n"); }
        else if (c == '\r') { f.print("\\r"); }
        else if (c == '\t') { f.print("\\t"); }
        else if ((uint8_t)c < 0x20) {
            // Skip other control characters
        }
        else { f.print(c); }
    }
    f.print('"');
}

void config::save() {
    File f = SPIFFS.open(CONFIG_PATH, "w");
    if (!f) {
        OMS_LOG("Config", "Failed to write config!");
        return;
    }
    f.print("{\n");
    f.print("  \"radioRegion\": ");
    writeJsonString(f, s_cfg.radioRegion);
    f.print(",\n  \"callsign\": ");
    writeJsonString(f, s_cfg.callsign);
    f.printf(",\n  \"channel\": %d,\n"
        "  \"brightness\": %d,\n"
        "  \"screenTimeoutSec\": %d,\n"
        "  \"notifySound\": %s,\n",
        s_cfg.channel,
        s_cfg.brightness,
        s_cfg.screenTimeoutSec,
        s_cfg.notifySound ? "true" : "false");
    f.print("  \"mapTileDir\": ");
    writeJsonString(f, s_cfg.mapTileDir);
    f.printf(",\n  \"theme\": %d,\n"
        "  \"txPower\": %d\n"
        "}\n",
        s_cfg.theme,
        s_cfg.txPower
    );
    f.close();
    OMS_LOG("Config", "Config saved");
}

void config::setCallsign(const char* cs) {
    // Validate: allow alphanumeric, dash, underscore only
    // Prevents JSON injection and BLE config abuse
    char safe[16];
    size_t j = 0;
    for (size_t i = 0; cs[i] && j < sizeof(safe) - 1; i++) {
        char c = cs[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '_') {
            safe[j++] = c;
        }
    }
    safe[j] = '\0';
    if (j == 0) {
        strncpy(safe, "OMS-0001", sizeof(safe) - 1);
        safe[sizeof(safe) - 1] = '\0';
    }
    strncpy(s_cfg.callsign, safe, sizeof(s_cfg.callsign) - 1);
    s_cfg.callsign[sizeof(s_cfg.callsign) - 1] = '\0';
    markDirty();
}

void config::setRegion(const char* reg) {
    // Validate: only known region strings accepted
    static const char* validRegions[] = {
        "EU868", "US915", "AU915", "AS923", "KR920", "IN865"
    };
    bool valid = false;
    for (auto& r : validRegions) {
        if (strncmp(reg, r, sizeof(s_cfg.radioRegion)) == 0) {
            valid = true;
            break;
        }
    }
    if (!valid) {
        OMS_LOG("Config", "Invalid region '%s', keeping current", reg);
        return;
    }
    strncpy(s_cfg.radioRegion, reg, sizeof(s_cfg.radioRegion) - 1);
    s_cfg.radioRegion[sizeof(s_cfg.radioRegion) - 1] = '\0';
    markDirty();
}

void config::setTxPower(int dBm) {
    if (dBm < 5) dBm = 5;
    if (dBm > 22) dBm = 22;
    s_cfg.txPower = dBm;
    markDirty();
}

void config::setTheme(int t) {
    s_cfg.theme = t;
    markDirty();
}

void config::markDirty() {
    s_dirty = true;
    s_dirtyTime = millis();
    OMS_LOG("Config", "Config marked dirty (deferred save)");
}

void config::tick() {
    if (!s_dirty) return;
    if (millis() - s_dirtyTime >= DEFERRED_SAVE_MS) {
        save();
        s_dirty = false;
    }
}

void config::saveNow() {
    save();
    s_dirty = false;
}

}  // namespace oms