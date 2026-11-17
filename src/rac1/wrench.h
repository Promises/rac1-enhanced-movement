/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The wrench decisions (DESIGN.md section 5): Going Commando's wrench timing, apart from game
 * memory. Written for this mod from rac2-decomp's func_L00_002C9448 (which press starts or chains a
 * swing), func_L00_002CB3A8 cases 0x13 and 0x15 (how a swing and the throw end) and
 * func_L00_002BA7D8 cases 0x13 to 0x15 (the lock-on turn and the slam's drive). rac1-decomp's
 * process_hero_equipped_item_input and hero_update_transitions give what rac1 does in that tick.
 *
 * The mod never starts a swing itself. Each tick, before rac1's transitions run, it works out what
 * rac2 would do (bm_swing_rac2) and what rac1 will do (bm_swing_rac1). Where they differ,
 * bm_swing_steer_for picks a value to set for the transitions (the animation frame, or the blend
 * flag that holds the swing) so that rac1's code in the loaded level does what rac2 does. Damage,
 * hit windows and enemies stay rac1's.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_WRENCH_H
#define RAC1_ENHANCED_MOVEMENT_WRENCH_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* The frames of a swing's attack record (+0xC, +0x10, +0x14, +0x18). */
typedef struct bm_attack {
    int first; /* windows are (frame - first) * 2 polls */
    int open;  /* Square chains the next swing from this frame on */
    int jump;  /* X cancels into a jump after this frame */
    int exit;  /* the swing ends after this frame */
} bm_attack;

/* The number of attack records the mod steers: the combo's three swings and the throw. */
#define BM_ATTACK_RECORDS 4

/* Returns rac1's attack record (rac1.h), or all zeros for a record the mod does not steer. */
static inline bm_attack bm_rac1_attack(int record) {
    bm_attack a = {0, 0, 0, 0};

    // rac1's attack_records are the same in every level.
    switch (record) {
        case 0: /* first swing */
            a.first = RAC1_REC0_FIRST;
            a.open = RAC1_REC0_OPEN;
            a.jump = RAC1_REC0_JUMP;
            a.exit = RAC1_REC0_EXIT;
            break;
        case 1: /* second swing */
            a.first = RAC1_REC1_FIRST;
            a.open = RAC1_REC1_OPEN;
            a.jump = RAC1_REC1_JUMP;
            a.exit = RAC1_REC1_EXIT;
            break;
        case 2: /* third swing */
            a.first = RAC1_REC2_FIRST;
            a.open = RAC1_REC2_OPEN;
            a.jump = RAC1_REC2_JUMP;
            a.exit = RAC1_REC2_EXIT;
            break;
        case 3: /* the throw */
            a.first = RAC1_REC3_FIRST;
            a.open = RAC1_REC3_OPEN;
            a.jump = RAC1_REC3_JUMP;
            a.exit = RAC1_REC3_EXIT;
            break;
        default: /* not steered */
            break;
    }

    return a;
}

/* Returns rac2's attack record (rac2.h, from level 0's data), or all zeros for a record the mod
 * does not steer. */
static inline bm_attack bm_rac2_attack(int record) {
    bm_attack a = {0, 0, 0, 0};

    // rac2's attack records, D_L00_0025D990.
    switch (record) {
        case 0: /* first swing */
            a.first = RAC2_REC0_FIRST;
            a.open = RAC2_REC0_OPEN;
            a.jump = RAC2_REC0_JUMP;
            a.exit = RAC2_REC0_EXIT;
            break;
        case 1: /* second swing */
            a.first = RAC2_REC1_FIRST;
            a.open = RAC2_REC1_OPEN;
            a.jump = RAC2_REC1_JUMP;
            a.exit = RAC2_REC1_EXIT;
            break;
        case 2: /* third swing */
            a.first = RAC2_REC2_FIRST;
            a.open = RAC2_REC2_OPEN;
            a.jump = RAC2_REC2_JUMP;
            a.exit = RAC2_REC2_EXIT;
            break;
        case 3: /* the throw */
            a.first = RAC2_REC3_FIRST;
            a.open = RAC2_REC3_OPEN;
            a.jump = RAC2_REC3_JUMP;
            a.exit = RAC2_REC3_EXIT;
            break;
        default: /* not steered */
            break;
    }

    return a;
}

/* The swing and pad values the transitions read in state 0x13 or 0x15. */
typedef struct bm_swing_in {
    int state;    /* 0x13 the combo, 0x15 the throw */
    int record;   /* attack_record: 0 to 2 the combo's swings, 3 the throw */
    float frame;  /* anim_frame */
    int changing; /* anim_changing: a new animation is blending in */
    int done;     /* anim_flags & 2: the animation ended this tick */
    float stick;  /* stick amount */
    int timer;    /* state_timer */
    int square;   /* polls since Square was last pressed (1 newest), 0 if not in the history */
    int cross;    /* the same for X */
    int crouch;   /* R1 held, or R2, which rac1 reads with it */
    int unk308;   /* magnet-ground mode: no jump out of the throw while set */
    int slip;     /* on slip ground: no Square */
    int out;      /* the wrench is thrown and not back: rac2 takes no Square */
} bm_swing_in;

/* What a tick's transitions do with a swing. */
enum {
    BM_SWING_STAY,  /* the swing goes on */
    BM_SWING_CHAIN, /* Square: the next swing, or the throw again with R1 */
    BM_SWING_JUMP,  /* X: a jump */
    BM_SWING_RUN,   /* the run */
    BM_SWING_IDLE   /* standing */
};

/* Returns whether a press `age` polls back falls within a window of `n` polls, as
 * pad_pressed_within tests it. */
static inline int bm_within(int age, int n) {
    return age > 0 && age <= n;
}

/* Returns a swing's window in polls: the truncated frame less the record's first frame, times 2. */
static inline int bm_swing_window(float frame, int first) {
    return ((int)frame - first) * 2;
}

/* Returns what rac1 does with the swing this tick: the attack branch of
 * process_hero_equipped_item_input, then cases 0x13 and 0x15 of hero_update_transitions. X with R1
 * held is rac1's crouch jump, which counts as a jump. */
static inline int bm_swing_rac1(const bm_swing_in* in, const bm_tick* t) {
    const bm_attack a = bm_rac1_attack(in->record);
    const int chain = bm_scale_ticks(RAC1_WRENCH_CHAIN_WINDOW, t);

    // The throw again: after 30 ticks, from the record's open frame, R1 held, Square within 15.
    if (in->state == RAC1_STATE_CROUCH_SWING
        && in->timer > bm_scale_ticks(RAC1_THROW_AGAIN_TICKS, t) && !in->changing
        && in->frame >= (float)a.open && in->crouch && bm_within(in->square, chain)) {
        return BM_SWING_CHAIN;
    }

    // The next swing: from the open frame, off slip ground, Square within (frame - first) * 2
    // polls, kept to 1 to 15.
    if (!in->changing && in->frame >= (float)a.open && !in->slip) {
        int n = bm_swing_window(in->frame, a.first);

        n = n < 1 ? 1 : (n > chain ? chain : n);

        // Square came within the window.
        if (bm_within(in->square, n)) {
            return BM_SWING_CHAIN;
        }
    }

    // A jump: after the jump frame, X within (frame - first) * 2 polls, but not in the throw on
    // magnet ground.
    if ((float)a.jump < in->frame && !in->changing
        && !(in->state == RAC1_STATE_CROUCH_SWING && in->unk308 != 0)
        && bm_within(in->cross, bm_swing_window(in->frame, a.first))) {
        return BM_SWING_JUMP;
    }

    // Past the exit frame he stands at once.
    if (!in->changing && (float)a.exit < in->frame) {
        return BM_SWING_IDLE;
    }

    return BM_SWING_STAY;
}

/* Returns what rac2 does with the swing this tick: the attack branch of func_L00_002C9448, which
 * takes nothing while the wrench is out or on slip ground, then cases 0x13 and 0x15 of
 * func_L00_002CB3A8. Its counts are scaled to this tick as rac2's own release scales them
 * (bm_rac2_ticks). */
static inline int bm_swing_rac2(const bm_swing_in* in, const bm_tick* t) {
    const bm_attack a = bm_rac2_attack(in->record);
    const int chain = bm_rac2_ticks(RAC2_CHAIN_WINDOW, t);

    // Square passes the same tests as in rac1, on rac2's frames.
    if (!in->out && !in->slip && !in->changing && in->frame >= (float)a.open) {
        int n = bm_swing_window(in->frame, a.first);

        // The throw again: from its 31st tick, R1 held, Square within the chain window.
        if (in->state == RAC1_STATE_CROUCH_SWING
            && in->timer >= bm_rac2_ticks(RAC2_THROW_AGAIN_TICKS, t) && in->crouch
            && bm_within(in->square, chain)) {
            return BM_SWING_CHAIN;
        }

        n = n < 1 ? 1 : (n > chain ? chain : n);

        // The next swing: Square within the swing's window.
        if (bm_within(in->square, n)) {
            return BM_SWING_CHAIN;
        }
    }

    // A jump after the jump frame: in the throw X counts within 8 polls (past frame 35), in the
    // combo within the swing's window, as in rac1.
    if ((float)a.jump < in->frame && !in->changing
        && !(in->state == RAC1_STATE_CROUCH_SWING && in->unk308 != 0)) {
        const int n = in->state == RAC1_STATE_CROUCH_SWING
                          ? bm_rac2_ticks(RAC2_THROW_JUMP_WINDOW, t)
                          : bm_swing_window(in->frame, a.first);

        // X came within the window.
        if (bm_within(in->cross, n)) {
            return BM_SWING_JUMP;
        }
    }

    // While a new animation blends in, rac2 neither runs nor stands.
    if (in->changing) {
        return BM_SWING_STAY;
    }

    // Past frame 34, a stick past 0.9 runs out.
    if (in->frame > RAC2_THROW_RUN_FRAME && in->stick > RAC2_THROW_RUN_STICK) {
        return BM_SWING_RUN;
    }

    // Past the exit frame he runs with the stick past 0.8, or else stands 5 frames later or when
    // the animation ends.
    if ((float)a.exit < in->frame) {
        // A stick past 0.8 runs out.
        if (in->stick > RAC2_RUN_OUT_STICK) {
            return BM_SWING_RUN;
        }

        // Otherwise he stands.
        if ((float)(a.exit + RAC2_EXIT_IDLE_FRAMES) < in->frame || in->done) {
            return BM_SWING_IDLE;
        }
    }

    return BM_SWING_STAY;
}

/* How the mod steers rac1's transitions for one tick. */
enum {
    BM_STEER_NONE,  /* no change */
    BM_STEER_FRAME, /* anim_frame set to `frame` */
    BM_STEER_HOLD   /* anim_changing set: rac1 leaves the swing alone this tick */
};

/* A tick's steer: what rac2 and rac1 do, how to make rac1 do what rac2 does, and which presses to
 * hide from the transitions (Square in the newest `hide_square` polls, X in the newest
 * `hide_cross`) so that the steered frame changes nothing else. */
typedef struct bm_swing_steer {
    int rac2;        /* BM_SWING_* rac2 does */
    int rac1;        /* BM_SWING_* rac1 would do */
    int kind;        /* BM_STEER_* */
    float frame;     /* the frame for BM_STEER_FRAME */
    int hide_square; /* polls of Square to hide */
    int hide_cross;  /* polls of X to hide */
} bm_swing_steer;

/* Returns the steer for a swing this tick: BM_STEER_NONE where rac1 already does what rac2 does, or
 * cannot be led to it. rac2's run out of a swing is rac1's idle, which starts the run on the next
 * tick once the stick is past 0.22. */
static inline bm_swing_steer bm_swing_steer_for(const bm_swing_in* in, const bm_tick* t) {
    const bm_attack a = bm_rac1_attack(in->record);
    bm_swing_steer s = {BM_SWING_STAY, BM_SWING_STAY, BM_STEER_NONE, 0.0f, 0, 0};
    // The input as rac1's transitions would see it after a steer.
    bm_swing_in probe = *in;

    // Only the combo and the throw are steered, and only on the records the mod knows.
    if ((in->state != RAC1_STATE_WRENCH_COMBO && in->state != RAC1_STATE_CROUCH_SWING)
        || in->record < 0 || in->record >= BM_ATTACK_RECORDS) {
        return s;
    }

    s.rac2 = bm_swing_rac2(in, t);
    s.rac1 = bm_swing_rac1(in, t);

    // Nothing to do when both agree, or when rac1's idle leads to rac2's run on the next tick.
    if (s.rac2 == s.rac1 || (s.rac2 == BM_SWING_RUN && s.rac1 == BM_SWING_IDLE)) {
        return s;
    }

    // For an earlier chain, show rac1 the frame its own chain opens at.
    if (s.rac2 == BM_SWING_CHAIN) {
        probe.frame = (float)a.open;

        // Only a later frame, where rac1's window takes the same press.
        if (probe.frame > in->frame && bm_swing_rac1(&probe, t) == BM_SWING_CHAIN) {
            s.kind = BM_STEER_FRAME;
            s.frame = probe.frame;
        }

        return s;
    }

    // For an earlier jump, show rac1 the first frame past its jump frame whose window takes the
    // press, with Square hidden so that the frame chains nothing.
    if (s.rac2 == BM_SWING_JUMP) {
        int frame = a.jump + 1;

        // The window, (frame - first) * 2 polls, must reach back to the press.
        if (frame < a.first + (in->cross + 1) / 2) {
            frame = a.first + (in->cross + 1) / 2;
        }

        probe.frame = (float)frame;
        probe.square = 0;

        // Steer only if rac1 then takes the jump.
        if (bm_swing_rac1(&probe, t) == BM_SWING_JUMP) {
            s.kind = BM_STEER_FRAME;
            s.frame = probe.frame;
            s.hide_square = bm_scale_ticks(RAC1_WRENCH_CHAIN_WINDOW, t);
        }

        return s;
    }

    // For an earlier run or stand, show rac1 a frame half a frame past its exit, unless already
    // past it, with Square and X hidden.
    if (s.rac2 == BM_SWING_RUN || s.rac2 == BM_SWING_IDLE) {
        probe.frame = in->frame > (float)a.exit ? in->frame : (float)a.exit + 0.5f;
        probe.square = 0;
        probe.cross = 0;

        // Steer only if rac1 then stands.
        if (bm_swing_rac1(&probe, t) == BM_SWING_IDLE) {
            s.kind = BM_STEER_FRAME;
            s.frame = probe.frame;
            s.hide_square = bm_scale_ticks(RAC1_WRENCH_CHAIN_WINDOW, t);
            s.hide_cross = 30; /* the pad's whole history */
        }

        return s;
    }

    // rac2 stays in the swing where rac1 would leave it, so set the blend flag, which makes rac1
    // wait.
    probe.changing = 1;

    // Steer only if the flag keeps rac1 in the swing.
    if (bm_swing_rac1(&probe, t) == BM_SWING_STAY) {
        s.kind = BM_STEER_HOLD;
    }

    return s;
}

/* ---- Square outside a swing ---- */

/* The hero values that Square's window for a first swing or the slam reads
 * (process_hero_equipped_item_input). */
typedef struct bm_reach_in {
    int state;
    int control_mode;
    int timer;     /* state_timer */
    int rail_land; /* rail_land_frames: a landing on a rail counts as on foot */
    int unk450;    /* 3: a high jump whose Square comes 2 frames later */
    float frame;   /* anim_frame */
    int slip;      /* on slip ground */
} bm_reach_in;

/* Square's window in polls in each game. */
typedef struct bm_windows {
    int rac1;
    int rac2;
} bm_windows;

/* Returns whether rac1 takes Square in this airborne state: the jumps, landing and falling, the
 * double jump past frame 32, and the high jump past frame 18 (20 when unk450 is 3). */
static inline int bm_wrench_air_state(const bm_reach_in* in) {
    // The double jump takes Square only past frame 32.
    if (in->state == RAC1_STATE_DOUBLE_JUMP) {
        return in->frame > 32.0f;
    }

    // The high jump (the crouch jump and the strafe flips) takes it past frame 18, or 20 when
    // unk450 is 3.
    if (in->state == RAC1_STATE_HIGH_JUMP) {
        return in->unk450 != 3 ? in->frame > 18.0f : in->frame > 20.0f;
    }

    return in->state == RAC1_STATE_JUMP || in->state == RAC1_STATE_RUN_JUMP
           || in->state == RAC1_STATE_LAND || in->state == RAC1_STATE_FALL
           || in->state == RAC1_STATE_ITEM_RELEASE_FALL || in->state == 0x11;
}

/* Returns Square's windows for a first swing or the slam now, or {0, 0} where neither game takes
 * Square. On foot rac1 allows 7 polls and rac2 10. In a jump after its 10th tick rac1 allows 10 and
 * rac2 14, and both 18 in the high and double jumps. rac1's are counted by scale_ticks and rac2's
 * by bm_rac2_ticks, so at 50 Hz both games give 8 and 12, and rac2's late window is shorter:
 * nothing is buffered there. */
static inline bm_windows bm_wrench_windows(const bm_reach_in* in, const bm_tick* t) {
    bm_windows w = {0, 0};

    // Standing or running, or landing on a rail.
    if (in->control_mode < 2 || (in->control_mode == RAC1_MODE_JUMP && in->rail_land != 0)) {
        // Slip ground takes no Square.
        if (!in->slip) {
            w.rac1 = bm_scale_ticks(RAC1_WRENCH_WINDOW, t);
            w.rac2 = bm_rac2_ticks(RAC2_WRENCH_WINDOW, t);
        }

        return w;
    }

    // In the air, after the jump's 10th tick. The high and double jumps use the late window.
    if (bm_wrench_air_state(in) && in->timer > bm_scale_ticks(RAC1_WRENCH_AIR_TICKS, t)) {
        const int late = in->state == RAC1_STATE_HIGH_JUMP || in->state == RAC1_STATE_DOUBLE_JUMP;

        w.rac1 = bm_scale_ticks(late ? RAC1_WRENCH_AIR_LATE_WINDOW : RAC1_WRENCH_AIR_WINDOW, t);
        w.rac2 = bm_rac2_ticks(late ? RAC2_WRENCH_AIR_LATE_WINDOW : RAC2_WRENCH_AIR_WINDOW, t);
    }

    return w;
}

/* Returns whether a Square `age` polls back is inside rac2's window but outside rac1's. The mod
 * then shows it to rac1 as pressed this poll. */
static inline int bm_wrench_buffer(bm_windows w, int age) {
    return bm_within(age, w.rac2) && !bm_within(age, w.rac1);
}

/* ---- Lock-on turn and slam ---- */

/* A turn spring's arguments. */
typedef struct bm_spring {
    float accel;
    float damp;
    float limit;
} bm_spring;

/* Replaces rac1's lock-on turn, turn_to(0.05, 0.2, 15.01) in the swings, the slam and the throw,
 * with rac2's. Returns 1 and the new arguments in `out` when the call is that turn, or 0 for any
 * other call (the ice's, or an unknown one). Each argument is scaled by rac2's value over rac1's:
 * the limit 16.93 rad/s for 15.01, and the throw's accel 0.057 for 0.05. */
static inline int bm_wrench_turn(int state, bm_spring in, const bm_tick* t, bm_spring* out) {
    const float limit = RAC1_WRENCH_TURN_LIMIT * t->ft;
    const float damp = RAC1_WRENCH_TURN_DAMP * t->fs2;
    const float accel = RAC1_WRENCH_TURN_ACCEL * t->fs2;

    // Only rac1's lock-on turn is replaced: all three of its arguments, each within 1%.
    if (fabsf(in.limit - limit) > limit * 0.01f || fabsf(in.damp - damp) > damp * 0.01f
        || fabsf(in.accel - accel) > accel * 0.01f) {
        return 0;
    }

    out->accel =
        in.accel
        * (rac2_spring(
               state == RAC1_STATE_CROUCH_SWING ? RAC2_THROW_TURN_ACCEL : RAC2_WRENCH_TURN_ACCEL, t
           )
           / accel);
    out->damp = in.damp * (rac2_spring(RAC2_WRENCH_TURN_DAMP, t) / damp);
    out->limit = in.limit * (rac2_turn_limit(RAC2_WRENCH_TURN_LIMIT, t) / limit);
    return 1;
}

/* Returns whether rac2's slam drives forward at 4.5 u/s now (func_L00_002BA7D8 case 0x14, off ice):
 * more than 4 units above the ground, and either toward a target or within its first 70 ticks. */
static inline int bm_slam_drive(float ground_distance, int target, int timer, const bm_tick* t) {
    return ground_distance > RAC2_SLAM_DRIVE_HEIGHT
           && (target || timer < bm_rac2_ticks(RAC2_SLAM_DRIVE_TICKS, t));
}

/* Returns the slam's drive speed in this tick's units. */
static inline float bm_slam_drive_speed(const bm_tick* t) {
    return rac2_speed(RAC2_SLAM_DRIVE, t);
}

#endif /* RAC1_ENHANCED_MOVEMENT_WRENCH_H */
