// OpenMeshOS — Config: persistent settings on SPIFFS
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal
//
// Uses ArduinoJson to serialise/deserialise a flat JSON config file.
// All config lives in /oms.cfg on SPIFFS.

#include "Config.h"
#include "Log.h"
#include <SPIFFS.h>

namespace oms {

static const char* CONFIG_PATH = "/oms.cfg";

// ── Defaults ────────────────────────────────────────────────────────
static Config s_cfg;

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
}

static struct ConfigInit {
    ConfigInit() { initDefaults(s_cfg); }
} s_cfgInit;

const Config& config::get() { return s_cfg; }

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

    // Minimal hand-rolled JSON parse (no ArduinoJson dependency yet)
    // TODO: replace with ArduinoJson for robustness
    String json = f.readString();
    f.close();

    // Simple key=value extraction from JSON
    // This is intentionally basic — we'll use ArduinoJson later
    auto readString = [&](const char* key, char* dest, size_t maxLen) {
        String searchKey = String("\"") + key + "\":\"";
        int start = json.indexOf(searchKey);
        if (start >= 0) {
            start += searchKey.length();
            int end = json.indexOf('"', start);
            if (end > start) {
                size_t len = end - start;
                if (len >= maxLen) len = maxLen - 1;
                memcpy(dest, json.c_str() + start, len);
                dest[len] = '\0';
            }
        }
    };

    auto readInt = [&](const char* key, int defVal) -> int {
        String searchKey = String("\"") + key + "\":";
        int start = json.indexOf(searchKey);
        if (start >= 0) {
            start += searchKey.length();
            return json.substring(start).toInt();
        }
        return defVal;
    };

    auto readBool = [&](const char* key, bool defVal) -> bool {
        String searchKey = String("\"") + key + "\":";
        int start = json.indexOf(searchKey);
        if (start >= 0) {
            start += searchKey.length();
            String val = json.substring(start);
            if (val.startsWith("true")) return true;
            if (val.startsWith("false")) return false;
        }
        return defVal;
    };

    readString("radioRegion", s_cfg.radioRegion, sizeof(s_cfg.radioRegion));
    readString("callsign", s_cfg.callsign, sizeof(s_cfg.callsign));
    readString("mapTileDir", s_cfg.mapTileDir, sizeof(s_cfg.mapTileDir));
    s_cfg.channel          = readInt("channel", 0);
    s_cfg.brightness       = readInt("brightness", 200);
    s_cfg.screenTimeoutSec = readInt("screenTimeoutSec", 30);
    s_cfg.theme            = readInt("theme", 0);
    s_cfg.notifySound      = readBool("notifySound", true);

    OMS_LOG("Config", "Loaded: callsign=%s region=%s",
            s_cfg.callsign, s_cfg.radioRegion);
}

void config::save() {
    File f = SPIFFS.open(CONFIG_PATH, "w");
    if (!f) {
        OMS_LOG("Config", "Failed to write config!");
        return;
    }
    f.printf(
        "{\n"
        "  \"radioRegion\": \"%s\",\n"
        "  \"callsign\": \"%s\",\n"
        "  \"channel\": %d,\n"
        "  \"brightness\": %d,\n"
        "  \"screenTimeoutSec\": %d,\n"
        "  \"notifySound\": %s,\n"
        "  \"mapTileDir\": \"%s\",\n"
        "  \"theme\": %d\n"
        "}\n",
        s_cfg.radioRegion,
        s_cfg.callsign,
        s_cfg.channel,
        s_cfg.brightness,
        s_cfg.screenTimeoutSec,
        s_cfg.notifySound ? "true" : "false",
        s_cfg.mapTileDir,
        s_cfg.theme
    );
    f.close();
    OMS_LOG("Config", "Config saved");
}

void config::setCallsign(const char* cs) {
    strncpy(s_cfg.callsign, cs, sizeof(s_cfg.callsign) - 1);
    s_cfg.callsign[sizeof(s_cfg.callsign) - 1] = '\0';
    save();
}

void config::setRegion(const char* reg) {
    strncpy(s_cfg.radioRegion, reg, sizeof(s_cfg.radioRegion) - 1);
    s_cfg.radioRegion[sizeof(s_cfg.radioRegion) - 1] = '\0';
    save();
}

}  // namespace oms