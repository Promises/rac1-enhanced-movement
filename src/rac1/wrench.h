/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's wrench, decided apart from game memory (DESIGN.md section 5). Written for this mod
 * from rac2-decomp: func_L00_002C9448 (which button press starts or chains a swing), func_L00_002CB3A8
 * cases 0x13 and 0x15 (how a swing and the throw end), func_L00_002BA7D8 cases 0x13 to 0x15 (the
 * lock-on turn, the slam's drive), and from rac1-decomp's process_hero_equipped_item_input and
 * hero_update_transitions for what rac1's own code does in the same tick.
 *
 * The mod never starts a swing itself. Each tick, before rac1's transitions, it works out what rac2's
 * input and decider would do (bm_swing_rac2) and what rac1's will do (bm_swing_rac1). Where they
 * differ, bm_swing_steer says which value to set for the transitions (the animation's frame, or the
 * blend flag that holds the swing), so that rac1's own code (the loaded level's copy) does what rac2
 * does. Damage, hit windows and enemies stay rac1's.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_WRENCH_H
#define RAC1_ENHANCED_MOVEMENT_WRENCH_H

#include <math.h>

#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* A swing's frames (attack_records' +0xC, +0x10, +0x14, +0x18). */
typedef struct bm_attack {
    int first; /* X's and Square's windows are (frame - first) * 2 polls */
    int open;  /* Square chains the next swing from this frame on */
    int jump;  /* X cancels into a jump after this frame */
    int exit;  /* the swing ends after this frame */
} bm_attack;

/* The records the mod steers: the combo's three swings and the throw. */
#define BM_ATTACK_RECORDS 4

/* rac1's record (rac1.h); a record the mod does not steer is all zeros. */
static inline bm_attack bm_rac1_attack(int record) {
    bm_attack a = {0, 0, 0, 0};

    // attack_records, the same in every level.
    switch (record) {
        case 0:
            a.first = RAC1_REC0_FIRST;
            a.open = RAC1_REC0_OPEN;
            a.jump = RAC1_REC0_JUMP;
            a.exit = RAC1_REC0_EXIT;
            break;
        case 1:
            a.first = RAC1_REC1_FIRST;
            a.open = RAC1_REC1_OPEN;
            a.jump = RAC1_REC1_JUMP;
            a.exit = RAC1_REC1_EXIT;
            break;
        case 2:
            a.first = RAC1_REC2_FIRST;
            a.open = RAC1_REC2_OPEN;
            a.jump = RAC1_REC2_JUMP;
            a.exit = RAC1_REC2_EXIT;
            break;
        case 3:
            a.first = RAC1_REC3_FIRST;
            a.open = RAC1_REC3_OPEN;
            a.jump = RAC1_REC3_JUMP;
            a.exit = RAC1_REC3_EXIT;
            break;
        default:
            break;
    }

    return a;
}

/* rac2's record (rac2.h, level 0's data). */
static inline bm_attack bm_rac2_attack(int record) {
    bm_attack a = {0, 0, 0, 0};

    // D_L00_0025D990.
    switch (record) {
        case 0:
            a.first = RAC2_REC0_FIRST;
            a.open = RAC2_REC0_OPEN;
            a.jump = RAC2_REC0_JUMP;
            a.exit = RAC2_REC0_EXIT;
            break;
        case 1:
            a.first = RAC2_REC1_FIRST;
            a.open = RAC2_REC1_OPEN;
            a.jump = RAC2_REC1_JUMP;
            a.exit = RAC2_REC1_EXIT;
            break;
        case 2:
            a.first = RAC2_REC2_FIRST;
            a.open = RAC2_REC2_OPEN;
            a.jump = RAC2_REC2_JUMP;
            a.exit = RAC2_REC2_EXIT;
            break;
        case 3:
            a.first = RAC2_REC3_FIRST;
            a.open = RAC2_REC3_OPEN;
            a.jump = RAC2_REC3_JUMP;
            a.exit = RAC2_REC3_EXIT;
            break;
        default:
            break;
    }

    return a;
}

/* What the transitions read of a swing (state 0x13 or 0x15) and of the pad. */
typedef struct bm_swing_in {
    int state;    /* 0x13 the combo, 0x15 the throw */
    int record;   /* attack_record: 0 to 2 the combo's swings, 3 the throw */
    float frame;  /* anim_frame */
    int changing; /* anim_changing: a new animation is blending in */
    int done;     /* anim_flags & 2: the animation came to its end this tick */
    float stick;  /* stick amount */
    int timer;    /* state_timer */
    int square; /* how many polls back Square was pressed last (1 the newest), 0 not in the history */
    int cross;  /* the same for X */
    int crouch; /* R1 held (or R2, which rac1 reads with it) */
    int unk308; /* rac1's magnet-ground mode: no jump out of the throw while it is set */
    int slip;   /* on slip ground: no Square */
    int out;    /* the wrench is thrown and not back: rac2 takes no Square */
} bm_swing_in;

/* What a tick's transitions do with a swing. */
enum {
    BM_SWING_STAY,  /* nothing: the swing goes on */
    BM_SWING_CHAIN, /* Square: the next swing (or the throw again, with R1) */
    BM_SWING_JUMP,  /* X: a jump */
    BM_SWING_RUN,   /* the run */
    BM_SWING_IDLE   /* standing */
};

/* Whether a press `age` polls back counts in a window of `n` polls (pad_pressed_within). */
static inline int bm_within(int age, int n) {
    return age > 0 && age <= n;
}

/* A swing's window: (the frame, cut, less the record's first frame) twice. */
static inline int bm_swing_window(float frame, int first) {
    return ((int)frame - first) * 2;
}

/* What rac1 does with the swing this tick: the attack branch of process_hero_equipped_item_input,
 * then hero_update_transitions' case 0x13/0x15 (X with R1 held is its crouch jump, a jump too). */
static inline int bm_swing_rac1(const bm_swing_in* in, const bm_tick* t) {
    const bm_attack a = bm_rac1_attack(in->record);
    const int chain = bm_scale_ticks(RAC1_WRENCH_CHAIN_WINDOW, t);

    // The throw again: after 30 ticks, from the record's open frame, R1 held, Square within 15.
    if (in->state == RAC1_STATE_CROUCH_SWING
        && in->timer > bm_scale_ticks(RAC1_THROW_AGAIN_TICKS, t) && !in->changing
        && in->frame >= (float)a.open && in->crouch && bm_within(in->square, chain)) {
        return BM_SWING_CHAIN;
    }

    // The next swing: from the open frame, Square within (frame - first) * 2 polls, 1 to 15.
    if (!in->changing && in->frame >= (float)a.open && !in->slip) {
        int n = bm_swing_window(in->frame, a.first);

        n = n < 1 ? 1 : (n > chain ? chain : n);

        // Pressed in time.
        if (bm_within(in->square, n)) {
            return BM_SWING_CHAIN;
        }
    }

    // A jump: after the jump frame, X within (frame - first) * 2 polls; not in the throw on magnet
    // ground.
    if ((float)a.jump < in->frame && !in->changing
        && !(in->state == RAC1_STATE_CROUCH_SWING && in->unk308 != 0)
        && bm_within(in->cross, bm_swing_window(in->frame, a.first))) {
        return BM_SWING_JUMP;
    }

    // The end: standing, at once after the exit frame.
    if (!in->changing && (float)a.exit < in->frame) {
        return BM_SWING_IDLE;
    }

    return BM_SWING_STAY;
}

/* What rac2 does with the swing this tick: func_L00_002C9448's attack branch (nothing while the
 * wrench is out or on slip ground), then func_L00_002CB3A8's case 0x13/0x15. Its counts as its own
 * release counts them at this tick (bm_rac2_ticks). */
static inline int bm_swing_rac2(const bm_swing_in* in, const bm_tick* t) {
    const bm_attack a = bm_rac2_attack(in->record);
    const int chain = bm_rac2_ticks(RAC2_CHAIN_WINDOW, t);

    // Square: the same tests as rac1's, on rac2's frames.
    if (!in->out && !in->slip && !in->changing && in->frame >= (float)a.open) {
        int n = bm_swing_window(in->frame, a.first);

        // The throw again.
        if (in->state == RAC1_STATE_CROUCH_SWING
            && in->timer >= bm_rac2_ticks(RAC2_THROW_AGAIN_TICKS, t) && in->crouch
            && bm_within(in->square, chain)) {
            return BM_SWING_CHAIN;
        }

        n = n < 1 ? 1 : (n > chain ? chain : n);

        // The next swing.
        if (bm_within(in->square, n)) {
            return BM_SWING_CHAIN;
        }
    }

    // A jump: the throw's window is 8 polls from frame 35, the combo's as rac1's.
    if ((float)a.jump < in->frame && !in->changing
        && !(in->state == RAC1_STATE_CROUCH_SWING && in->unk308 != 0)) {
        const int n = in->state == RAC1_STATE_CROUCH_SWING
                          ? bm_rac2_ticks(RAC2_THROW_JUMP_WINDOW, t)
                          : bm_swing_window(in->frame, a.first);

        // Pressed in time.
        if (bm_within(in->cross, n)) {
            return BM_SWING_JUMP;
        }
    }

    // A new animation blending in holds the rest.
    if (in->changing) {
        return BM_SWING_STAY;
    }

    // Past frame 34, a full stick runs.
    if (in->frame > RAC2_THROW_RUN_FRAME && in->stick > RAC2_THROW_RUN_STICK) {
        return BM_SWING_RUN;
    }

    // Past the exit frame: the run with the stick past 0.8, else standing 5 frames later or when the
    // animation ends.
    if ((float)a.exit < in->frame) {
        // Running out.
        if (in->stick > RAC2_RUN_OUT_STICK) {
            return BM_SWING_RUN;
        }

        // Standing.
        if ((float)(a.exit + RAC2_EXIT_IDLE_FRAMES) < in->frame || in->done) {
            return BM_SWING_IDLE;
        }
    }

    return BM_SWING_STAY;
}

/* How the mod steers rac1's transitions for one tick. */
enum {
    BM_STEER_NONE,  /* rac1 does what rac2 does */
    BM_STEER_FRAME, /* anim_frame set to `frame` */
    BM_STEER_HOLD   /* anim_changing set: rac1 leaves the swing alone this tick */
};

/* The tick's steer: what rac2 and rac1 do, how to make rac1 do rac2's, and which presses the
 * transitions must not see (the newest `hide_square` polls of Square, `hide_cross` of X) so that the
 * steered frame changes nothing else. */
typedef struct bm_swing_steer {
    int rac2;
    int rac1;
    int kind;
    float frame;
    int hide_square;
    int hide_cross;
} bm_swing_steer;

/* The steer for a swing this tick (BM_STEER_NONE where rac1 already does rac2's, or cannot be led
 * to it). rac2's run is rac1's idle, then its run on the next tick (idle starts the run past 0.22
 * of stick). */
static inline bm_swing_steer bm_swing_steer_for(const bm_swing_in* in, const bm_tick* t) {
    const bm_attack a = bm_rac1_attack(in->record);
    bm_swing_steer s = {BM_SWING_STAY, BM_SWING_STAY, BM_STEER_NONE, 0.0f, 0, 0};
    bm_swing_in x = *in;

    // Only the swings and the throw, on the records the mod knows.
    if ((in->state != RAC1_STATE_WRENCH_COMBO && in->state != RAC1_STATE_CROUCH_SWING)
        || in->record < 0 || in->record >= BM_ATTACK_RECORDS) {
        return s;
    }

    s.rac2 = bm_swing_rac2(in, t);
    s.rac1 = bm_swing_rac1(in, t);

    // Already the same, or rac1's idle on the way to rac2's run.
    if (s.rac2 == s.rac1 || (s.rac2 == BM_SWING_RUN && s.rac1 == BM_SWING_IDLE)) {
        return s;
    }

    // The next swing earlier: the frame rac1's own chain opens at.
    if (s.rac2 == BM_SWING_CHAIN) {
        x.frame = (float)a.open;

        // Later than now, and rac1's window there takes the same press.
        if (x.frame > in->frame && bm_swing_rac1(&x, t) == BM_SWING_CHAIN) {
            s.kind = BM_STEER_FRAME;
            s.frame = x.frame;
        }

        return s;
    }

    // A jump earlier: the first frame past rac1's jump frame whose window takes the press, with
    // Square hidden so that the frame chains nothing.
    if (s.rac2 == BM_SWING_JUMP) {
        int f = a.jump + 1;

        // A window long enough for the press.
        if (f < a.first + (in->cross + 1) / 2) {
            f = a.first + (in->cross + 1) / 2;
        }

        x.frame = (float)f;
        x.square = 0;

        // rac1 then takes the jump.
        if (bm_swing_rac1(&x, t) == BM_SWING_JUMP) {
            s.kind = BM_STEER_FRAME;
            s.frame = x.frame;
            s.hide_square = bm_scale_ticks(RAC1_WRENCH_CHAIN_WINDOW, t);
        }

        return s;
    }

    // Running or standing earlier: just past rac1's exit frame, with Square and X hidden.
    if (s.rac2 == BM_SWING_RUN || s.rac2 == BM_SWING_IDLE) {
        x.frame = in->frame > (float)a.exit ? in->frame : (float)a.exit + 0.5f;
        x.square = 0;
        x.cross = 0;

        // rac1 then stands.
        if (bm_swing_rac1(&x, t) == BM_SWING_IDLE) {
            s.kind = BM_STEER_FRAME;
            s.frame = x.frame;
            s.hide_square = bm_scale_ticks(RAC1_WRENCH_CHAIN_WINDOW, t);
            s.hide_cross = 30; /* the pad's whole history */
        }

        return s;
    }

    // rac2 stays in the swing where rac1 would leave it: rac1 holds while an animation blends in.
    x.changing = 1;

    // Held.
    if (bm_swing_rac1(&x, t) == BM_SWING_STAY) {
        s.kind = BM_STEER_HOLD;
    }

    return s;
}

/* --- Square from standing, running and jumping ------------------------------------------------- */

/* What the first swing's or the slam's window reads (process_hero_equipped_item_input). */
typedef struct bm_reach_in {
    int state;
    int control_mode;
    int timer;     /* state_timer */
    int rail_land; /* rail_land_frames: a landing on a rail counts as on foot */
    int unk450;    /* 3: a high jump whose Square comes 2 frames later */
    float frame;   /* anim_frame */
    int slip;      /* on slip ground */
} bm_reach_in;

/* Square's window in polls, rac1's and rac2's. */
typedef struct bm_windows {
    int rac1;
    int rac2;
} bm_windows;

/* Whether rac1's wrench takes Square in this jump: the jumps, landing, falling, the double jump past
 * frame 32 and the high jump past 18 (20 in its unk450 3 form). */
static inline int bm_wrench_air_state(const bm_reach_in* in) {
    // The double and the high jump, late enough.
    if (in->state == RAC1_STATE_DOUBLE_JUMP) {
        return in->frame > 32.0f;
    }

    // The high jump (the crouch jump, and the strafe's flips).
    if (in->state == RAC1_STATE_HIGH_JUMP) {
        return in->unk450 != 3 ? in->frame > 18.0f : in->frame > 20.0f;
    }

    return in->state == RAC1_STATE_JUMP || in->state == RAC1_STATE_RUN_JUMP
           || in->state == RAC1_STATE_LAND || in->state == RAC1_STATE_FALL
           || in->state == RAC1_STATE_ITEM_RELEASE_FALL || in->state == 0x11;
}

/* Square's windows for a first swing or the slam now; {0, 0} where neither game takes Square. On
 * foot 7 polls (rac2 10); in a jump after its 10th tick 10 (rac2 14), both 18 in the high and double
 * jumps. rac1's counted by scale_ticks, rac2's by bm_rac2_ticks: at 50 Hz both games' windows are
 * then the same (8, 12) and rac2's late one shorter, so nothing is buffered there. */
static inline bm_windows bm_wrench_windows(const bm_reach_in* in, const bm_tick* t) {
    bm_windows w = {0, 0};

    // Standing or running, or just landed on a rail.
    if (in->control_mode < 2 || (in->control_mode == RAC1_MODE_JUMP && in->rail_land != 0)) {
        // Not on slip ground.
        if (!in->slip) {
            w.rac1 = bm_scale_ticks(RAC1_WRENCH_WINDOW, t);
            w.rac2 = bm_rac2_ticks(RAC2_WRENCH_WINDOW, t);
        }

        return w;
    }

    // In a jump, after its 10th tick.
    if (bm_wrench_air_state(in) && in->timer > bm_scale_ticks(RAC1_WRENCH_AIR_TICKS, t)) {
        const int late = in->state == RAC1_STATE_HIGH_JUMP || in->state == RAC1_STATE_DOUBLE_JUMP;

        w.rac1 = bm_scale_ticks(late ? RAC1_WRENCH_AIR_LATE_WINDOW : RAC1_WRENCH_AIR_WINDOW, t);
        w.rac2 = bm_rac2_ticks(late ? RAC2_WRENCH_AIR_LATE_WINDOW : RAC2_WRENCH_AIR_WINDOW, t);
    }

    return w;
}

/* Whether a Square `age` polls back is one rac2 takes and rac1 would not: the mod then shows it to
 * rac1 as pressed this poll. */
static inline int bm_wrench_buffer(bm_windows w, int age) {
    return bm_within(age, w.rac2) && !bm_within(age, w.rac1);
}

/* --- The lock-on turn and the slam ------------------------------------------------------------- */

/* A turn spring's arguments. */
typedef struct bm_spring {
    float accel;
    float damp;
    float limit;
} bm_spring;

/* rac2's lock-on turn for rac1's (turn_to(0.05, 0.2, 15.01) in the swings, the slam and the throw):
 * 1 and the new arguments when the call is that turn, 0 for any other (the ice's, an unknown one).
 * Each argument is what came in times rac2's over rac1's: 16.93 radians a second for 15.01, and the
 * throw's accel 0.057 for 0.05. */
static inline int bm_wrench_turn(int state, bm_spring in, const bm_tick* t, bm_spring* out) {
    const float limit = RAC1_WRENCH_TURN_LIMIT * t->ft;
    const float damp = RAC1_WRENCH_TURN_DAMP * t->fs2;
    const float accel = RAC1_WRENCH_TURN_ACCEL * t->fs2;

    // Only the lock-on turn: rac1's own numbers, within 1%.
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

/* Whether rac2's slam drives on at 4.5 units a second now (func_L00_002BA7D8 case 0x14, off ice):
 * more than 4 above the ground, toward a target, or in its first 70 ticks without one. */
static inline int bm_slam_drive(float ground_distance, int target, int timer, const bm_tick* t) {
    return ground_distance > RAC2_SLAM_DRIVE_HEIGHT
           && (target || timer < bm_rac2_ticks(RAC2_SLAM_DRIVE_TICKS, t));
}

/* The slam's drive speed in this tick's units. */
static inline float bm_slam_drive_speed(const bm_tick* t) {
    return rac2_speed(RAC2_SLAM_DRIVE, t);
}

#endif /* RAC1_ENHANCED_MOVEMENT_WRENCH_H */
