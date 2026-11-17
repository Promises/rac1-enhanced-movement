/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The run decisions: Going Commando's running, turning and stopping, apart from game memory
 * (DESIGN.md section 2). game.cpp fills bm_run_in from the hero and applies the results; the tests
 * call the functions with values worked out by hand. The numbers are rac2's (rac2.h).
 *
 * rac2's run (func_L00_002BA7D8 cases 2 and 0x70) works in this order:
 *   1. It sets the along and turnaround flags from the previous tick's yaw error
 *      (bm_run_flags_update).
 *   2. It takes the target speed from the stick (bm_run_speed).
 *   3. It turns, on the run band's spring or on func_L00_002B3F68's, with k from the yaw error
 *      before the turn (bm_run_turn, bm_run_k).
 *   4. It applies the turnaround's cap and scale and the slow stick's cut, from the yaw error after
 *      the turn, then takes the speed step (bm_run_speed).
 *   5. It moves him along the move yaw while `along` is set, else along his facing.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_GROUND_H
#define RAC1_ENHANCED_MOVEMENT_GROUND_H

#include "rac1/angle.h"
#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* The hero values the run decisions read. */
typedef struct bm_run_in {
    float stick;       /* stick amount, 0 to 1 (+0x229C) */
    float yaw_error;   /* move yaw less facing, signed (+0x188) */
    float flat_speed;  /* ground speed, this tick's units (+0x164) */
    int step;          /* state.step: 1 is the run band (+0x2088) */
    int wading;        /* in state 0x73, wading */
    int item_flag;     /* unk20AA, set while an item's animation locks the turn (+0x20AA) */
    int firing;        /* a hold-to-fire weapon in hand with its animation on */
    float stick_swing; /* angle between the stick 1 and 7 polls back (the pad's hist_ang) */
    int strafing;      /* the mod's strafe is on this tick */
} bm_run_in;

/* rac2's run flags, which the mod keeps itself. */
typedef struct bm_run_flags {
    int along;      /* +0x40C: move along the stick */
    int turnaround; /* +0x40F: turning round */
} bm_run_flags;

/* A turn spring: Hero_TurnToHeading's accel, damping and limit, in this tick's units. */
typedef struct bm_turn {
    float accel;
    float damp;
    float limit;
} bm_turn;

/* A speed step: the target speed in this tick's units, and Hero_ApproachSpeed's up and down. */
typedef struct bm_speed {
    float target;
    float up;
    float down;
} bm_speed;

/* Updates the run flags for step 1 from the previous tick's yaw error, before the run's tick, since
 * rac2 sets them first. */
static inline void bm_run_flags_update(bm_run_flags* f, const bm_run_in* in, const bm_tick* t) {
    const float err = fabsf(in->yaw_error);

    // While strafing rac2 sets neither flag and keeps `along`; its strafe zeroes the yaw error.
    if (!in->strafing) {
        // Under 1.7 u/s he moves along the stick, and a full stick far off starts a turnaround.
        if (in->flat_speed < rac2_speed(RAC2_ALONG_SPEED, t)) {
            f->along = 1;

            // A full stick more than 120 degrees off.
            if (err > RAC2_TURNAROUND_SLOW_ANGLE && in->stick == 1.0f) {
                f->turnaround = 1;
            }
        }

        // A full stick swung more than 110 degrees in six polls, over 70 degrees off, turns him
        // round at any speed.
        if (in->stick_swing > RAC2_TURNAROUND_FLICK_ANGLE && err > RAC2_TURNAROUND_FLICK_ERROR
            && in->stick == 1.0f) {
            f->turnaround = 1;
            f->along = 1;
        }

        // Facing the stick within 5 degrees, he runs along his facing again.
        if (err < RAC2_ALONG_CLEAR) {
            f->along = 0;
        }
    }

    // The turnaround ends within 4 degrees.
    if (f->turnaround && err < RAC2_TURNAROUND_CLEAR) {
        f->turnaround = 0;
    }
}

/* Returns whether the run band's turn applies (rac2's first branch), which also keeps k at 1. */
static inline int bm_run_band_turn(const bm_run_in* in, const bm_run_flags* f) {
    return in->step == 1 && !f->turnaround && !in->item_flag;
}

/* Returns the turn spring for step 3 when not strafing. */
static inline bm_turn bm_run_turn(const bm_run_in* in, const bm_run_flags* f, const bm_tick* t) {
    const float stick = in->stick;
    float accel;
    float damp = RAC2_WALK_TURN_DAMP;
    float limit;
    bm_turn out;

    // The run band turns on its own spring.
    if (bm_run_band_turn(in, f)) {
        out.accel = rac2_spring(RAC2_RUNBAND_TURN_ACCEL, t);
        out.damp = rac2_spring(RAC2_RUNBAND_TURN_DAMP, t);
        out.limit = rac2_turn_limit(RAC2_RUNBAND_TURN_LIMIT, t);
        return out;
    }

    // In func_L00_002B3F68 the accel and limit grow with the stick, up to their maximum at 1.
    if (stick == 1.0f) {
        accel = RAC2_WALK_TURN_ACCEL_MAX;
        limit = RAC2_WALK_TURN_LIMIT_MAX;
    } else {
        accel = bm_clamp(
            stick * RAC2_WALK_TURN_ACCEL_PER_STICK,
            RAC2_WALK_TURN_ACCEL_MIN,
            RAC2_WALK_TURN_ACCEL_MAX
        );
        limit = bm_clamp(
            stick * RAC2_WALK_TURN_LIMIT_PER_STICK,
            RAC2_WALK_TURN_LIMIT_MIN,
            RAC2_WALK_TURN_LIMIT_MAX
        );
    }

    // The turnaround turns harder.
    if (f->turnaround) {
        accel = RAC2_TURNAROUND_TURN_ACCEL;
        limit = RAC2_TURNAROUND_TURN_LIMIT;
    }

    // Firing a hold-to-fire weapon slows the turn, except in a turnaround.
    if (in->firing && !f->turnaround) {
        // Past 0.95 of the stick the spring is fixed; below that it scales with the stick.
        if (stick > RAC2_FIRING_FULL_STICK) {
            accel = RAC2_FIRING_ACCEL;
            damp = RAC2_FIRING_DAMP;
            limit = RAC2_FIRING_LIMIT;
        } else {
            accel = stick * RAC2_FIRING_ACCEL_PER_STICK;
            damp = RAC2_FIRING_DAMP_SLOW;
            limit = stick * RAC2_FIRING_LIMIT_PER_STICK;
        }
    }

    out.accel = rac2_spring(accel, t);
    out.damp = rac2_spring(damp, t);
    out.limit = rac2_turn_limit(limit, t);
    return out;
}

/* Returns k for step 3, the share of the acceleration the speed step gets, from the yaw error
 * before the turn. He does not speed up while more than 45 degrees off with a part stick or an
 * item's lock, except in the run band. */
static inline float bm_run_k(const bm_run_in* in, const bm_run_flags* f) {
    // The run band never stops speeding up.
    if (bm_run_band_turn(in, f)) {
        return 1.0f;
    }

    // More than 45 degrees off, with the stick under 0.9 or an item's lock outside a turnaround.
    if ((in->stick < RAC2_K_STICK || (in->item_flag && !f->turnaround))
        && fabsf(in->yaw_error) > RAC2_K_ANGLE) {
        return 0.0f;
    }

    return 1.0f;
}

/* Returns the speed step for steps 2 and 4: the target, up and down, from the yaw error after the
 * turn and from k. rac2's yaw error is 0 while strafing, so game.cpp passes 0 then and the caps do
 * nothing. */
static inline bm_speed bm_run_speed(
    const bm_run_in* in, const bm_run_flags* f, float k, const bm_tick* t
) {
    const float err = fabsf(in->yaw_error);
    bm_speed out;

    // Under 0.2 the stick asks for no speed; above it, the run speed times the stick.
    if (in->stick < RAC2_STICK_RELEASE) {
        out.target = 0.0f;
    } else {
        out.target = RAC2_RUN_SPEED * in->stick * t->ft;

        // Wading is slower, but never under 2.5 u/s.
        if (in->wading) {
            out.target *= RAC2_WADING_SCALE;

            // A small stick would wade under the floor.
            if (out.target < rac2_speed(RAC2_WADING_MIN, t)) {
                out.target = rac2_speed(RAC2_WADING_MIN, t);
            }
        }
    }

    out.up = k * rac2_accel(RAC2_RUN_ACCEL, t);
    out.down = rac2_accel(RAC2_RUN_BRAKE, t);

    // In a turnaround the target is capped by how far off he faces, then scaled by the signed
    // error, and he brakes at 17 u/s^2.
    if (f->turnaround) {
        float cap =
            rac2_speed(RAC2_TURNAROUND_CAP, t) - err * rac2_speed(RAC2_TURNAROUND_CAP_SLOPE, t);

        // The cap never goes under 0.
        if (cap < 0.0f) {
            cap = 0.0f;
        }

        // The cap only ever lowers the target.
        if (cap < out.target) {
            out.target = cap;
        }

        // rac2 keeps the scale within 0.2 to 1.
        out.target *= bm_clamp(RAC2_TURNAROUND_SCALE_BASE - in->yaw_error, 0.2f, 1.0f);
        out.down = rac2_accel(RAC2_TURNAROUND_BRAKE, t);
    }

    // Under 0.75 of the stick and more than 35 degrees off, he slows with the error, down to 0.
    if (in->stick < RAC2_SLOW_STICK && err > RAC2_SLOW_ANGLE) {
        float share = 1.0f - err * RAC2_SLOW_FACTOR;
        out.target *= share < 0.0f ? 0.0f : share;
    }

    return out;
}

/* Returns the `down` rac1 itself passes in the run: 8.5 u/s^2, or 17 with its turnaround flag.
 * Whatever came in over this is another mod's scale. */
static inline float bm_rac1_run_down(int rac1_turnaround, const bm_tick* t) {
    return (rac1_turnaround ? RAC1_TURNAROUND_BRAKE : RAC1_RUN_BRAKE) * t->ft2;
}

/* Returns whether a state is in the idle group, which brakes at 12.6 u/s^2 in rac1 and 17.7 in
 * rac2. */
static inline int bm_is_idle_group(int state) {
    // The idle group of hero_update_movement, without its dying states.
    switch (state) {
        case RAC1_STATE_IDLE:
        case RAC1_STATE_LOOK:
        case RAC1_STATE_ITEM_BUTTON_RELEASE_WAIT:
        case 0x1D:
        case RAC1_STATE_LOOK_RELEASE_WAIT:
        case 0x1F:
        case 0x70:
        case 0x71:
        case 0x72:
            return 1;
        default:
            return 0;
    }
}

/* Returns how much harder rac2 brakes than rac1 in the idle group, STOP and the crouch, as a factor
 * on Hero_ApproachSpeed's `down`; rac2's per-tick-squared numbers are x 3600 per second squared.
 * Returns 1 where both games brake alike and in states the mod leaves alone. */
static inline float bm_brake_scale(int state, int on_ice) {
    // STOP brakes at 27 * t in rac2 against 12 * t in rac1.
    if (state == RAC1_STATE_STOP) {
        return RAC2_STOP_BRAKE * 3600.0f / RAC1_STOP_BRAKE;
    }

    // The crouch brakes at 27 against 6.48, and at 1.7 in both games on ice.
    if (state == RAC1_STATE_CROUCH) {
        return on_ice ? 1.0f : RAC2_CROUCH_BRAKE * 3600.0f / RAC1_CROUCH_BRAKE;
    }

    // Idle, look and the item waits brake at 17.7 against 12.6.
    if (bm_is_idle_group(state)) {
        return RAC2_IDLE_BRAKE * 3600.0f / RAC1_IDLE_BRAKE;
    }

    return 1.0f;
}

/* Returns whether rac2 leaves the run for idle now: the stick is under 0.17 and, while strafing, he
 * faces within 2 degrees of the camera (func_L00_002B3268). */
static inline int bm_run_exit(float stick, int strafing, float facing_off_camera) {
    return stick < RAC2_EXIT_STICK && !(strafing && facing_off_camera >= RAC2_EXIT_TURNED);
}

/* Returns the speed, 1.5 u/s, that rac2 clamps his momentum and ground speed to on entering idle
 * from the run. */
static inline float bm_exit_clamp(const bm_tick* t) {
    return rac2_speed(RAC2_EXIT_CLAMP, t);
}

/* Returns whether X in the run gives rac2's running jump rather than the plain jump. */
static inline int bm_rac2_running_jump(float stick, int strafing) {
    return stick > RAC2_RUN_JUMP_STICK && !strafing;
}

#endif /* RAC1_ENHANCED_MOVEMENT_GROUND_H */
