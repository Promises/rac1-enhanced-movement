/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * How fast the right stick turns the follow camera under the `camera` setting, decided apart from game
 * memory (DESIGN.md section 8). Written for this mod from rac1-decomp's follow_camera_update,
 * follow_camera_read_stick, follow_camera_turn_and_pitch and follow_camera_orbit, and rac2-decomp's
 * func_L00_004263B8, func_L00_00424F98, func_L00_00425118 and func_L00_00425CD0: the same four
 * functions, carried from one game to the next.
 *
 * Each tick rac1 turns the camera's orbit by a gain (follow_cam_setting_value, its speed table's entry
 * for the game's camera speed option) times the right stick's x, aims its pitch with the stick's y in
 * small steps, and eases the eye after the orbit with a spring. The mod never turns the camera itself.
 * It sets the gain rac1 reads for the one update (rac1 sets it again from its table at its next
 * update), and the game layer puts it back before the next tick. 0.3.0's third choice, `modern`, was
 * removed in 0.3.1 (PLAN.md section 10).
 */
#ifndef RAC1_ENHANCED_MOVEMENT_CAMERA_H
#define RAC1_ENHANCED_MOVEMENT_CAMERA_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* The setting's choices. */
enum {
    BM_CAMERA_RAC1 = 0, /* the game's own: the mod takes nothing */
    BM_CAMERA_RAC2 = 1, /* Going Commando's fastest step, whatever the game's option */
};

/* rac1's speed table's entry for the game's camera speed option (opt_cam_yaw_rate: 0, 1 or 2, 1 by
 * default), radians a tick; 0 for an option it does not have. */
static inline float bm_rac1_camera_speed(int option) {
    // follow_camera_yaw_rate_table.
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

/* rac2's (D_L00_001B15B8). */
static inline float bm_rac2_camera_speed(int option) {
    // The same three steps, the last faster.
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

/* The gain to give the follow camera's turn this tick. `gain` is what rac1 set (its table's entry for
 * `option`, or what another mod made of it):
 * - rac1: as it is;
 * - rac2: Going Commando's fastest step (2.2 degrees a tick, 132 a second) whichever step the game's
 *   option is on, as rac2's entry over rac1's, so another mod's scaling stays. At the game's default
 *   step the two games turn alike (78 degrees a second); the setting asks for Going Commando's top
 *   speed (PLAN.md section 10). An option rac1's table does not have keeps the gain.
 * Both games count the gain per tick, unscaled at 50 Hz, so this needs no tick. A gain that is no
 * number or not above 0 is left alone. */
static inline float bm_camera_gain(int setting, float gain, int option) {
    // Nothing to scale.
    if (!isfinite(gain) || gain <= 0.0f) {
        return gain;
    }

    // Going Commando's fastest step.
    if (setting == BM_CAMERA_RAC2) {
        const float own = bm_rac1_camera_speed(option);

        // An option rac1's table does not have.
        if (own <= 0.0f) {
            return gain;
        }

        return gain * (bm_rac2_camera_speed(2) / own);
    }

    return gain;
}

/* Whether the stick turns the camera this tick, as rac1's camera reads it: the pad's analog x and y
 * (rac1's dead zone already applied), unless the camera's flags (+0x1B8: 1 no turn, 2 no pitch) shut
 * an axis. The pitch reads the y only past 0.3 (follow_camera_turn_and_pitch). The trace's camera line
 * is written then. */
static inline int bm_camera_turning(float x, float y, int flags) {
    const int yaw = isfinite(x) && x != 0.0f && !(flags & 1);
    const int pitch = isfinite(y) && fabsf(y) > RAC1_CAMERA_PITCH_DEAD && !(flags & 2);
    return yaw || pitch;
}

#endif /* RAC1_ENHANCED_MOVEMENT_CAMERA_H */
