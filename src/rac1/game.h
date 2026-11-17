// SPDX-License-Identifier: GPL-3.0-or-later
//
// The rac1 part of the mod, implemented in game.cpp: it checks the disc, reads the settings and
// takes the hook points.
#ifndef RAC1_ENHANCED_MOVEMENT_GAME_H
#define RAC1_ENHANCED_MOVEMENT_GAME_H

#include "openrac/mod.hpp"

namespace enhanced_movement::rac1 {

// Loads the mod on rac1: refuses a disc it has no addresses for, reads the settings and takes the
// hook points. Throws openrac::Error to refuse.
void load(openrac::Mod& mod);

}  // namespace enhanced_movement::rac1

#endif  // RAC1_ENHANCED_MOVEMENT_GAME_H
