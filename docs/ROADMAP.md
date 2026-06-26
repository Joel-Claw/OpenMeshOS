# Roadmap — OpenMeshOS

Current firmware version: **0.1.0-alpha.2** (first compile, not flashed to hardware)

## Phase 0: Foundation Cleanup (v0.1.0 — Current)

- [x] Project structure and PlatformIO config
- [x] Hardware abstraction layer (Board.h/cpp)
- [x] Config system (SPIFFS JSON)
- [x] LVGL 9 + TFT_eSPI display driver stub
- [x] Theme system (dark mode colours)
- [x] Home screen layout (chat bubbles, tabs, input)
- [x] MapEngine coordinate math
- [x] MeshCore as git submodule
- [x] First successful compile without errors
- [x] CI pipeline (build, test, security audit, release workflow)
- [x] CodeQL security scanning (weekly, Copilot Autofix enabled)
- [x] MapEngine unit tests (34 tests passing)
- [x] Config export/import via SD card (MeshCore-compatible format)
- [x] Branching model (main/dev/alpha/beta)
- [x] Release v0.1.0-alpha.1 (draft firmware on GitHub)
- [x] ScreenMap: offline map with pan/zoom via trackball
- [x] ScreenSettings: device info, mesh config, export/import, about
- [x] ScreenTerminal: built-in command interpreter (help, version, info, etc.)
- [x] Screen navigation: proper cleanup, back buttons, status bar buttons
- [ ] **Human review needed**: Validate pin assignments against T-Deck schematic
- [ ] **Human review needed**: Test TFT_eSPI User_Setup.h configuration
- [ ] **Human review needed**: Verify LVGL 9 integration with TFT_eSPI on ESP32-S3
- [ ] First flash to T-Deck hardware

## Phase 1: Core Chat (v0.2.0)

Goal: Send and receive messages on the mesh.

- [x] MeshCore MainBoard implementation for T-Deck
  - Battery voltage read (ADC on GPIO)
  - MCU temperature
  - Reboot/powerOff
- [x] MeshCore RTCClock implementation
  - Sync from GPS (T-Deck Plus)
  - Sync from BLE companion app
  - Manual set via settings
- [x] MeshCore radio init (SX1262, region config)
- [x] Identity: generate key on first boot, store in SPIFFS
- [x] Message send: channel messages via MeshCore
- [x] Message send: direct messages via MeshCore
- [x] Message receive: display incoming messages in chat UI
- [x] Message ring buffer (PSRAM, 1000 messages max)
- [x] Keyboard input: BBQ10KB I2C driver
  - Key events → LVGL textarea
  - Special keys (Enter = send, Esc = back)
- [x] Channel switching (tab buttons on status bar)
- [x] Battery/RSSI status bar indicators
- [x] Per-tab message buffers (64 messages each)
- [x] BLE companion app connectivity
- [x] Config import/export (MeshCore companion app format)
- [x] Unit tests for coordinate math (MapEngine)
- [x] Unit tests for Config save/load round-trip (SPIFFS)

## Phase 2: Map (v0.3.0)

Goal: Offline GPS map with node positions.

- [x] GPS serial driver (T-Deck Plus built-in GPS)
- [x] TinyGPSPlus integration
- [x] SD card initialization and tile directory scan
- [x] PNG tile decoder (lodepng)
- [x] Tile rendering to LVGL canvas (TileRenderer)
- [x] Pan (trackball left/right/up/down)
- [x] Zoom (trackball center press → zoom menu, or +/- buttons)
- [x] Node marker overlay (self, contacts, repeaters) — with tap-to-select + highlight ring
- [x] Node tap/select → info popup
- [x] Tile caching in PSRAM (LRU, 9 tiles)
- [x] Progressive tile loading (center first, edges after)
- [x] Tile download helper script (`scripts/download_tiles.py`)
- [x] Map screen accessible from home screen navigation

## Phase 3: Settings & Polish (v0.4.0)

Goal: Full settings, terminal, and polish.

- [x] Settings screen implementation
  - [x] Radio region selector (EU868, US915, AU915, etc.)
  - [x] Channel selector
  - [x] TX power slider
  - [x] Callsign editor
  - [x] Brightness slider
  - [x] Screen timeout selector
  - [x] Sound toggle
  - [x] Theme toggle (dark/light)
  - [x] BLE toggle
- [ ] Repeater scanner
  - [x] NodeTracker: fixed-size node tracker with onAdvert callback
  - [x] ScreenScanner: LVGL UI showing discovered nodes, type, RSSI, distance
  - [x] Haversine distance calculation from GPS
  - [x] Whitelist toggle (long-press)
  - [x] Whitelist persistence (SPIFFS /whitelist.bin)
  - [x] Settings menu entry (Node Scanner)
  - [ ] Test: verify node list updates from MeshCore adverts
  - [ ] Discover repeaters on the mesh
  - [x] Show signal strength, distance, uptime — quality column (+++/++/+/-), age (now/Xs/Xm/Xh), distance (m/km)
  - [x] Whitelist management UI (add/remove from settings) — June 5, 2026
- [x] Terminal screen
  - [x] Full MeshCore CLI
  - [x] Command history (up/down arrows)
  - [x] Multi-colour output (errors red, warnings orange, data green)
- [x] Notifications
  - [x] Sound on incoming message (buzzer)
  - [x] Screen wake on incoming message
- [x] Lock screen
  - Time, date, battery, node count
  - Press any key to unlock
  - Auto-dimming after timeout
- [x] OTA firmware update (via SD card)
- [x] OTA firmware update (via BLE) — implemented in BLECompanion

## Phase 4: Multi-Device (v0.5.0)

Goal: Support other ESP32-S3 LoRa devices.

- [ ] **Abstract Board interface**
  - [x] Define IBoard interface (IBoard.h) with BoardCaps, LoRaConfig, DisplayConfig
  - [x] Implement BoardTDeck (T-Deck specific impl of IBoard)
  - [x] BoardFactory::create() returns correct IBoard* per build target
  - [x] Backward-compatible Board wrapper delegates to BoardTDeck
  - [x] Pin constants: tdeck:: namespace (new) with pins:: (deprecated compat)
  - [x] Unit tests for IBoard (117 tests: T-Deck + Heltec V3 pins, configs, caps, battery, haversine, RSSI)
  - [x] TDeckBoard.cpp migrated to use tdeck:: namespace
  - [x] Migrate remaining callers from Board::instance() to BoardFactory::create()/theBoard()
  - [x] Migrate remaining callers from pins:: to tdeck::
  - [x] Remove Board.h compat wrapper (deleted in e16361e)
  - [x] `Board.h` becomes `BoardTDeck.h` (in hardware/)
  - [x] New `BoardHeltecV3.h/cpp` for Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262 + SSD1306 OLED)
  - [x] BoardFactory moved to BoardFactory.cpp with #ifdef platform selection
  - [x] Unit tests for Heltec V3 pin constants, BoardCaps, LoRaConfig, DisplayConfig
- [ ] PlatformIO environments for:
  - [x] `t-deck` (current)
  - [x] `t-deck-plus` (GPS variant)
  - [x] `heltec-v3` (ESP32-S3 + SX1262 + SSD1306 OLED)
  - [x] `rak-wisblock` (RAK4631: nRF52840 + SX1262 + SSD1306 OLED) — board support files created, PlatformIO env added, main.cpp path added; Config/MeshService/HeapMonitor adapted for nRF52 via PlatformCompat abstraction
- [x] Display driver abstraction (IDisplay.h: TFT SPI vs OLED I2C vs none)
- [x] Input abstraction (IInput.h: keyboard + trackball + touch + serial)
- [x] Build matrix in CI (t-deck, t-deck-plus, heltec-v3, rak-wisblock)

## Phase 5: Hardening (v1.0.0)

Goal: Production-ready firmware.

- [x] Power optimization (sleep modes, LoRa TX burst timing)
- [ ] Memory audit (no fragmentation after 72h runtime)
- [ ] Long-duration stress test (48h continuous operation)
- [x] SD card corruption prevention (SDCard manager with safe mount/unmount)
- [x] SPIFFS wear minimization (config writes debounced 5s, explicit saveNow() before reboot)
- [x] Watchdog timer (auto-reboot on hang after 30s)
- [x] Crash logging (save stack trace to SPIFFS, show on next boot)
- [x] Security audit (MeshCore encryption, no plaintext key storage) — see docs/SECURITY_AUDIT.md
- [x] User documentation (flashing guide, settings reference, FAQ) — docs/USER_GUIDE.md
- [x] Release binaries on GitHub with SHA-256 checksums
- [x] Web flasher page (like MeshCore's) — instructions in release notes

## Version Numbering

- `0.x.y` — Development builds, things may break
- `1.0.0` — First production release
- Versions only go forward, never backward