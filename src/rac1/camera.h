/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The camera decision: how fast the right stick turns the follow camera under the `camera` setting,
 * apart from game memory (DESIGN.md section 8). Written for this mod from rac1-decomp's
 * follow_camera_update, follow_camera_read_stick, follow_camera_turn_and_pitch and
 * follow_camera_orbit, and the same four functions in rac2-decomp: func_L00_004263B8,
 * func_L00_00424F98, func_L00_00425118 and func_L00_00425CD0.
 *
 * Each tick rac1 turns the camera's orbit by a gain times the right stick's x, aims the pitch with
 * the stick's y in small steps, and eases the eye after the orbit with a spring. The gain
 * (follow_cam_setting_value) is the speed table's entry for the game's camera speed option. The mod
 * never turns the camera itself. It sets the gain rac1 reads for one update, and the game layer
 * puts it back before the next tick (rac1 also reloads it from its table each update). PLAN.md
 * section 10 has the choices.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_CAMERA_H
#define RAC1_ENHANCED_MOVEMENT_CAMERA_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* The values of the `camera` setting. */
enum {
    BM_CAMERA_RAC1 = 0, /* the game's speed: the mod changes nothing */
    BM_CAMERA_RAC2 = 1, /* Going Commando's fastest step, whatever the game's option */
};

/* Returns rac1's speed table entry in radians a tick for the game's camera speed option
 * (opt_cam_yaw_rate: 0, 1 or 2, 1 by default), or 0 for an option it does not have. */
static inline float bm_rac1_camera_speed(int option) {
    // follow_camera_yaw_rate_table: slow, medium and fast.
    switch (option) {
        case 0:
            return RAC1_CAMERA_SPEED_SLOW;
        case 1:
            return RAC1_CAMERA_SPEED_MEDIUM;
        case 2:
            return RAC1_CAMERA_SPEED_FAST;
        default:
            return 0.0f;
    }
}

/* Returns rac2's entry for the same option (D_L00_001B15B8), or 0 for an option it lacks. */
static inline float bm_rac2_camera_speed(int option) {
    // rac2 has the same three steps, with a faster last one.
    switch (option) {
        case 0:
            return RAC2_CAMERA_SPEED_SLOW;
        case 1:
            return RAC2_CAMERA_SPEED_MEDIUM;
        case 2:
            return RAC2_CAMERA_SPEED_FAST;
        default:
            return 0.0f;
    }
}

/* Returns the gain for the follow camera's turn this tick. `gain` is what rac1 set: its table's
 * entry for `option`, or another mod's change of it.
 * - BM_CAMERA_RAC1 keeps it as it is.
 * - BM_CAMERA_RAC2 gives Going Commando's fastest step (2.2 degrees a tick, 132 a second) on every
 *   step of the game's option: the gain times rac2's fastest entry over rac1's entry for `option`,
 *   so another mod's scaling stays. At the default step both games turn at 78 degrees a second, so
 *   the setting asks for Going Commando's top speed (PLAN.md section 10). An option rac1's table
 *   does not have keeps the gain.
 * Both games count the gain per tick, unscaled at 50 Hz, so this needs no tick. A gain that is not
 * a finite number above 0 is returned as it is. */
static inline float bm_camera_gain(int setting, float gain, int option) {
    // A bad read or a zero gain has nothing to scale.
    if (!isfinite(gain) || gain <= 0.0f) {
        return gain;
    }

    // Going Commando's fastest step, entry 2 of its table.
    if (setting == BM_CAMERA_RAC2) {
        const float own = bm_rac1_camera_speed(option);

        // rac1's table has no entry for this option.
        if (own <= 0.0f) {
            return gain;
        }

        return gain * (bm_rac2_camera_speed(2) / own);
    }

    return gain;
}

/* Returns whether the stick turns the camera this tick, as rac1's camera reads it. `x` and `y` are
 * the pad's analog axes with rac1's dead zone already applied. The camera's flags (+0x1B8) can shut
 * an axis: 1 the turn, 2 the pitch. The pitch reads y only past 0.3 (follow_camera_turn_and_pitch).
 * game.cpp writes the trace's camera line when this is true. */
static inline int bm_camera_turning(float x, float y, int flags) {
    const int yaw = isfinite(x) && x != 0.0f && !(flags & 1);
    const int pitch = isfinite(y) && fabsf(y) > RAC1_CAMERA_PITCH_DEAD && !(flags & 2);
    return yaw || pitch;
}

#endif /* RAC1_ENHANCED_MOVEMENT_CAMERA_H */
