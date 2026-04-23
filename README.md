# OpenMeshOS

Open-source standalone firmware for LoRa mesh devices. Built on [MeshCore](https://github.com/meshcore-dev/MeshCore), designed for the LilyGo T-Deck and T-Deck Plus.

**A phone in your pocket, without the phone, and without the paywall.**

## What Is This?

OpenMeshOS is a free, open-source firmware that turns affordable LoRa devices into powerful standalone mesh communicators. It provides smartphone-grade messaging, GPS maps, encrypted comms, and more, all running directly on the device with no phone, no internet, and no license fees required.

This project was initially vibecoded by [GLM-5.1](https://github.com/THUDM/GLM-5) (a 754B parameter large language model by Zhipu AI). The foundation it produced is functional but raw. Now it is the humans' turn to make it truly usable. The AI may occasionally join in to help, but this is a community project first.

## Features (Target)

- **Chat**: Public channels, private channels, direct messages with speech-bubble UI
- **GPS Map**: Offline tile-based map from SD card, node positions, route tracing
- **Encrypted Comms**: End-to-end encryption via MeshCore protocol
- **Repeater Scanner**: Discover and manage repeaters, signal strength, noise floor
- **Notifications**: Customizable alerts with screen wake, auto-dimming, lock screen
- **Terminal Access**: Full MeshCore terminal for power users
- **BLE Companion**: Connect with MeshCore mobile apps via Bluetooth
- **Config Import/Export**: Compatible with MeshCore companion app format

## Supported Hardware

| Device | Status | Notes |
|--------|--------|-------|
| LilyGo T-Deck (ESP32-S3) | Target | 320x240 IPS, keyboard, trackball, SX1262 LoRa |
| LilyGo T-Deck Plus | Target | Same + built-in GPS, different Grove pin assignment |
| Other ESP32-S3 devices | Future | PlatformIO abstraction allows porting |

## Branches

| Branch | Purpose |
|--------|----------|
| `main` | Stable releases. Tagged with versions. Don't push directly. |
| `dev` | Active development. PRs go here. CI must pass before merge. |
| `alpha` | Created from `dev` when enough features accumulate for an alpha release. |
| `beta` | Created from `alpha` when features are hardware-tested and mostly working. |

Workflow: contribute to `dev` via PR. When enough changes accumulate, create `alpha` or `beta` branch from `dev`. When stable, merge to `main` and tag a release.

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (CLI or VS Code extension)
- USB-C cable
- A LilyGo T-Deck or T-Deck Plus

### Build

```bash
git clone --recurse-submodules https://github.com/PeterAlfonsLoch/OpenMeshOS.git
cd OpenMeshOS
pio run -e t-deck
```

### Flash

Hold the trackball center button, press the reset button on the side, then release both. The T-Deck is now in DFU mode.

**First time / recovery (merged binary):**
```bash
pio run -e t-deck -t upload
# Or manually with the merged binary:
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 openmeshos-merged.bin
```

**OTA update (app only):**
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x10000 openmeshos-0.1.0-alpha.1.bin
```

See [docs/VERSIONING.md](docs/VERSIONING.md) for firmware variant details.

### Map Tiles

Map tiles go on a FAT32-formatted SD card in this structure:

```
/map/
  /10/        ← zoom level
    /529/      ← x tile
      /340.png ← y tile (256x256 PNG)
```

You can download tiles from [OpenStreetMap](https://wiki.openstreetmap.org/wiki/Downloading_data) or use tools like [renderd/render_list](https://wiki.openstreetmap.org/wiki/Mod_tile) to pre-render them.

## Project Structure

```
OpenMeshOS/
├── src/
│   ├── main.cpp              # Entry point
│   ├── hardware/
│   │   ├── Board.h/cpp       # T-Deck hardware abstraction
│   │   └── Keyboard.h/cpp   # BBQ10KB I2C keyboard driver
│   ├── mesh/
│   │   └── MeshService.h/cpp # MeshCore bridge
│   ├── ui/
│   │   ├── UIScreen.h/cpp    # LVGL display controller
│   │   ├── ScreenHome.h/cpp  # Chat screen
│   │   ├── ScreenMap.h/cpp   # Map screen
│   │   ├── ScreenSettings.h/cpp
│   │   ├── ScreenTerminal.h/cpp
│   │   └── Theme.h/cpp       # Colour palette
│   ├── map/
│   │   └── MapEngine.h/cpp   # Tile renderer
│   └── utils/
│       ├── Config.h/cpp      # Persistent settings
│       └── ConfigExport.h/cpp # SD card import/export (MeshCore format)
│       └── Log.h              # Serial logger
├── lib/
│   └── MeshCore/             # Git submodule (mesh protocol)
├── docs/
│   ├── UI_DESIGN.md          # ASCII art UI layouts
│   ├── ARCHITECTURE.md       # System design
│   ├── MAP_SYSTEM.md         # Map tile system design
│   ├── ROADMAP.md            # Development plan
│   ├── CONTRIBUTING.md       # How to help
│   └── HARDWARE.md           # T-Deck pin reference
├── platformio.ini
├── partitions.csv
└── LICENSE                   # WTFPL v2
```

## Architecture

OpenMeshOS is layered:

```
┌─────────────────────────────────┐
│          UI (LVGL 9)            │
│  Home │ Map │ Settings │ Term   │
├─────────────────────────────────┤
│        App Logic                 │
│  Messages │ Contacts │ Config    │
├─────────────────────────────────┤
│      Hardware Abstraction        │
│  Board │ Keyboard │ GPS │ LoRa   │
├─────────────────────────────────┤
│     MeshCore (C++ library)       │
│  Routing │ Encryption │ Radio    │
├─────────────────────────────────┤
│         ESP32-S3 Hardware         │
└─────────────────────────────────┘
```

- **MeshCore** handles all mesh networking: routing, encryption, packet handling
- **Board** abstracts T-Deck peripherals: display, keyboard, trackball, GPS, LoRa radio
- **App Logic** manages messages, contacts, settings, map state
- **UI** renders everything through LVGL with a consistent dark theme

No dynamic memory allocation after setup. No heap fragmentation. This is embedded software.

## Versioning

OpenMeshOS follows [Semantic Versioning](https://semver.org/) with pre-release tags:

- **`-alpha.N`** — compiles, not tested on hardware. Anything may break.
- **`-beta.N`** — running on hardware. Core features work but bugs expected.
- **`-rc.N`** — release candidate. Final testing.
- **(none)** — stable release.

Current version: **0.1.0-alpha.1** (first compile, not flashed)

Each release includes two firmware binaries:
1. **App-only** (`openmeshos-X.Y.Z.bin`) — for OTA updates, flash at `0x10000`
2. **Merged** (`openmeshos-X.Y.Z-merged.bin`) — bootloader + partitions + app, flash at `0x0`

See [docs/VERSIONING.md](docs/VERSIONING.md) for full details.

## Vibecoding Disclosure

The initial codebase of OpenMeshOS was generated by **GLM-5.1** (Zhipu AI, 754B parameters) via prompt-driven development. This means:

- The architectural decisions and code patterns reflect AI-generated output
- Some code may contain logical gaps or untested edge cases
- The code has NOT been tested on physical hardware yet
- Human review, testing, and contribution are essential and welcome

This is not a polished product. It is a foundation. Build on it.

## License

WTFPL v2 (Do What The Fuck You Want To Public License). Do whatever you want with it. See [THIRD_PARTY_LICENSES.md](docs/THIRD_PARTY_LICENSES.md) for dependency licenses.

## Links

- [MeshCore](https://github.com/meshcore-dev/MeshCore) — the mesh networking library we depend on
- [LilyGo T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck) — hardware reference
- [LVGL](https://lvgl.io/) — UI framework
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — display driver