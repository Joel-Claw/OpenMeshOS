// OpenMeshOS — Board.cpp (backward compat wrapper)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// The Board singleton now delegates to BoardTDeck.
// All hardware init logic has moved to BoardTDeck.cpp.
// This file only provides the static instance wiring.

#include "Board.h"

namespace oms {

BoardTDeck* Board::s_tdeck = &BoardTDeck::instance();
Board Board::s_self;

}  // namespace oms