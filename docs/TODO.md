# TODO — OpenMeshOS

## Immediate (before first flash)

- [x] Verify all GPIO pin assignments against T-Deck schematic — done from docs, needs hardware validation
  - **CORRECTED** (June 4): cross-referenced with official LilyGo utilities.h and Meshtastic variant.h
  - Fixed: LORA_RST (12→17), LORA_DIO1 (14→45), LORA_MISO (41→38), SD_CS (11→39), BAT_ADC (1→4)
  - Fixed: DISP_CS (4→12), DISP_DC (5→11), DISP_BL (2→42), SPI begin() params
  - **V1 trackball pins were WRONG**: GPIO 21=mic LRCK, 43=GPS TX, 44=GPS RX (not trackball)
  - All GPIO trackballs use pins 3,15,1,2,0 (confirmed by Meshtastic + LilyGo)
  - **IBoard migration complete (June 15)**: Board.h/cpp deleted, replaced by IBoard.h + BoardTDeck.h/cpp
  - Pin constants now in tdeck:: namespace inside BoardTDeck.h
- [x] Create TFT_eSPI `User_Setup.h` for T-Deck ST7789 config
- [x] Test first compile: `pio run -e t-deck` — compiles clean (870KB)
- [x] Fix all compile errors — all resolved
- [x] ScreenMap, ScreenSettings, ScreenTerminal implemented
- [ ] Flash to T-Deck, verify serial output appears
- [ ] Confirm display shows something (even just a black screen with backlight)

## Keyboard Driver

- [x] Implement BBQ10KB I2C driver (`src/hardware/Keyboard.h/cpp`)
- [x] Key event mapping to LVGL via indev bridge (`src/hardware/KeyboardInput.h/cpp`)
- [x] Special keys: Enter, Esc, Tab, Backspace
- [x] Modifier tracking: Shift, Ctrl, Alt, Sym
- [x] Enter key sends message (LV_EVENT_READY on textarea)
- [ ] Test all keys produce correct input

## Trackball

- [x] Detect I2C trackball variant (AFBR S10) vs GPIO variant
- [x] Implement I2C trackball polling for affected models
- [x] Add I2C drift suppression and dead zone filter for AFBR S10 (fixes "scrolling itself down" bug, MeshCore #1469)
- [x] **Fix V1 trackball pin definitions** — were WRONG (GPIO 21/43/44 are mic/GPS, not trackball)
- [x] Unified GPIO trackball: all boards use pins 3,15,1,2,0
- [ ] Test trackball on both hardware revisions

## Research Sources

- [x] Review Aurora firmware trackball implementation
- [ ] Review LilyGo official T-Deck examples
- [x] Review community firmware projects (MeshCore #1469, #1424, Meshtastic #9440)
- [x] Cross-reference: Meshtastic trackball revamp, LilyGo T-Deck #71

## MeshCore Integration

- [x] Create `TDeckBoard` class implementing `mesh::MainBoard`
  - [x] `getBattMilliVolts()` — ADC on GPIO1
  - [x] `getMCUTemperature()` — ESP32 internal temp

## OpenMeshChat Integration

- [x] OpenMeshChat class created (BaseChatMesh subclass)
- [x] Fix getContactByIdx to return correct contact by index (was always returning first)
- [x] Add file header (magic+version+count) to contacts persistence
- [x] Refactor MeshService to use OpenMeshChat instead of OpenMesh (Mesh subclass)
- [x] Message callback bridge: OpenMeshChat → MessageBus → UI (UI screens unchanged)
- [x] NodeTracker fed from OpenMeshChat::onDiscoveredContact
- [x] sendChannel uses OpenMeshChat::sendChannelMessage (PSK group channel)
- [x] sendDirect uses OpenMeshChat::sendDirectMessage (with contact lookup)
- [x] Build passes on t-deck and heltec-v3
- [x] All host-side tests pass (7/7)
- [ ] Benefits to verify on hardware: automatic contact discovery, group channels with PSK, DM ACKs
- [ ] Send periodic adverts (sendAdvert) on a timer in the main loop
  - [x] `reboot()` — ESP.restart()
  - [x] `getResetReason()` — esp_reset_reason()
  - [x] `getManufacturerName()` — return "LilyGo"
  - [x] `getStartupReason()` — check RTC memory
- [x] Create `TDeckClock` class implementing `mesh::RTCClock`
  - [x] GPS time sync
  - [x] NTP fallback
  - [x] millis() drift tracking
- [x] Wire TDeckBoard + TDeckClock into MeshService
- [x] Load identity from SPIFFS (generate on first boot)
- [x] Configure radio region (EU868 / US915 etc) from Config
- [x] Start MeshCore loop in MeshService::tick()
- [x] Wire MeshService::sendChannel/sendDirect through MeshCore
- [x] Wire mesh receive path to UI via MessageBus
- [x] Wire hopCount/rssi from MeshCore state
- [x] Wire MeshCore radio init with SX1262 pin config
- [x] Wire MeshCore serial interface (BLE companion)
- [ ] Test: can we see adverts from other nodes?

## UI

- [x] ScreenHome: wire send button to MeshService::sendChannel()
- [x] ScreenHome: wire incoming messages to bubble list (via MessageBus)
- [x] ScreenHome: channel tabs (#Public, CH1, DM) with per-tab message buffers
- [x] ScreenHome: battery voltage and RSSI status bar indicators
- [x] ScreenHome: message timestamps (HH:MM)
- [x] ScreenHome: Enter key on keyboard sends message
- [x] ScreenHome: notification sound + screen wake on incoming message
- [x] ScreenHome: push messages to BLE companion on receive
- [x] ScreenMap: create LVGL canvas for tile rendering
- [x] ScreenMap: TileRenderer with PNG decode (lodepng), SD card, PSRAM LRU cache
- [x] ScreenMap: implement touch/trackball pan
- [x] ScreenMap: implement zoom controls
- [x] ScreenMap: node markers rendering via TileRenderer::drawNodes
- [x] ScreenMap: node info popup on tap
- [x] ScreenSettings: implement all config fields with live save (callsign, region, brightness, timeout, sound)
- [x] ScreenSettings: BLE companion toggle
- [x] ScreenSettings: implement OTA firmware update sub-page
- [x] ScreenTerminal: MeshCore CLI passthrough + command history + multi-color output
- [x] ScreenLock: implement with auto-dimming
- [ ] Test: trackball navigation between all screens

## Map

- [x] Integrate PNG decoder (lodepng)
- [x] SD card init and tile directory scan on boot
- [x] Render tiles to LVGL canvas (TileRenderer)
- [x] Pan with trackball (wire trackball events to MapEngine::pan)
- [x] Zoom in/out (wire trackball/buttons to MapEngine::zoomIn/zoomOut)
- [x] Node markers (TileRenderer::drawNodes wired in ScreenMap::refresh)
- [x] Node info popup on tap
- [x] PSRAM tile cache (LRU eviction)
- [x] Write `scripts/download_tiles.py`

## Testing

- [x] Unit test: MapEngine coordinate conversion (lat/lng to/from tile) — 34 tests passing
- [x] Unit test: Config save/load round-trip (SPIFFS mock)
- [x] Unit test: SPIFFS stress test (rapid writes, interleaved, corrupt recovery, whitelist binary) — 213 tests passing
- [x] Integration test: SPIFFS read/write under load — 2255 tests passing
- [ ] Hardware test: keyboard scan produces expected keycodes
- [ ] Hardware test: GPS NMEA sentences decode correctly
- [ ] Stress test: 24h continuous operation, check heap fragmentation

## Polish

- [x] Font size audit (nothing below 10px) — all fonts montserrat_10+
- [ ] Touch responsiveness tuning
- [x] Trackball debounce and acceleration (3-tick debounce, quadratic acceleration)
- [x] Sound/buzzer notification on incoming message
- [x] Screen wake on incoming message
- [x] Battery icon in status bar with live voltage
- [x] Auto-dimming and screen lock on idle timeout
- [x] OTA firmware update via SD card
- [x] Release binary + checksums on GitHub
- [x] Config export/import via SD card (ConfigExport.h/cpp)