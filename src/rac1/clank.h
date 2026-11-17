/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The Clank decisions: his controls under the `clank` setting, apart from game memory (DESIGN.md
 * section 9, PLAN.md section 11). Written for this mod from rac1-decomp's hero_update_movement
 * (Clank's states 0x43 to 0x52), hero_update_transitions and HeroEnterMoveState, and from the same
 * states in rac2-decomp's func_L00_002BA7D8 (cases 0x3F to 0x4E), func_L00_002CB3A8 and
 * func_L00_002C42E0.
 *
 * In the first game Clank alone is a vehicle mode of the hero (+0x20A4 = 1) with states of his own.
 * They run through the same helpers as Ratchet's: his run turns with turn_to, sets its speed with
 * speed_step and moves with set_planar_vel, and his punch turns toward a target with the wrench's
 * turn_to. As for Ratchet, the mod changes the arguments in the hooks on those helpers, writes his
 * jump's entry values on its first tick, and steers his transitions for a tick (a Square shown as
 * pressed, or the punch's frame). It never calls a game function.
 *
 * The choices:
 * - rac1: the game's controls; the mod leaves him alone.
 * - rac2: Going Commando's Clank where it differs: acceleration and braking in the run, the turn
 *   and turnaround, moving along the stick, a quicker jump takeoff, and the punch's quicker lock-on
 *   turn and ways out (he stands after 22 ticks and jumps out after 18).
 * - enhanced: rac2's, and past it by the steps in clank_tune.h.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_CLANK_H
#define RAC1_ENHANCED_MOVEMENT_CLANK_H

#include <math.h>

#include "rac1/angle.h"
#include "rac1/clank_tune.h"
#include "rac1/ground.h"
#include "rac1/rac1.h"
#include "rac1/rac2.h"
#include "rac1/wrench.h"

/* The values of the `clank` setting. */
enum {
    BM_CLANK_RAC1 = 0,    /* the game's: the mod changes nothing of his */
    BM_CLANK_RAC2 = 1,    /* Going Commando's Clank */
    BM_CLANK_ENHANCED = 2 /* rac2's, taken further by clank_tune.h */
};

/* The numbers a choice plays with: speeds and turn limits per second (u/s, u/s^2, rad/s), turn
 * coefficients per tick at 60 Hz, counts at 60 Hz before the game scales them, and frames as
 * attack_records count them. */
typedef struct bm_clank_params {
    float top_speed;        /* the run at full stick (rac1 3.0) */
    float run_accel;        /* speeding up in the run (rac1 7.5 x k) */
    float run_brake;        /* slowing in the run (rac1 8.5) */
    float turn_accel;       /* run turn accel (rac1's depends on stick and speed) */
    float turn_damp;        /* run turn damping */
    float turn_limit;       /* run turn limit */
    float turnaround_accel; /* turnaround accel */
    float turnaround_limit; /* turnaround limit */
    float along_speed;      /* moves along the stick under this speed */
    int takeoff;            /* the jump leaves the ground after these ticks (rac1 5) */
    float apex_frame;       /* jump animation's apex frame (rac1 18) */
    float target_frame;     /* jump animation's landing frame (rac1 27) */
    float jump_air_speed;   /* air stick speed in his jump (rac1 2.0) */
    float punch_turn_limit; /* punch lock-on turn limit (rac1 15.01) */
    int square_window;      /* Square remembered standing or running (rac1 9) */
    int land_square_window; /* Square remembered landing (rac1 5) */
    float punch_lead;       /* frames ahead the punch's ways out are read (rac1 0) */
    float run_out_frame;    /* with the stick past run_out_stick he leaves past this frame */
    float run_out_stick;    /* 0: no run out */
} bm_clank_params;

/* Returns a choice's numbers. BM_CLANK_RAC1 and any value it does not know get rac1's, which the
 * mod leaves as the game has them. */
static inline bm_clank_params bm_clank_params_for(int choice) {
    bm_clank_params p;

    // Going Commando's Clank from rac2.h, its per-tick numbers made per second (x 60, x 3600).
    if (choice == BM_CLANK_RAC2) {
        p.top_speed = RAC2_CLANK_RUN_SPEED * 60.0f;
        p.run_accel = RAC2_CLANK_RUN_ACCEL * 3600.0f;
        p.run_brake = RAC2_CLANK_RUN_BRAKE * 3600.0f;
        p.turn_accel = RAC2_CLANK_TURN_ACCEL;
        p.turn_damp = RAC2_CLANK_TURN_DAMP;
        p.turn_limit = RAC2_CLANK_TURN_LIMIT * 60.0f;
        p.turnaround_accel = RAC2_CLANK_TURNAROUND_ACCEL;
        p.turnaround_limit = RAC2_CLANK_TURNAROUND_LIMIT * 60.0f;
        p.along_speed = RAC2_CLANK_ALONG_SPEED * 60.0f;
        p.takeoff = RAC2_CLANK_TAKEOFF;
        p.apex_frame = RAC2_CLANK_APEX_FRAME;
        p.target_frame = RAC2_CLANK_TARGET_FRAME;
        p.jump_air_speed = RAC2_CLANK_AIR_STICK_SPEED * 60.0f;
        p.punch_turn_limit = RAC2_CLANK_PUNCH_TURN_LIMIT * 60.0f;
        p.square_window = RAC2_CLANK_SQUARE_WINDOW;
        p.land_square_window = RAC2_CLANK_LAND_SQUARE_WINDOW;
        p.punch_lead = BM_CLANK_TUNE_PUNCH_LEAD; /* measured on rac2's punch, DESIGN.md 9.2 */
        p.run_out_frame = (float)RAC1_CLANK_REC_EXIT;
        p.run_out_stick = 0.0f;
        return p;
    }

    // rac2's Clank taken further, from clank_tune.h.
    if (choice == BM_CLANK_ENHANCED) {
        p.top_speed = BM_CLANK_TUNE_TOP_SPEED;
        p.run_accel = BM_CLANK_TUNE_RUN_ACCEL;
        p.run_brake = BM_CLANK_TUNE_RUN_BRAKE;
        p.turn_accel = BM_CLANK_TUNE_TURN_ACCEL;
        p.turn_damp = BM_CLANK_TUNE_TURN_DAMP;
        p.turn_limit = BM_CLANK_TUNE_TURN_LIMIT;
        p.turnaround_accel = BM_CLANK_TUNE_TURNAROUND_ACCEL;
        p.turnaround_limit = BM_CLANK_TUNE_TURNAROUND_LIMIT;
        p.along_speed = BM_CLANK_TUNE_ALONG_SPEED;
        p.takeoff = BM_CLANK_TUNE_TAKEOFF;
        p.apex_frame = BM_CLANK_TUNE_APEX_FRAME;
        p.target_frame = BM_CLANK_TUNE_TARGET_FRAME;
        p.jump_air_speed = BM_CLANK_TUNE_JUMP_AIR_SPEED;
        p.punch_turn_limit = BM_CLANK_TUNE_PUNCH_TURN_LIMIT;
        p.square_window = BM_CLANK_TUNE_SQUARE_WINDOW;
        p.land_square_window = BM_CLANK_TUNE_LAND_SQUARE_WINDOW;
        p.punch_lead = BM_CLANK_TUNE_PUNCH_LEAD;
        p.run_out_frame = BM_CLANK_TUNE_RUN_OUT_FRAME;
        p.run_out_stick = BM_CLANK_TUNE_RUN_OUT_STICK;
        return p;
    }

    // rac1's own numbers (rac1.h). His turnaround has no spring of its own, he never moves along
    // the stick, and his punch has no lead and no run out.
    p.top_speed = RAC1_CLANK_RUN_SPEED;
    p.run_accel = RAC1_CLANK_RUN_ACCEL;
    p.run_brake = RAC1_CLANK_RUN_BRAKE;
    p.turn_accel = RAC1_CLANK_TURN_FAST_ACCEL;
    p.turn_damp = RAC1_CLANK_TURN_DAMP;
    p.turn_limit = RAC1_CLANK_TURN_FAST_LIMIT;
    p.turnaround_accel = RAC1_CLANK_TURN_FAST_ACCEL;
    p.turnaround_limit = RAC1_CLANK_TURN_FAST_LIMIT;
    p.along_speed = 0.0f;
    p.takeoff = RAC1_CLANK_TAKEOFF;
    p.apex_frame = RAC1_CLANK_APEX_FRAME;
    p.target_frame = RAC1_CLANK_TARGET_FRAME;
    p.jump_air_speed = RAC1_CLANK_AIR_STICK_SPEED;
    p.punch_turn_limit = RAC1_WRENCH_TURN_LIMIT;
    p.square_window = RAC1_CLANK_SQUARE_WINDOW;
    p.land_square_window = RAC1_CLANK_LAND_SQUARE_WINDOW;
    p.punch_lead = 0.0f;
    p.run_out_frame = (float)RAC1_CLANK_REC_EXIT;
    p.run_out_stick = 0.0f;
    return p;
}

/* Returns whether a state is his run: 0x44, or 0x50, which shares its code. */
static inline int bm_clank_is_run(int state) {
    return state == RAC1_STATE_CLANK_RUN || state == RAC1_STATE_CLANK_RUN_ALT;
}

/* Returns whether an argument that came in is within 1% of rac1's value. */
static inline int bm_clank_near(float got, float want) {
    return fabsf(got - want) <= fabsf(want) * 0.01f;
}

/* ---- The run ---- */

/* Returns rac1's run turn this tick (hero_update_movement case 0x44), scaled by t = stick + 0.35:
 * the slow spring under half the run speed (`speed_xy`, this tick's units), else the fast one. */
static inline bm_spring bm_clank_rac1_turn(float stick, float speed_xy, const bm_tick* t) {
    const float s = stick + RAC1_CLANK_TURN_STICK_BIAS;
    bm_spring out;

    // Under half the run speed he turns on the slow spring, the quicker of the two.
    if (speed_xy < RAC1_CLANK_RUN_SPEED * t->ft * RAC1_CLANK_TURN_SLOW_SHARE) {
        out.accel = RAC1_CLANK_TURN_SLOW_ACCEL * s * t->fs2;
        out.damp = RAC1_CLANK_TURN_DAMP * t->fs2;
        out.limit = RAC1_CLANK_TURN_SLOW_LIMIT * s * t->ft;
        return out;
    }

    out.accel = RAC1_CLANK_TURN_FAST_ACCEL * s * t->fs2;
    out.damp = RAC1_CLANK_TURN_DAMP * t->fs2;
    out.limit = RAC1_CLANK_TURN_FAST_LIMIT * s * t->ft;
    return out;
}

/* Replaces rac1's run turn call `in`, on either of its springs, with the choice's. Returns 1 and
 * the new spring in `out`, each argument scaled by what came in over what rac1 asks, so that
 * another mod's scaling survives. Returns 0 for any other call, which passes as it came. */
static inline int bm_clank_turn(
    const bm_clank_params* p,
    bm_spring in,
    float stick,
    int turnaround,
    const bm_tick* t,
    bm_spring* out
) {
    const float s = stick + RAC1_CLANK_TURN_STICK_BIAS;
    bm_spring own;

    // rac1's slow spring for this stick.
    own.accel = RAC1_CLANK_TURN_SLOW_ACCEL * s * t->fs2;
    own.damp = RAC1_CLANK_TURN_DAMP * t->fs2;
    own.limit = RAC1_CLANK_TURN_SLOW_LIMIT * s * t->ft;

    // Not the slow spring: try the fast one, which shares its damping.
    if (!(bm_clank_near(in.accel, own.accel) && bm_clank_near(in.damp, own.damp)
          && bm_clank_near(in.limit, own.limit))) {
        own.accel = RAC1_CLANK_TURN_FAST_ACCEL * s * t->fs2;
        own.limit = RAC1_CLANK_TURN_FAST_LIMIT * s * t->ft;

        // Neither spring: this is not rac1's run turn.
        if (!(bm_clank_near(in.accel, own.accel) && bm_clank_near(in.damp, own.damp)
              && bm_clank_near(in.limit, own.limit))) {
            return 0;
        }
    }

    // The turnaround has its own accel and limit.
    out->accel =
        in.accel * ((turnaround ? p->turnaround_accel : p->turn_accel) * t->fs2 / own.accel);
    out->damp = in.damp * (p->turn_damp * t->fs2 / own.damp);
    out->limit =
        in.limit * ((turnaround ? p->turnaround_limit : p->turn_limit) * t->ft / own.limit);
    return 1;
}

/* The hero values rac2's run reads to set its flags, after the turn. */
typedef struct bm_clank_flag_in {
    float stick;       /* stick amount (+0x229C) */
    float yaw_error;   /* move yaw less facing after this tick's turn (+0x188) */
    float flat_speed;  /* ground speed, this tick's units (+0x164) */
    float stick_swing; /* angle between the stick 0 polls back (the ring's oldest) and 2 back */
} bm_clank_flag_in;

/* Updates rac2's run flags as case 0x40 sets them after the turn and the speed step: along the
 * stick when slow, a turnaround when slow and far off or when the stick swings round, and both
 * cleared once he faces the stick. They are kept in bm_run_flags, as Ratchet's are. */
static inline void bm_clank_flags_update(
    bm_run_flags* f, const bm_clank_params* p, const bm_clank_flag_in* in, const bm_tick* t
) {
    const float err = fabsf(in->yaw_error);

    // Under the along speed he moves along the stick.
    if (in->flat_speed < p->along_speed * t->ft) {
        f->along = 1;

        // More than 100 degrees off with the stick past 0.8 starts a turnaround.
        if (err > RAC2_CLANK_TURNAROUND_ANGLE && in->stick > RAC2_CLANK_TURNAROUND_STICK) {
            f->turnaround = 1;
        }
    }

    // A stick past 0.8 that swung more than 150 degrees, over 70 degrees off, turns him round at
    // any speed.
    if (in->stick_swing > RAC2_CLANK_FLICK_ANGLE && err > RAC2_CLANK_FLICK_ERROR
        && in->stick > RAC2_CLANK_FLICK_STICK) {
        f->turnaround = 1;
        f->along = 1;
    }

    // Facing the stick within 5 degrees, he moves along his facing again.
    if (err < RAC2_CLANK_ALONG_CLEAR) {
        f->along = 0;
    }

    // The turnaround ends within 4 degrees.
    if (f->turnaround && err < RAC2_CLANK_TURNAROUND_CLEAR) {
        f->turnaround = 0;
    }
}

/* Returns the choice's speed step for rac1's run call. rac1 asked for `target` (+0x190, the stick
 * times 3 u/s) and `down` (8.5 u/s^2), either possibly changed by another mod. The target is scaled
 * by the choice's top speed over rac1's; up and down are the choice's, times the scale `down` came
 * with. */
static inline bm_speed bm_clank_run_speed(
    const bm_clank_params* p, float target, float down, const bm_tick* t
) {
    const float own = RAC1_CLANK_RUN_BRAKE * t->ft2;
    // The scale another mod put on `down`.
    float scale = own > 0.0f ? down / own : 1.0f;
    bm_speed out;

    // A scale that is not a finite number above 0 counts as none.
    if (!isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }

    out.target = target * (p->top_speed / RAC1_CLANK_RUN_SPEED);
    out.up = p->run_accel * t->ft2 * scale;
    out.down = p->run_brake * t->ft2 * scale;
    return out;
}

/* ---- The jump ---- */

/* His jump's entry values (HeroEnterMoveState, state 0x49), written on the jump's first tick before
 * its first physics tick. */
typedef struct bm_clank_jump {
    int takeoff;     /* takeoff_frame (+0x420) */
    float apex;      /* apex_anim_frame (+0x440) */
    float target;    /* target_anim_frame (+0x444) */
    float air_speed; /* air_stick_speed (+0x424), this tick's units */
} bm_clank_jump;

/* Returns the choice's entry values in place of rac1's defaults; any other value, such as a level's
 * own, stays. The choice's takeoff is counted as rac2's PAL release counts it (2 at 50 Hz for 3),
 * and rac1's as scale_ticks counts it (6 at 50 Hz for 5). */
static inline bm_clank_jump bm_clank_jump_entry(
    const bm_clank_params* p, bm_clank_jump in, const bm_tick* t
) {
    bm_clank_jump out = in;
    const float air = RAC1_CLANK_AIR_STICK_SPEED * t->ft;

    // rac1's default takeoff becomes the choice's.
    if (in.takeoff == bm_scale_ticks(RAC1_CLANK_TAKEOFF, t)) {
        out.takeoff = bm_rac2_ticks(p->takeoff, t);
    }

    // So does its apex frame.
    if (in.apex == RAC1_CLANK_APEX_FRAME) {
        out.apex = p->apex_frame;
    }

    // And its landing frame.
    if (in.target == RAC1_CLANK_TARGET_FRAME) {
        out.target = p->target_frame;
    }

    // rac1's default air stick speed, within float rounding (0.0001 u/s), scales to the choice's.
    if (fabsf(in.air_speed - air) < 1e-4f * t->ft) {
        out.air_speed = in.air_speed * (p->jump_air_speed / RAC1_CLANK_AIR_STICK_SPEED);
    }

    return out;
}

/* ---- Square remembered ---- */

/* Returns whether a Square `age` polls back (1 newest, 0 none) should be shown to rac1's
 * transitions as pressed this poll: it is inside the choice's window but outside rac1's, both
 * counted by scale_ticks. `landing` picks the jump's landing window (past the apex, near the
 * ground) over idle's and the run's. */
static inline int bm_clank_square_buffer(
    const bm_clank_params* p, int age, int landing, const bm_tick* t
) {
    const int own =
        bm_scale_ticks(landing ? RAC1_CLANK_LAND_SQUARE_WINDOW : RAC1_CLANK_SQUARE_WINDOW, t);
    const int mine = bm_scale_ticks(landing ? p->land_square_window : p->square_window, t);
    return bm_within(age, mine) && !bm_within(age, own);
}

/* ---- The punch ---- */

/* The punch values the transitions read in state 0x51. */
typedef struct bm_clank_punch_in {
    int record;   /* attack_record: 7 and 8 are his punches */
    float frame;  /* anim_frame */
    int changing; /* anim_changing: the punch is still blending in */
    float stick;  /* stick amount */
    int cross;    /* polls since X was last pressed (1 newest), 0 none */
} bm_clank_punch_in;

/* A punch's steer for this tick: the frame to show rac1's transitions so that its own code jumps
 * out or stands. Steered frames stay under 12, where Square would chain, so they chain nothing. To
 * stand, X is hidden, since the steered frame's jump window is longer than the choice's. */
typedef struct bm_clank_punch_steer {
    int steer;      /* 1: anim_frame set to `frame` for the transitions */
    float frame;    /* the frame to show */
    int hide_cross; /* polls of X to hide */
    int jump;       /* the steer is a jump out, for the trace */
    int run_out;    /* the steer is a run out with the stick, for the trace */
} bm_clank_punch_steer;

/* Returns whether X `cross` polls back jumps out of a punch at `frame` read `lead` frames ahead:
 * past the record's jump frame, and within (frame - first) * 2 polls (hero_update_transitions case
 * 0x51). */
static inline int bm_clank_punch_jumps(float frame, float lead, int cross) {
    const float f = frame + lead;
    return (float)RAC1_CLANK_REC_JUMP < f
           && bm_within(cross, bm_swing_window(f, RAC1_CLANK_REC_FIRST));
}

/* Returns the punch's steer. rac1 stands past frame 10 and jumps out past frame 8 with X within
 * (frame - 3) * 2 polls; it checks the jump last, so the jump wins. The choice reads the frame
 * `punch_lead` ahead for both, and with the stick pushed also leaves past its run-out frame. Where
 * rac1 would stay and the choice would leave, rac1 is shown a frame where its own code leaves the
 * same way: for a jump, the first frame past 8 whose window takes the press; to stand, half a
 * frame past 10. */
static inline bm_clank_punch_steer bm_clank_punch(
    const bm_clank_params* p, const bm_clank_punch_in* in
) {
    bm_clank_punch_steer s = {0, 0.0f, 0, 0, 0};
    const float own = (float)RAC1_CLANK_REC_EXIT;

    // Only his punches are steered, once blended in, and only while rac1 would not leave by itself.
    if ((in->record != RAC1_CLANK_REC_A && in->record != RAC1_CLANK_REC_B) || in->changing
        || in->frame > own || bm_clank_punch_jumps(in->frame, 0.0f, in->cross)) {
        return s;
    }

    // A jump out ahead of rac1's: show the first frame past 8 whose window takes the press, half a
    // frame in.
    if (bm_clank_punch_jumps(in->frame, p->punch_lead, in->cross)) {
        int n = RAC1_CLANK_REC_FIRST + (in->cross + 1) / 2;

        // It must also be past the jump frame.
        if (n < RAC1_CLANK_REC_JUMP) {
            n = RAC1_CLANK_REC_JUMP;
        }

        s.steer = 1;
        s.frame = (float)n + 0.5f;
        s.jump = 1;
        return s;
    }

    // A run out with the stick pushed past the run-out frame.
    s.run_out =
        p->run_out_stick > 0.0f && in->stick > p->run_out_stick && in->frame > p->run_out_frame;

    // Without a run out, he leaves only once the lead takes him past rac1's exit frame.
    if (!s.run_out && !(in->frame + p->punch_lead > own)) {
        return s;
    }

    // Show rac1 half a frame past its exit frame, with X hidden.
    s.steer = 1;
    s.frame = own + 0.5f;
    s.hide_cross = 30; /* the pad's whole history */
    return s;
}

/* Replaces the limit of rac1's punch lock-on turn, turn_to(0.05, 0.2, 15.01) while a target or a
 * heading is locked, with the choice's, scaled as the wrench's is. Returns 1 and the new spring in
 * `out`, or 0 for any other call. */
static inline int bm_clank_punch_turn(
    const bm_clank_params* p, bm_spring in, const bm_tick* t, bm_spring* out
) {
    const float limit = RAC1_WRENCH_TURN_LIMIT * t->ft;

    // Only rac1's own lock-on turn, each argument within 1%.
    if (!(bm_clank_near(in.accel, RAC1_WRENCH_TURN_ACCEL * t->fs2)
          && bm_clank_near(in.damp, RAC1_WRENCH_TURN_DAMP * t->fs2)
          && bm_clank_near(in.limit, limit))) {
        return 0;
    }

    // Only the limit changes.
    out->accel = in.accel;
    out->damp = in.damp;
    out->limit = in.limit * (p->punch_turn_limit * t->ft / limit);
    return 1;
}

#endif /* RAC1_ENHANCED_MOVEMENT_CLANK_H */
