// OpenMeshOS — BoardFactory.cpp
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Board factory and global accessor. Selects the correct IBoard
// implementation based on build flags:
//   - OMS_PLATFORM_TDECK     → BoardTDeck (LilyGo T-Deck / T-Deck Plus)
//   - OMS_PLATFORM_HELTEC_V3 → BoardHeltecV3 (Heltec WiFi LoRa 32 V3)
//   - OMS_PLATFORM_RAK4631   → BoardRAK4631 (RAK WisBlock RAK4631, nRF52840)
//
// If no platform is defined, compilation will fail with an error.

#include "IBoard.h"
#include "utils/Log.h"

// ── Include platform-specific headers ────────────────────────────────
#ifdef OMS_PLATFORM_TDECK
  #include "BoardTDeck.h"
#elif defined(OMS_PLATFORM_HELTEC_V3)
  #include "BoardHeltecV3.h"
#elif defined(OMS_PLATFORM_RAK4631)
  #include "BoardRAK4631.h"
#else
  #error "No OpenMeshOS platform defined. Define OMS_PLATFORM_TDECK, OMS_PLATFORM_HELTEC_V3, or OMS_PLATFORM_RAK4631."
#endif

namespace oms {

// ── Global board accessor ────────────────────────────────────────────
IBoard* theBoard() {
    static IBoard* s_board = BoardFactory::create();
    return s_board;
}

// ── Board Factory ────────────────────────────────────────────────────
IBoard* BoardFactory::create() {
#ifdef OMS_PLATFORM_TDECK
    OMS_LOG("board", "Creating T-Deck board");
    return &BoardTDeck::instance();
#elif defined(OMS_PLATFORM_HELTEC_V3)
    OMS_LOG("board", "Creating Heltec V3 board");
    return &BoardHeltecV3::instance();
#elif defined(OMS_PLATFORM_RAK4631)
    OMS_LOG("board", "Creating RAK4631 board");
    return &BoardRAK4631::instance();
#else
    #error "No platform defined"
#endif
}

}  // namespace oms