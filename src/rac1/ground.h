/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's running, turning and stopping, decided apart from game memory (DESIGN.md
 * section 2). game.cpp fills bm_run_in from the hero and applies what these return; the tests call
 * them with values worked out by hand.
 *
 * rac2's run (func_L00_002BA7D8 case 2 and 0x70), in its order:
 *   1. the flags along / turnaround from last tick's yaw error (bm_run_flags_update);
 *   2. the target from the stick (bm_run_speed);
 *   3. the turn: the run band's, or func_L00_002B3F68's with k from the yaw error before the turn
 *      (bm_run_turn, bm_run_k);
 *   4. the turnaround's cap and scale, the slow stick's cut, from the yaw error after the turn;
 *      the speed step (bm_run_speed);
 *   5. the velocity along the move yaw while `along`, else along the facing.
 */
#ifndef RAC1_BETTER_MOVEMENT_GROUND_H
#define RAC1_BETTER_MOVEMENT_GROUND_H

#include "rac1/angle.h"
#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* What the run's decisions read of the hero. */
typedef struct bm_run_in {
    float stick;       /* stick amount, 0 to 1 (+0x229C) */
    float yaw_error;   /* move yaw less facing, signed (+0x188) */
    float flat_speed;  /* this tick's units (+0x164) */
    int step;          /* state.step: 1 is the run band (+0x2088) */
    int wading;        /* in state 0x73 */
    int item_flag;     /* unk20AA, set while an item's animation locks the turn (+0x20AA) */
    int firing;        /* a hold-to-fire weapon in hand with its animation on */
    float stick_swing; /* the stick's angle 1 poll back against 7 back (the pad's hist_ang) */
    int strafing;
} bm_run_in;

/* rac2's +0x40C (move along the stick) and +0x40F (turnaround), kept by the mod. */
typedef struct bm_run_flags {
    int along;
    int turnaround;
} bm_run_flags;

/* A turn spring: Hero_TurnToHeading's accel, damping and limit, in this tick's units. */
typedef struct bm_turn {
    float accel;
    float damp;
    float limit;
} bm_turn;

/* A speed step: the target speed (this tick's units) and Hero_ApproachSpeed's up and down. */
typedef struct bm_speed {
    float target;
    float up;
    float down;
} bm_speed;

/* Step 1: the flags from last tick's yaw error, before the run's tick (rac2 works them out first). */
static inline void bm_run_flags_update(bm_run_flags* f, const bm_run_in* in, const bm_tick* t) {
    const float err = fabsf(in->yaw_error);

    // rac2 leaves both flags alone while strafing (its strafe zeroes the yaw error instead).
    if (!in->strafing) {
        // Slow: move along the stick; a stick fully the other way is a turnaround.
        if (in->flat_speed < rac2_speed(RAC2_ALONG_SPEED, t)) {
            f->along = 1;

            // A full stick more than 120 degrees off.
            if (err > RAC2_TURNAROUND_SLOW_ANGLE && in->stick == 1.0f) {
                f->turnaround = 1;
            }
        }

        // The stick swung more than 110 degrees in six polls, at full stick, 70 degrees off.
        if (in->stick_swing > RAC2_TURNAROUND_FLICK_ANGLE && err > RAC2_TURNAROUND_FLICK_ERROR
            && in->stick == 1.0f) {
            f->turnaround = 1;
            f->along = 1;
        }

        // Facing the stick within 5 degrees: back to running along the facing.
        if (err < RAC2_ALONG_CLEAR) {
            f->along = 0;
        }
    }

    // The turnaround ends within 4 degrees.
    if (f->turnaround && err < RAC2_TURNAROUND_CLEAR) {
        f->turnaround = 0;
    }
}

/* Whether the turn is the run band's (rac2's first branch), which leaves k at 1. */
static inline int bm_run_band_turn(const bm_run_in* in, const bm_run_flags* f) {
    return in->step == 1 && !f->turnaround && !in->item_flag;
}

/* Step 3: the turn, not strafing. */
static inline bm_turn bm_run_turn(const bm_run_in* in, const bm_run_flags* f, const bm_tick* t) {
    const float s = in->stick;
    float a;
    float d = RAC2_WALK_TURN_DAMP;
    float l;
    bm_turn out;

    // The run band turns on its own spring.
    if (bm_run_band_turn(in, f)) {
        out.accel = rac2_spring(RAC2_RUNBAND_TURN_ACCEL, t);
        out.damp = rac2_spring(RAC2_RUNBAND_TURN_DAMP, t);
        out.limit = rac2_turn_limit(RAC2_RUNBAND_TURN_LIMIT, t);
        return out;
    }

    // func_L00_002B3F68: accel and limit grow with the stick, full at 1.
    if (s == 1.0f) {
        a = RAC2_WALK_TURN_ACCEL_MAX;
        l = RAC2_WALK_TURN_LIMIT_MAX;
    } else {
        a = bm_clamp(
            s * RAC2_WALK_TURN_ACCEL_PER_STICK, RAC2_WALK_TURN_ACCEL_MIN, RAC2_WALK_TURN_ACCEL_MAX
        );
        l = bm_clamp(
            s * RAC2_WALK_TURN_LIMIT_PER_STICK, RAC2_WALK_TURN_LIMIT_MIN, RAC2_WALK_TURN_LIMIT_MAX
        );
    }

    // The turnaround turns harder.
    if (f->turnaround) {
        a = RAC2_TURNAROUND_TURN_ACCEL;
        l = RAC2_TURNAROUND_TURN_LIMIT;
    }

    // Firing a hold-to-fire weapon slows the turn (not in a turnaround).
    if (in->firing && !f->turnaround) {
        // Full stick has its own spring; less scales with the stick.
        if (s > RAC2_FIRING_FULL_STICK) {
            a = RAC2_FIRING_ACCEL;
            d = RAC2_FIRING_DAMP;
            l = RAC2_FIRING_LIMIT;
        } else {
            a = s * RAC2_FIRING_ACCEL_PER_STICK;
            d = RAC2_FIRING_DAMP_SLOW;
            l = s * RAC2_FIRING_LIMIT_PER_STICK;
        }
    }

    out.accel = rac2_spring(a, t);
    out.damp = rac2_spring(d, t);
    out.limit = rac2_turn_limit(l, t);
    return out;
}

/* Step 3: k, the share of the acceleration, from the yaw error before the turn. No speeding up
 * while turning more than 45 degrees with a part stick or an item's lock, except in the run band. */
static inline float bm_run_k(const bm_run_in* in, const bm_run_flags* f) {
    // The run band never stops speeding up.
    if (bm_run_band_turn(in, f)) {
        return 1.0f;
    }

    // Turning hard with a part stick, or with an item's lock outside a turnaround.
    if ((in->stick < RAC2_K_STICK || (in->item_flag && !f->turnaround))
        && fabsf(in->yaw_error) > RAC2_K_ANGLE) {
        return 0.0f;
    }

    return 1.0f;
}

/* Steps 2 and 4: the target, up and down, from the yaw error after the turn (in->yaw_error) and k.
 * While strafing rac2's yaw error is 0, so the caps do nothing; game.cpp passes 0 then. */
static inline bm_speed bm_run_speed(
    const bm_run_in* in, const bm_run_flags* f, float k, const bm_tick* t
) {
    const float err = fabsf(in->yaw_error);
    bm_speed out;

    // A stick under 0.2 asks for no speed; else the run's speed times the stick.
    if (in->stick < RAC2_STICK_RELEASE) {
        out.target = 0.0f;
    } else {
        out.target = RAC2_RUN_SPEED * in->stick * t->ft;

        // Wading: slower, but never under 2.5 units a second.
        if (in->wading) {
            out.target *= RAC2_WADING_SCALE;

            // The wading floor.
            if (out.target < rac2_speed(RAC2_WADING_MIN, t)) {
                out.target = rac2_speed(RAC2_WADING_MIN, t);
            }
        }
    }

    out.up = k * rac2_accel(RAC2_RUN_ACCEL, t);
    out.down = rac2_accel(RAC2_RUN_BRAKE, t);

    // The turnaround: capped by how far off he faces, then scaled by the signed error, braking at 17.
    if (f->turnaround) {
        float cap =
            rac2_speed(RAC2_TURNAROUND_CAP, t) - err * rac2_speed(RAC2_TURNAROUND_CAP_SLOPE, t);

        // The cap never goes under 0.
        if (cap < 0.0f) {
            cap = 0.0f;
        }

        // Only ever lower.
        if (cap < out.target) {
            out.target = cap;
        }

        out.target *= bm_clamp(RAC2_TURNAROUND_SCALE_BASE - in->yaw_error, 0.2f, 1.0f);
        out.down = rac2_accel(RAC2_TURNAROUND_BRAKE, t);
    }

    // A part stick far off the facing slows him.
    if (in->stick < RAC2_SLOW_STICK && err > RAC2_SLOW_ANGLE) {
        float m = 1.0f - err * RAC2_SLOW_FACTOR;
        out.target *= m < 0.0f ? 0.0f : m;
    }

    return out;
}

/* rac1's own `down` in the run (8.5, 17 with its turnaround flag): another mod's scale on the call
 * is what came in over this. */
static inline float bm_rac1_run_down(int rac1_turnaround, const bm_tick* t) {
    return (rac1_turnaround ? RAC1_TURNAROUND_BRAKE : RAC1_RUN_BRAKE) * t->ft2;
}

/* Whether a state is in the idle group whose braking is 12.6 in rac1 and 17.7 in rac2. */
static inline int bm_is_idle_group(int state) {
    // hero_update_movement's idle group, but its dying states.
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

/* How much harder rac2 brakes than rac1 in the idle group, STOP and the crouch: the factor for
 * Hero_ApproachSpeed's `down` there (1: the same, or a state the mod leaves alone). */
static inline float bm_brake_scale(int state, int on_ice) {
    // STOP: 27 * t against 12 * t.
    if (state == RAC1_STATE_STOP) {
        return RAC2_STOP_BRAKE * 3600.0f / RAC1_STOP_BRAKE;
    }

    // The crouch: 27 against 6.48; on ice both 1.7.
    if (state == RAC1_STATE_CROUCH) {
        return on_ice ? 1.0f : RAC2_CROUCH_BRAKE * 3600.0f / RAC1_CROUCH_BRAKE;
    }

    // Idle, look and the item waits: 17.7 against 12.6.
    if (bm_is_idle_group(state)) {
        return RAC2_IDLE_BRAKE * 3600.0f / RAC1_IDLE_BRAKE;
    }

    return 1.0f;
}

/* Whether rac2 leaves the run for idle now: the stick under 0.17, and not still turning to the
 * camera while strafing (func_L00_002B3268(2 degrees)). */
static inline int bm_run_exit(float stick, int strafing, float facing_off_camera) {
    return stick < RAC2_EXIT_STICK && !(strafing && facing_off_camera >= RAC2_EXIT_TURNED);
}

/* The momentum and ground speed rac2 leaves him on entering idle from the run: 1.5 units a second. */
static inline float bm_exit_clamp(const bm_tick* t) {
    return rac2_speed(RAC2_EXIT_CLAMP, t);
}

/* Whether X in the run gives rac2's running jump (else the plain jump). */
static inline int bm_rac2_running_jump(float stick, int strafing) {
    return stick > RAC2_RUN_JUMP_STICK && !strafing;
}

#endif /* RAC1_BETTER_MOVEMENT_GROUND_H */
