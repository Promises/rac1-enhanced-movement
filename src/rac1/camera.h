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
 * It sets what rac1 reads for the one update: the gain (rac1 sets it again from its table at its next
 * update), and for `modern` the pitch's steps and, while the stick turns the camera, the eye's springs.
 * The game layer puts those back before the next tick.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_CAMERA_H
#define RAC1_ENHANCED_MOVEMENT_CAMERA_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* The setting's choices. */
enum {
    BM_CAMERA_RAC1 = 0,   /* the game's own: the mod takes nothing */
    BM_CAMERA_RAC2 = 1,   /* Going Commando's fastest step, whatever the game's option */
    BM_CAMERA_MODERN = 2, /* three times the game's speed, the eye held close */
};

/* `modern`: three times the game's own rate, in degrees a second: 234 at the game's default step (1.3
 * degrees a tick at 60 Hz), 180 and 288 at the others. That is three times both games' default and
 * 1.8 times Going Commando's fastest (132), in the 180 to 300 degrees a second current third-person
 * games' default stick turns span (PLAN.md section 10). */
#define BM_MODERN_CAMERA_SCALE 3.0f
/* `modern`'s eye spring while the stick turns the camera, a tick at 60 Hz (rac1's own is 0.015 / 0.2):
 * 90% of the stick's rate within 6 ticks (a tenth of a second) where rac1 takes 24, and a stop within
 * 10 ticks of letting go, where rac1 coasts on for 30. camera_test.c works it through rac1's
 * rope_spring_turn. */
#define BM_MODERN_CAMERA_SPRING_ACCEL 0.2f
#define BM_MODERN_CAMERA_SPRING_DAMP 0.7f
/* How long `modern`'s spring stays after the stick lets go, in ticks at 60 Hz: twice the stop. */
#define BM_MODERN_CAMERA_SETTLE_TICKS 20

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
 *   speed (PLAN.md section 10). An option rac1's table does not have keeps the gain;
 * - modern: three times, as degrees a second (times 60 x g_frame_time, so 50 Hz turns as fast).
 * A gain that is no number or not above 0 is left alone. */
static inline float bm_camera_gain(int setting, float gain, int option, const bm_tick* t) {
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

    // Modern.
    if (setting == BM_CAMERA_MODERN) {
        return gain * BM_MODERN_CAMERA_SCALE * 60.0f * t->ft;
    }

    return gain;
}

/* One of the pitch's three steps (follow_camera_pitch_input_tracking, _return, _angle_step_limit:
 * radians or shares of the stick a tick) for this tick: `modern` scales them as it scales the gain, so
 * the pitch follows the stick three times as fast; the others leave them. */
static inline float bm_camera_pitch_step(int setting, float step, const bm_tick* t) {
    // Only modern, and only a step.
    if (setting != BM_CAMERA_MODERN || !isfinite(step) || step <= 0.0f) {
        return step;
    }

    return step * BM_MODERN_CAMERA_SCALE * 60.0f * t->ft;
}

/* Whether the stick turns the camera this tick, as rac1's camera reads it: the pad's analog x and y
 * (rac1's dead zone already applied), unless the camera's flags (+0x1B8: 1 no turn, 2 no pitch) shut
 * an axis. The pitch reads the y only past 0.3 (follow_camera_turn_and_pitch). */
static inline int bm_camera_turning(float x, float y, int flags) {
    const int yaw = isfinite(x) && x != 0.0f && !(flags & 1);
    const int pitch = isfinite(y) && fabsf(y) > RAC1_CAMERA_PITCH_DEAD && !(flags & 2);
    return yaw || pitch;
}

/* Whether the pitch is the player's this tick: the stick's y past 0.3 (unless the flags shut the
 * pitch), or the pitch's aim (+0x1B0, the share of 40 degrees the stick last asked for) not yet back
 * at 0. Only then does `modern` scale the pitch's steps: rac1 uses the same steps to level the orbit
 * again after anything else tilts it (a jump, a slope, a swing), and those stay the game's. */
static inline int bm_camera_pitching(float y, int flags, float aim) {
    const int stick = isfinite(y) && fabsf(y) > RAC1_CAMERA_PITCH_DEAD && !(flags & 2);
    return stick || (isfinite(aim) && aim != 0.0f);
}

/* The ticks of `modern`'s spring left after this one: all of them while the stick turns the camera,
 * one fewer each tick after (counted as rac1 counts its ticks: more at 50 Hz). The spring holds this
 * tick while the stick turns the camera or `left` (before this tick) is above 0: 20 ticks after
 * letting go. */
static inline int bm_camera_settle(int left, int turning, const bm_tick* t) {
    // Turning: the whole window again.
    if (turning) {
        return bm_scale_ticks(BM_MODERN_CAMERA_SETTLE_TICKS, t);
    }

    return left > 0 ? left - 1 : 0;
}

/* `modern`'s eye spring at this tick: the accel by g_frame_scale_sq and the damping by frame_scale,
 * which keep a spring of one tick's steps as quick in seconds at 50 Hz. */
static inline float bm_camera_spring_accel(const bm_tick* t) {
    return BM_MODERN_CAMERA_SPRING_ACCEL * t->fs2;
}

static inline float bm_camera_spring_damp(const bm_tick* t) {
    return BM_MODERN_CAMERA_SPRING_DAMP * t->fs;
}

#endif /* RAC1_ENHANCED_MOVEMENT_CAMERA_H */
