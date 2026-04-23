// OpenMeshOS — ConfigExport.h
// Copyright 2026 Joel Claw & contributors -- CC0 1.0 Universal
//
// Export and import config + identity to/from SD card.
// Uses MeshCore-compatible format:
//   /oms/config.json   -- OpenMeshOS settings (JSON)
//   /oms/identity.id   -- MeshCore local identity (binary)
//   /oms/regions.bin   -- MeshCore region map (binary)
//   /oms/contacts/     -- Directory of exported contacts (binary)

#pragma once

#include <Arduino.h>

namespace oms {

// Export all config to SD card. Returns true on success.
// Overwrites existing files. Creates /oms/ directory if needed.
bool configExportToSD();

// Import all config from SD card. Returns true on success.
// Does NOT overwrite files that fail to parse -- rolls back on error.
bool configImportFromSD();

// Export just the identity (for sharing with other devices).
// Writes a single .id file to SD card.
bool exportIdentityToSD(const char* filename = nullptr);

// Import an identity file from SD card.
bool importIdentityFromSD(const char* filename = nullptr);

// Check if SD card has a valid config export.
bool hasSDConfig();

}  // namespace oms