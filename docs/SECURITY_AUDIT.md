# Security Audit — OpenMeshOS

**Date**: 2026-06-05
**Auditor**: Automated (OpenClaw GLM-5.1)
**Scope**: Source code review of all security-relevant components

## Findings and Remediations

### Critical

#### 1. SD Card OTA: No Firmware Integrity Verification
- **Status**: ✅ Fixed
- **Risk**: Corrupted or tampered firmware on SD card would be flashed without verification
- **Fix**: Added SHA-256 checksum verification before flashing. If `/oms/firmware.bin.sha256` exists alongside the firmware, the hash is verified before proceeding. Missing checksum file is logged as a warning but doesn't block the update (backward compatible).
- **Files**: `src/ui/ScreenSettings.cpp` (ota_start_sd_cb)

### High

#### 2. Config JSON Injection via Unsanitized Strings
- **Status**: ✅ Fixed
- **Risk**: Callsign or region containing `"` or `\` characters would break JSON config format, potentially corrupting the config file or allowing injection
- **Fix**: Replaced `printf`-style JSON serialization with a `writeJsonString()` helper that properly escapes `"`, `\`, `\n`, `\r`, `\t`, and strips control characters
- **Files**: `src/utils/Config.cpp`

#### 3. BLE Config Write: No Input Validation
- **Status**: ✅ Fixed
- **Risk**: BLE companion app could set arbitrary values for channel (out of range), or unknown config keys could be silently applied
- **Fix**: Added bounds checking for channel (0-7), validation of callsign (alphanumeric + dash/underscore only via `setCallsign()`), region (validated against known list via `setRegion()`), and rejection of unknown config keys
- **Files**: `src/mesh/BLECompanion.cpp`, `src/utils/Config.cpp`

### Medium

#### 4. Callsign Allows Arbitrary Characters
- **Status**: ✅ Fixed
- **Risk**: Callsign could contain special characters that break JSON, display incorrectly, or confuse other mesh nodes
- **Fix**: `config::setCallsign()` now validates: only alphanumeric, dash, and underscore allowed. Falls back to "OMS-0001" if empty after sanitization
- **Files**: `src/utils/Config.cpp`

#### 5. Region String Not Validated
- **Status**: ✅ Fixed
- **Risk**: Invalid region string could cause undefined behavior in radio initialization
- **Fix**: `config::setRegion()` now validates against the known list: EU868, US915, AU915, AS923, KR920, IN865. Unknown regions are rejected
- **Files**: `src/utils/Config.cpp`

#### 6. Mesh Advert Names Not Sanitized
- **Status**: ✅ Fixed
- **Risk**: Node names from mesh adverts could contain non-printable characters that display incorrectly or cause LVGL issues
- **Fix**: Added `sanitizeMeshName()` in NodeTracker to strip non-printable characters (keeps 0x20-0x7E)
- **Files**: `src/mesh/NodeTracker.cpp`

### Low / Informational

#### 7. BLE OTA: No Firmware Signature Verification
- **Status**: ⚠️ Acknowledged (design limitation)
- **Risk**: Compromised BLE companion app could push malicious firmware
- **Mitigation**: BLE pairing is required (encrypted link). The companion app must authenticate with a shared secret derived from the device identity key. Future improvement: add Ed25519 signature verification for firmware images
- **Recommendation**: For v1.0, consider requiring firmware to be signed with a developer key

#### 8. MeshCore Encryption Key Storage
- **Status**: ⚠️ Acknowledged (MeshCore responsibility)
- **Risk**: Identity private keys are stored in SPIFFS without hardware encryption (ESP32-S3 has secure boot and flash encryption but they're not enabled by default)
- **Mitigation**: ESP32-S3 supports flash encryption and secure boot v2. Enabling these would protect keys at rest
- **Recommendation**: Enable ESP32-S3 flash encryption and secure boot v2 before production deployment

#### 9. Config File on SPIFFS is Plaintext
- **Status**: ⚠️ Acknowledged (low risk on embedded device)
- **Risk**: Config file `/oms.cfg` is readable/writable by anyone with physical access to SPIFFS
- **Mitigation**: Physical access to the device already implies full control. Flash encryption would mitigate this
- **Recommendation**: Enable flash encryption for production builds

## Security Features Already in Place

- **BLE pairing required** for all BLE characteristic access (ESP_BLE_SEC_ENCRYPT)
- **CodeQL** automated security scanning (weekly) in CI
- **Security audit workflow** checks incoming issues and PRs for suspicious patterns
- **Watchdog timer** (30s) auto-reboots on hang
- **Crash logging** persists stack traces to SPIFFS
- **SPIFFS wear minimization** with 5s deferred save prevents flash wear attacks
- **TX power bounds** (5-22 dBm) enforced in `setTxPower()`
- **OTA size limits** (0-6.4MB) prevent absurdly large firmware writes
- **Packet forwarding** explicitly allowed (repeater mode)

## Recommendations for v1.0

1. Enable ESP32-S3 **flash encryption** and **secure boot v2**
2. Add **Ed25519 firmware signing** for both BLE and SD card OTA updates
3. Add **rate limiting** on BLE config writes (prevent rapid wear attacks)
4. Consider **encrypted config** storage (encrypt sensitive fields before writing to SPIFFS)
5. Add **BLE OTA abort timeout** — if no data for N seconds, auto-abort in-progress OTA