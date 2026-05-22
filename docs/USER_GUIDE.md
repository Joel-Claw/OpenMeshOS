# OpenMeshOS User Guide

## Flashing

### Prerequisites
- ESP32-S3 device (LilyGo T-Deck or T-Deck Plus)
- USB-C cable
- [PlatformIO](https://platformio.org/) installed

### Build & Flash
```bash
# Clone
git clone https://github.com/Joel-Claw/OpenMeshOS.git
cd OpenMeshOS
git submodule update --init --recursive

# Build for T-Deck
pio run -e t-deck

# Build for T-Deck Plus (with built-in GPS)
pio run -e t-deck-plus

# Flash
pio run -e t-deck -t upload

# Serial monitor
pio device monitor
```

### Web Flasher
See the GitHub Releases page for web flasher instructions and pre-built firmware binaries with SHA-256 checksums.

## Navigation

The T-Deck has three input methods:
- **Keyboard**: Type text, Enter to send/submit, Esc to go back
- **Trackball**: Scroll through lists, pan the map
- **Trackball press**: Select items, toggle zoom on map

### Screen Layout
1. **Home** — Chat messages, channel tabs, send input
2. **Map** — Offline tile map with node positions
3. **Settings** — Device config, mesh params, OTA updates
4. **Terminal** — Command-line interface
5. **Lock** — Shows time/battery, press any key to unlock

## Settings Reference

### Device Info
- Firmware version, chip ID, flash/PSRAM size, heap memory
- GPS fix status (T-Deck Plus only)

### Mesh Config
| Setting | Values | Description |
|---------|--------|-------------|
| Callsign | Up to 15 chars | Your identity on the mesh |
| Region | EU868, US915, AU915, etc. | LoRa frequency (restart to apply) |
| Channel | 0-7 | Channel index (0 = public) |
| TX Power | 5-22 dBm | Transmit power (default 17) |

### Display
| Setting | Values | Description |
|---------|--------|-------------|
| Brightness | 0-255 | Screen backlight level |
| Screen timeout | 10/15/30/60/120s / Never | Time before screen sleeps |
| Sound on message | On/Off | Buzzer notification |
| Light theme | On/Off | Switch between dark and light mode |
| BLE companion | On/Off | Enable BLE phone connection |

### OTA Update
- Place `/oms/firmware.bin` on SD card
- Select "Update from SD" to flash
- **Do not power off during update!**
- BLE OTA is planned but not yet supported

### Export / Import
- Export config + identity to SD card (MeshCore-compatible format)
- Import from SD card

## Terminal Commands

Type commands in the Terminal screen or via serial:

| Command | Description |
|---------|-------------|
| `help` | Show all commands |
| `version` | Firmware version string |
| `info` | Device info, heap memory |
| `reboot` | Restart the device |
| `mesh` | Mesh status (region, channel, TX) |
| `mesh send <msg>` | Send message to public channel |
| `config` | Show current configuration |
| `clear` | Clear terminal output |
| `free` | Heap/PSRAM memory info |
| `gps` | GPS fix, coordinates, satellites |
| `ble` | BLE companion status |
| `battery` | Battery voltage |

Use **Up/Down arrows** to browse command history.

## Map

- **Pan**: Trackball left/right/up/down
- **Zoom**: Trackball center press to cycle zoom levels
- **Node markers**: Blue circles on map for mesh nodes
- **Node info**: Tap a node marker to see details (name, RSSI, hops, coordinates)
- Map tiles stored on SD card in `/map/` directory
- Use `scripts/download_tiles.py` to download tiles for your area

## Map Tiles

```bash
# Download tiles for a bounding box
python3 scripts/download_tiles.py \
  --lat 49.6 --lng 6.1 \
  --zoom 12 --radius 3 \
  --output /sd/map/
```

Tiles are standard OSM-style 256x256 PNG files organized as:
```
/map/<zoom>/<x>/<y>.png
```

## BLE Companion

Connect from a BLE app to:
- Read/write device config
- Send/receive messages
- Encrypted pairing (see MeshCore companion protocol)

## Configuration File

Config is stored in SPIFFS as `/oms.cfg` (JSON):
```json
{
  "radioRegion": "EU868",
  "callsign": "OMS-0001",
  "channel": 0,
  "brightness": 200,
  "screenTimeoutSec": 30,
  "notifySound": true,
  "mapTileDir": "/map",
  "theme": 0,
  "txPower": 17
}
```

Changes are debounced: config is written to SPIFFS 5 seconds after the last change, minimizing flash wear.

## Hardware Notes

- **Watchdog**: 30-second hardware watchdog auto-reboots if loop() hangs
- **Crash log**: If the device panics, crash info is saved to SPIFFS and shown on next boot
- **PSRAM**: 8MB PSRAM used for tile cache (9 tiles, ~1.1MB) and message buffer

## Supported Devices

| Device | PlatformIO Env | GPS | Notes |
|--------|---------------|-----|-------|
| LilyGo T-Deck | `t-deck` | External only | Standard model |
| LilyGo T-Deck Plus | `t-deck-plus` | Built-in | GPS UART auto-configured |

## FAQ

**Q: The screen is blank after flashing**
A: Check TFT_eSPI pin configuration in `platformio.ini` build flags. Verify ST7789 driver is selected.

**Q: No mesh messages received**
A: Ensure both devices use the same region and channel. Check antenna connection.

**Q: Map shows "no tiles"**
A: Download map tiles to SD card using the download script. Verify SD card is inserted.

**Q: Battery not showing**
A: Battery voltage ADC requires GPIO1 (T-Deck). Check hardware revision.

**Q: How do I reset all settings?**
A: Delete `/oms.cfg` from SPIFFS via terminal, then reboot. Defaults will be recreated.