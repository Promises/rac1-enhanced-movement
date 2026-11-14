// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ratchet & Clank's part of the mod (game.cpp): the disc, the settings, the hooks.
#ifndef RAC1_ENHANCED_MOVEMENT_GAME_H
#define RAC1_ENHANCED_MOVEMENT_GAME_H

#include "openrac/mod.hpp"

namespace enhanced_movement::rac1 {

// Loads the mod on rac1: refuses a disc it has no addresses for, reads the settings, takes the
// hook points. Throws openrac::Error to refuse.
void load(openrac::Mod& mod);

}  // namespace enhanced_movement::rac1

#endif  // RAC1_ENHANCED_MOVEMENT_GAME_H
