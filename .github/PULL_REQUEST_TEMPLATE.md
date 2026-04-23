# Contributing to OpenMeshOS

First: thank you. This project needs hardware testers more than anything.

## Quick Facts

- **License:** WTFPL v2 (Do What The Fuck You Want To Public License)
- **Code origin:** Vibecoded by GLM-5.1, iterated by humans
- **Hardware:** LilyGo T-Deck / T-Deck Plus (ESP32-S3)
- **Framework:** PlatformIO + Arduino
- **UI:** LVGL 9
- **Mesh:** MeshCore (git submodule)

## Code Conventions

- **No dynamic allocation after setup()** — match MeshCore convention
- Allman/BSD brace style (opening brace on own line)
- 4-space indentation
- C++14 compatible
- `oms::` namespace for all OpenMeshOS code

## Before Submitting a PR

1. **Build locally:** `pio run -e t-deck`
2. **Run host tests:** `g++ -std=c++14 -o test_map test/test_map_engine.cpp -lm && ./test_map`
3. **No new dependencies** without discussion — supply chain risk
4. **No workflow changes** without maintainer approval
5. **WTFPL v2 header** on all new source files

## What We Need Most

1. **Hardware testers** — flash the firmware, tell us what breaks
2. **Screen implementations** — ScreenMap, ScreenSettings, ScreenTerminal
3. **Keyboard driver** — BBQ10KB I2C integration
4. **MeshCore wiring** — MainBoard + RTCClock implementations for T-Deck

## Security

- Report vulnerabilities privately to the maintainers
- Do not post exploit details in public issues
- Dependencies are reviewed before merging