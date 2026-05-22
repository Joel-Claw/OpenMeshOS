# TODO — OpenMeshOS

## Immediate (before first flash)

- [x] Verify all GPIO pin assignments against T-Deck schematic — done from docs, needs hardware validation
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
- [ ] Trackball down not working on some T-Deck models (I2C variant)
- [ ] Test trackball on both hardware revisions

## Research Sources

- [ ] Review Aurora firmware trackball implementation
- [ ] Review LilyGo official T-Deck examples
- [ ] Review community firmware projects (beyond MeshOS)
- [ ] Don't just copy MeshOS patterns - cross-reference multiple implementations

## MeshCore Integration

- [x] Create `TDeckBoard` class implementing `mesh::MainBoard`
  - [x] `getBattMilliVolts()` — ADC on GPIO1
  - [x] `getMCUTemperature()` — ESP32 internal temp
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
- [ ] ScreenTerminal: MeshCore CLI passthrough (basic command interpreter done, full passthrough pending)
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
- [ ] Integration test: SPIFFS read/write under load
- [ ] Hardware test: keyboard scan produces expected keycodes
- [ ] Hardware test: GPS NMEA sentences decode correctly
- [ ] Stress test: 24h continuous operation, check heap fragmentation

## Polish

- [ ] Font size audit (nothing below 10px)
- [ ] Touch responsiveness tuning
- [ ] Trackball debounce and acceleration
- [x] Sound/buzzer notification on incoming message
- [x] Screen wake on incoming message
- [x] Battery icon in status bar with live voltage
- [x] Auto-dimming and screen lock on idle timeout
- [x] OTA firmware update via SD card
- [x] Release binary + checksums on GitHub
- [x] Config export/import via SD card (ConfigExport.h/cpp)