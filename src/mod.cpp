// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mod's entry point (GUIDE.md section 4). It hands Ratchet & Clank (rac1) to src/rac1/game.cpp
// and refuses any other game with a reason.
#include "openrac/mod.hpp"

#include <string>

#include "rac1/game.h"
#include "version.h"

namespace {

// Loads the part of the mod for the running game, or refuses the game. The port calls it once at
// start, on the game's thread.
void load(openrac::Mod& mod) {
    // Only rac1 has addresses, hooks and decisions in this mod.
    if (mod.game() == "rac1") {
        enhanced_movement::rac1::load(mod);
        return;
    }

    throw openrac::Error(
        "rac1-enhanced-movement is for Ratchet & Clank (rac1), not " + std::string(mod.game())
    );
}

}  // namespace

OPENRAC_MOD_CPP(RAC1_ENHANCED_MOVEMENT_ID, RAC1_ENHANCED_MOVEMENT_VERSION, load)
