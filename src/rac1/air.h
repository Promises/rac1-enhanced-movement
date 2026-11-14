/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's air control, decided apart from game memory (DESIGN.md section 3). The jump
 * heights stay rac1's; only air_stick_speed (+0x424), set when a jump state is entered, changes.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_AIR_H
#define RAC1_ENHANCED_MOVEMENT_AIR_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* The states hero_update_movement's jump group runs (its case list), where air_stick_speed counts. */
static inline int bm_is_jump_state(int state) {
    // The jump group's states, as hero_update_movement lists them.
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

/* On a jump state's first tick: rac2's air stick speed where rac1 set its own default (5.7, and 3.5
 * in the double jump), else what rac1 set (its other values are rac2's too). */
static inline float bm_air_stick_speed(int state, float current, const bm_tick* t) {
    const float near = 1e-4f * t->ft;

    // The double jump: 3.5 to 4.75.
    if (state == RAC1_STATE_DOUBLE_JUMP
        && fabsf(current - RAC1_DOUBLE_JUMP_AIR_SPEED * t->ft) < near) {
        return rac2_speed(RAC2_DOUBLE_JUMP_AIR_SPEED, t);
    }

    // Every jump left at rac1's default: 5.7 to 5.95.
    if (fabsf(current - RAC1_AIR_STICK_SPEED * t->ft) < near) {
        return rac2_speed(RAC2_AIR_STICK_SPEED, t);
    }

    return current;
}

#endif /* RAC1_ENHANCED_MOVEMENT_AIR_H */
