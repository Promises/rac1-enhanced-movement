/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The air control decision: Going Commando's air stick speeds, apart from game memory (DESIGN.md
 * section 3). Jump heights stay rac1's. Only air_stick_speed (+0x424) changes, which a jump state
 * sets when it is entered.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_AIR_H
#define RAC1_ENHANCED_MOVEMENT_AIR_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* Returns whether a state is in hero_update_movement's jump group, where air_stick_speed counts. */
static inline int bm_is_jump_state(int state) {
    // The case list of the jump group in hero_update_movement.
    switch (state) {
        case 0x07:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x1C:
        case 0x3C:
        case 0x49:
        case 0x4C:
        case 0x5E:
        case 0x69:
            return 1;
        default:
            return 0;
    }
}

/* Returns the air stick speed for a jump state's first tick. Where rac1 set its default (5.7 u/s,
 * or 3.5 in the double jump) this is rac2's; any other value is the same in both games and
 * stays. */
static inline float bm_air_stick_speed(int state, float current, const bm_tick* t) {
    // A tolerance for float rounding, 0.0001 u/s.
    const float near = 1e-4f * t->ft;

    // The double jump goes from 3.5 to 4.75 u/s.
    if (state == RAC1_STATE_DOUBLE_JUMP
        && fabsf(current - RAC1_DOUBLE_JUMP_AIR_SPEED * t->ft) < near) {
        return rac2_speed(RAC2_DOUBLE_JUMP_AIR_SPEED, t);
    }

    // Any jump at rac1's default goes from 5.7 to 5.95 u/s.
    if (fabsf(current - RAC1_AIR_STICK_SPEED * t->ft) < near) {
        return rac2_speed(RAC2_AIR_STICK_SPEED, t);
    }

    return current;
}

#endif /* RAC1_ENHANCED_MOVEMENT_AIR_H */
