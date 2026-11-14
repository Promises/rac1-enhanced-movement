/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's strafe, decided apart from game memory (DESIGN.md section 4): the stick's
 * sector, the side timers, the flip, the twist, the start from standing, and the backward step of a
 * looping animation for the backpedal. Written for this mod from rac2-decomp: func_L00_002B39F0
 * (sectors), func_L00_002B35A0 (facing and sides), func_L00_002C9FC8 (flip), func_L00_002B6BC8
 * (the flip's drift), func_L00_002B7B80 (twist), func_L00_002CB3A8 (standing start).
 */
#ifndef RAC1_ENHANCED_MOVEMENT_STRAFE_H
#define RAC1_ENHANCED_MOVEMENT_STRAFE_H

#include <math.h>

#include "rac1/angle.h"
#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* A stick direction: rac2's +0x1457 and rac1's stick_direction_vs_facing use the same codes. */
enum {
    BM_LEFT = 0,
    BM_RIGHT = 1,
    BM_FORWARD = 2,
    BM_BACK = 3
};

/* The stick's sector on the screen (func_L00_002B39F0): its axes as the pad gives them (x right,
 * y down), angle atan2(y, x); back, then right, then left, within 45 degrees each, else forward. */
static inline int bm_stick_sector(float x, float y) {
    const float a = atan2f(y, x);

    // Down on the stick.
    if (bm_diff(a, BM_HALF_PI) < RAC2_STRAFE_SECTOR) {
        return BM_BACK;
    }

    // Right.
    if (bm_diff(a, 0.0f) < RAC2_STRAFE_SECTOR) {
        return BM_RIGHT;
    }

    // Left.
    if (bm_diff(a, BM_PI) < RAC2_STRAFE_SECTOR) {
        return BM_LEFT;
    }

    return BM_FORWARD;
}

/* Whether he backpedals: the stick's sector back and the stick past 0.15 (rac2's +0x1456). */
static inline int bm_backpedal(int sector, float stick) {
    return sector == BM_BACK && stick > RAC2_STRAFE_BACKPEDAL_STICK;
}

/* The stick against the facing (rac1's stick_direction_vs_facing, the flip's direction): the move
 * yaw less the facing, back, then right, then left within 45 degrees, else forward. */
static inline int bm_stick_vs_facing(float move_yaw, float facing) {
    const float a = bm_sub(move_yaw, facing);

    // Behind him.
    if (bm_diff(a, BM_PI) < 0.785398185f) {
        return BM_BACK;
    }

    // To his right (the yaw grows to his left).
    if (bm_diff(a, -BM_HALF_PI) < 0.785398185f) {
        return BM_RIGHT;
    }

    // To his left.
    if (bm_diff(a, BM_HALF_PI) < 0.785398185f) {
        return BM_LEFT;
    }

    return BM_FORWARD;
}

/* The side timers (rac2's +0x146E set going left, +0x146C set going right), in rac2 ticks. */
typedef struct bm_sides {
    int left;
    int right;
} bm_sides;

/* This tick's factor on the ground speed from the side timers (func_L00_002B35A0): both count down
 * first; on foot, going left while the right timer runs gives -0.25, going right while the left one
 * runs gives (4 - its count) / 4. */
static inline float bm_side_factor(bm_sides* s, int sector, int on_foot) {
    float f = 1.0f;

    // Both timers count down each strafing tick.
    if (s->left > 0) {
        s->left--;
    }

    // The other side's timer.
    if (s->right > 0) {
        s->right--;
    }

    // Only on foot does going one way after the other kill the speed.
    if (!on_foot) {
        return f;
    }

    // Left: mark it; coming from the right reverses a quarter of the speed.
    if (sector == BM_LEFT) {
        s->left = RAC2_STRAFE_SIDE_TICKS;

        // From the right within five ticks.
        if (s->right != 0) {
            f = RAC2_STRAFE_REVERSE;
        }
    } else if (sector == BM_RIGHT) {
        s->right = RAC2_STRAFE_SIDE_TICKS;

        // From the left within five ticks: back by quarters.
        if (s->left != 0) {
            f = (4.0f - (float)s->left) * 0.25f;
        }
    }

    return f;
}

/* Whether he is still turning to the camera's yaw while strafing (func_L00_002B3268). */
static inline int bm_strafe_turning(float facing, float camera_yaw, float angle) {
    return bm_diff(facing, camera_yaw) >= angle;
}

/* Whether idle goes to the walk to turn him (rac2's standing start): strafing, more than 10 degrees
 * off the camera, and a stick rac1 would not start on by itself (0.22 or under). */
static inline int bm_strafe_start(float facing, float camera_yaw, float stick) {
    return bm_strafe_turning(facing, camera_yaw, RAC2_STRAFE_START_ANGLE)
           && !(stick > RAC2_STRAFE_START_STICK);
}

/* Whether X flips (func_L00_002C9FC8): strafing, the stick past 0.9, not forward of his facing. */
static inline int bm_flip(float stick, int stick_vs_facing) {
    return stick > RAC2_FLIP_STICK && stick_vs_facing != BM_FORWARD;
}

/* The flip's drift (high_jump_drift_speed) to leave before rac1's tick of state 0xB, so that rac1's
 * easing (toward 4.2 by 8, or to 0 near landing: `rac1_to_zero`) lands on what rac2's gives: toward
 * max(stick * 5, 2) by 37 (func_L00_002B6BC8, the strafe's flip). */
static inline float bm_flip_drift_preset(
    float drift, float stick, int rac1_to_zero, const bm_tick* t
) {
    float goal = stick * rac2_speed(RAC2_FLIP_DRIFT_PER_STICK, t);
    const float rac1_goal = rac1_to_zero ? 0.0f : RAC1_FLIP_DRIFT * t->ft;
    const float rac1_step = RAC1_FLIP_DRIFT_STEP * t->ft2;
    float want;

    // rac2's floor.
    if (goal < rac2_speed(RAC2_FLIP_DRIFT_MIN, t)) {
        goal = rac2_speed(RAC2_FLIP_DRIFT_MIN, t);
    }

    want = bm_approach(drift, goal, rac2_accel(RAC2_FLIP_DRIFT_STEP, t));

    // rac1 will move it one step toward its own goal: start one step the other side.
    if (want > rac1_goal) {
        return want + rac1_step;
    }

    // Below rac1's goal, rac1 steps it up.
    if (want < rac1_goal) {
        return want - rac1_step;
    }

    return want;
}

/* The flip's drift when it starts (rac2's entry with the strafe: 5 units a second). */
static inline float bm_flip_drift_start(const bm_tick* t) {
    return rac2_speed(RAC2_FLIP_DRIFT_START, t);
}

/* The twist's hip yaw (pose record 0's target; record 1's is its negative): where he goes less where
 * he faces (func_L00_002B7B80). */
static inline float bm_twist(float strafe_yaw, float facing) {
    return bm_sub(strafe_yaw, facing);
}

/* Whether the twist applies: the stick past 0.15, not flipping (0xB), not backpedalling. */
static inline int bm_twist_on(float stick, int state, int backpedal) {
    return stick > RAC2_TWIST_STICK && state != RAC1_STATE_HIGH_JUMP && !backpedal;
}

/* A looping animation's place: between key `frame` and key `next` (frame + 1, wrapping), `time`
 * of the way (rac1's moby frame, prev_frame and anim_time). */
typedef struct bm_anim {
    float time;
    int frame;
    int next;
} bm_anim;

/* Steps a looping sequence back by `speed` (anim_speed) times the key's rate, as
 * advance_hero_animation steps it forward: a key's time is in its own rate, so time carried past a
 * key is divided by the old key's rate and multiplied by the new one's. `rates[i]` is key i's.
 * Returns the keys stepped back over. */
static inline int bm_anim_step_back(bm_anim* a, float speed, const float* rates, int count) {
    int moved = 0;

    // A sequence of no keys, or a bad place, stays where it is.
    if (count <= 0 || a->frame < 0 || a->frame >= count) {
        return 0;
    }

    a->time -= speed * rates[a->frame];

    // Past the key: back one, the time carried at the new key's rate (never more than a lap).
    while (a->time < 0.0f && moved < count) {
        const int old = a->frame;
        const float old_rate = rates[old];

        a->next = old;
        a->frame = (old + count - 1) % count;
        a->time = (old_rate > 0.0f ? a->time / old_rate * rates[a->frame] : 0.0f) + 1.0f;
        moved++;
    }

    // A lap and more in a tick only ever comes from bad rates.
    if (a->time < 0.0f) {
        a->time = 0.0f;
    }

    return moved;
}

/* A place as advance_hero_animation compares it with the sound triggers: key * 16 + sixteenths. */
static inline int bm_anim_place(const bm_anim* a) {
    return a->frame * 16 + (int)(a->time * 16.0f);
}

/* Whether stepping back from `from` to `to` (places) went over a trigger at `at`: from > at >= ...
 * the forward test (from < at <= to) turned round, and across the loop's end when it wrapped. */
static inline int bm_anim_crossed_back(int from, int to, int at, int wrapped) {
    // Within the lap: between the new place and the old.
    if (!wrapped) {
        return to < at && at <= from;
    }

    // Across the end: before the old place, or after the new one.
    return at <= from || to < at;
}

#endif /* RAC1_ENHANCED_MOVEMENT_STRAFE_H */
