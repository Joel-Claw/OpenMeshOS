// OpenMeshOS -- ConfigExport.cpp
// Copyright 2026 Joel Claw & contributors -- CC0 1.0 Universal
//
// Export/import config + identity to/from SD card.
// MeshCore-compatible format on SD card:
//   /oms/config.json    -- OpenMeshOS settings (JSON, same format as SPIFFS)
//   /oms/identity.id    -- MeshCore local identity (binary, IdentityStore format)
//   /oms/regions.bin    -- MeshCore region map (binary, SimpleMeshTables format)
//   /oms/contacts/      -- Exported contacts as binary advert packets

#include "ConfigExport.h"
#include "Config.h"
#include "Log.h"
#include <SPIFFS.h>
#include <SD.h>

namespace oms {

static const char* OMS_DIR       = "/oms";
static const char* OMS_CONFIG   = "/oms/config.json";
static const char* OMS_IDENTITY = "/oms/identity.id";
static const char* OMS_REGIONS  = "/oms/regions.bin";
static const char* OMS_CONTACTS = "/oms/contacts";

// ── Helpers ─────────────────────────────────────────────────────────

static bool ensureOMSDir() {
    if (!SD.exists(OMS_DIR)) {
        if (!SD.mkdir(OMS_DIR)) {
            OMS_LOG("ConfigExport", "Failed to create %s on SD", OMS_DIR);
            return false;
        }
    }
    return true;
}

// Copy a file from src filesystem to dst filesystem
// fsSrc/fsDst are SPIFFS or SD, paths include full path
static bool copyFile(fs::FS& fsSrc, const char* srcPath,
                     fs::FS& fsDst, const char* dstPath) {
    File src = fsSrc.open(srcPath, "r");
    if (!src) return false;

    File dst = fsDst.open(dstPath, "w");
    if (!dst) { src.close(); return false; }

    uint8_t buf[512];
    while (src.available()) {
        size_t n = src.read(buf, sizeof(buf));
        if (n > 0) dst.write(buf, n);
    }
    src.close();
    dst.close();
    return true;
}

// ── SD Config Check ─────────────────────────────────────────────────

bool hasSDConfig() {
    if (!SD.exists(OMS_DIR)) return false;
    return SD.exists(OMS_CONFIG) || SD.exists(OMS_IDENTITY);
}

// ── Export ───────────────────────────────────────────────────────────

bool configExportToSD() {
    OMS_LOG("ConfigExport", "Exporting config to SD card");

    if (!ensureOMSDir()) return false;

    bool ok = true;

    // 1) Export OpenMeshOS config.json (from SPIFFS to SD)
    {
        // Re-generate JSON config to SD (same format as SPIFFS)
        File f = SD.open(OMS_CONFIG, "w");
        if (f) {
            const Config& c = config::get();
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
                c.radioRegion, c.callsign, c.channel,
                c.brightness, c.screenTimeoutSec,
                c.notifySound ? "true" : "false",
                c.mapTileDir, c.theme
            );
            f.close();
            OMS_LOG("ConfigExport", "Wrote %s", OMS_CONFIG);
        } else {
            OMS_LOG("ConfigExport", "Failed to write %s", OMS_CONFIG);
            ok = false;
        }
    }

    // 2) Export MeshCore identity (copy from SPIFFS to SD)
    {
        if (SPIFFS.exists("/identity.id")) {
            if (copyFile(SPIFFS, "/identity.id", SD, OMS_IDENTITY)) {
                OMS_LOG("ConfigExport", "Wrote %s", OMS_IDENTITY);
            } else {
                OMS_LOG("ConfigExport", "Failed to export identity");
                ok = false;
            }
        } else {
            OMS_LOG("ConfigExport", "No identity file to export");
        }
    }

    // 3) Export MeshCore region map (copy from SPIFFS to SD)
    {
        if (SPIFFS.exists("/regions.bin")) {
            if (copyFile(SPIFFS, "/regions.bin", SD, OMS_REGIONS)) {
                OMS_LOG("ConfigExport", "Wrote %s", OMS_REGIONS);
            } else {
                OMS_LOG("ConfigExport", "Failed to export regions");
                ok = false;
            }
        }
    }

    if (ok) {
        OMS_LOG("ConfigExport", "Export complete");
    }
    return ok;
}

// ── Import ───────────────────────────────────────────────────────────

bool configImportFromSD() {
    OMS_LOG("ConfigExport", "Importing config from SD card");

    if (!SD.exists(OMS_DIR)) {
        OMS_LOG("ConfigExport", "No /oms directory on SD");
        return false;
    }

    bool ok = true;

    // 1) Import OpenMeshOS config.json (from SD to SPIFFS, then reload)
    {
        if (SD.exists(OMS_CONFIG)) {
            // Read SD config and write to SPIFFS
            File src = SD.open(OMS_CONFIG, "r");
            if (src) {
                File dst = SPIFFS.open("/oms.cfg", "w");
                if (dst) {
                    uint8_t buf[512];
                    while (src.available()) {
                        size_t n = src.read(buf, sizeof(buf));
                        if (n > 0) dst.write(buf, n);
                    }
                    dst.close();
                    OMS_LOG("ConfigExport", "Imported config.json -> /oms.cfg");
                } else {
                    OMS_LOG("ConfigExport", "Failed to write /oms.cfg");
                    ok = false;
                }
                src.close();
            } else {
                OMS_LOG("ConfigExport", "Failed to read %s", OMS_CONFIG);
                ok = false;
            }
        }
    }

    // 2) Import MeshCore identity (from SD to SPIFFS)
    {
        if (SD.exists(OMS_IDENTITY)) {
            if (copyFile(SD, OMS_IDENTITY, SPIFFS, "/identity.id")) {
                OMS_LOG("ConfigExport", "Imported identity.id");
            } else {
                OMS_LOG("ConfigExport", "Failed to import identity");
                ok = false;
            }
        }
    }

    // 3) Import MeshCore region map (from SD to SPIFFS)
    {
        if (SD.exists(OMS_REGIONS)) {
            if (copyFile(SD, OMS_REGIONS, SPIFFS, "/regions.bin")) {
                OMS_LOG("ConfigExport", "Imported regions.bin");
            } else {
                OMS_LOG("ConfigExport", "Failed to import regions");
                ok = false;
            }
        }
    }

    // Reload config from the newly-imported SPIFFS file
    if (ok) {
        config::init();  // re-read /oms.cfg into memory
        OMS_LOG("ConfigExport", "Import complete, config reloaded");
    }

    return ok;
}

// ── Identity-only export/import ──────────────────────────────────────

bool exportIdentityToSD(const char* filename) {
    if (!ensureOMSDir()) return false;

    const char* destPath;
    char pathBuf[64];

    if (filename) {
        snprintf(pathBuf, sizeof(pathBuf), "%s/%s", OMS_CONTACTS, filename);
        destPath = pathBuf;
        // Create contacts dir if needed
        if (!SD.exists(OMS_CONTACTS)) {
            SD.mkdir(OMS_CONTACTS);
        }
    } else {
        destPath = OMS_IDENTITY;
    }

    if (!SPIFFS.exists("/identity.id")) {
        OMS_LOG("ConfigExport", "No identity to export");
        return false;
    }

    return copyFile(SPIFFS, "/identity.id", SD, destPath);
}

bool importIdentityFromSD(const char* filename) {
    const char* srcPath;
    char pathBuf[64];

    if (filename) {
        snprintf(pathBuf, sizeof(pathBuf), "%s/%s", OMS_CONTACTS, filename);
        srcPath = pathBuf;
    } else {
        srcPath = OMS_IDENTITY;
    }

    if (!SD.exists(srcPath)) {
        OMS_LOG("ConfigExport", "Identity file not found: %s", srcPath);
        return false;
    }

    return copyFile(SD, srcPath, SPIFFS, "/identity.id");
}

}  // namespace oms