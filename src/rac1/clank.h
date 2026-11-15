/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Clank's controls under the `clank` setting, decided apart from game memory (DESIGN.md section 9,
 * PLAN.md section 11). Written for this mod from rac1-decomp's hero_update_movement (Clank's states
 * 0x43 to 0x52), hero_update_transitions and HeroEnterMoveState, and rac2-decomp's func_L00_002BA7D8
 * (cases 0x3F to 0x4E), func_L00_002CB3A8 and func_L00_002C42E0, the same states carried on.
 *
 * In the first game Clank alone is a "vehicle mode" of the hero (+0x20A4 = 1) with states of his own,
 * run through the same helpers as Ratchet's: his run turns with turn_to, speeds with speed_step and
 * moves with set_planar_vel, and his punch turns toward a target with the wrench's turn_to. The mod
 * changes their arguments in the hooks on those helpers, writes his jump's entry values on its first
 * tick, and steers his transitions for a tick (a Square shown as pressed, the punch's frame), as it
 * does for Ratchet. It never calls a game function.
 *
 * The choices:
 * - rac1: the game's; the mod stays out of him.
 * - rac2: Going Commando's Clank where it differs: the run's speeding up and slowing, turning and
 *   turnaround, moving along the stick; the jump's quicker takeoff; the punch's quicker lock-on turn
 *   and its quicker ways out (it stands after 22 ticks, jumps out after 18).
 * - enhanced: rac2's, and past it by the steps clank_tune.h holds.
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

/* The setting's choices. */
enum {
    BM_CLANK_RAC1 = 0,    /* the game's: the mod takes nothing of his */
    BM_CLANK_RAC2 = 1,    /* Going Commando's Clank */
    BM_CLANK_ENHANCED = 2 /* past it, by clank_tune.h */
};

/* What a choice plays with, per second (u/s, u/s^2, rad/s), turn coefficients at 60 Hz, counts at 60
 * Hz before the game's own counting, frames as attack_records count them. */
typedef struct bm_clank_params {
    float top_speed;        /* the run at full stick (rac1 3.0) */
    float run_accel;        /* speeding up in the run (rac1 7.5 x k) */
    float run_brake;        /* slowing in the run (rac1 8.5) */
    float turn_accel;       /* the run's turn (rac1 by stick and speed) */
    float turn_damp;        /* and its damping */
    float turn_limit;       /* and its limit */
    float turnaround_accel; /* the turnaround's */
    float turnaround_limit;
    float along_speed; /* along the stick under this speed */
    int takeoff;       /* the jump leaves the ground after these ticks (rac1 5) */
    float apex_frame;  /* the jump's animation frames (rac1 18 and 27) */
    float target_frame;
    float jump_air_speed;   /* the stick's speed in his jump (rac1 2.0) */
    float punch_turn_limit; /* the punch's lock-on turn (rac1 15.01) */
    int square_window;      /* Square remembered standing or running (rac1 9) */
    int land_square_window; /* and landing (rac1 5) */
    float punch_lead;       /* the punch's way out reads this many frames ahead (rac1 0) */
    float run_out_frame;    /* with the stick past run_out_stick he leaves past this frame */
    float run_out_stick;    /* 0: no run out of his own */
} bm_clank_params;

/* The numbers of a choice; rac1's for BM_CLANK_RAC1 or any value it does not know (the mod does
 * nothing with them: they are the game's own). */
static inline bm_clank_params bm_clank_params_for(int choice) {
    bm_clank_params p;

    // Going Commando's Clank, from its code (rac2.h).
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
        p.punch_lead = BM_CLANK_TUNE_PUNCH_LEAD; /* measured: rac2's punch, DESIGN.md 9.2 */
        p.run_out_frame = (float)RAC1_CLANK_REC_EXIT;
        p.run_out_stick = 0.0f;
        return p;
    }

    // Past it (clank_tune.h).
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

/* Whether a state is his run (0x44, or 0x50, which shares its code). */
static inline int bm_clank_is_run(int state) {
    return state == RAC1_STATE_CLANK_RUN || state == RAC1_STATE_CLANK_RUN_ALT;
}

/* Whether arguments came in as rac1's, within 1% each. */
static inline int bm_clank_near(float got, float want) {
    return fabsf(got - want) <= fabsf(want) * 0.01f;
}

/* --- The run ----------------------------------------------------------------------------------- */

/* rac1's run turn this tick (hero_update_movement case 0x44): t = stick + 0.35; under half the run
 * speed (speed_xy, this tick's units) the slow spring, else the fast one. */
static inline bm_spring bm_clank_rac1_turn(float stick, float speed_xy, const bm_tick* t) {
    const float s = stick + RAC1_CLANK_TURN_STICK_BIAS;
    bm_spring out;

    // Slow: a quick spring to face the stick.
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

/* The run's turn for rac1's call `in` (either of its springs): 1 and the choice's spring in `out`,
 * each argument scaled by what came in over what rac1 asks (another mod's scaling survives); 0 for a
 * call that is not rac1's run turn (it passes as it came). */
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

    own.accel = RAC1_CLANK_TURN_SLOW_ACCEL * s * t->fs2;
    own.damp = RAC1_CLANK_TURN_DAMP * t->fs2;
    own.limit = RAC1_CLANK_TURN_SLOW_LIMIT * s * t->ft;

    // Not the slow spring: the fast one, or none of rac1's.
    if (!(bm_clank_near(in.accel, own.accel) && bm_clank_near(in.damp, own.damp)
          && bm_clank_near(in.limit, own.limit))) {
        own.accel = RAC1_CLANK_TURN_FAST_ACCEL * s * t->fs2;
        own.limit = RAC1_CLANK_TURN_FAST_LIMIT * s * t->ft;

        // Neither.
        if (!(bm_clank_near(in.accel, own.accel) && bm_clank_near(in.damp, own.damp)
              && bm_clank_near(in.limit, own.limit))) {
            return 0;
        }
    }

    out->accel =
        in.accel * ((turnaround ? p->turnaround_accel : p->turn_accel) * t->fs2 / own.accel);
    out->damp = in.damp * (p->turn_damp * t->fs2 / own.damp);
    out->limit =
        in.limit * ((turnaround ? p->turnaround_limit : p->turn_limit) * t->ft / own.limit);
    return 1;
}

/* What rac2's run reads to set its flags, after its turn. */
typedef struct bm_clank_flag_in {
    float stick;       /* stick amount (+0x229C) */
    float yaw_error;   /* move yaw less facing after this tick's turn (+0x188) */
    float flat_speed;  /* this tick's units (+0x164) */
    float stick_swing; /* the stick's angle 0 polls back (the ring's oldest) against 2 back */
} bm_clank_flag_in;

/* rac2's flags (case 0x40, after the turn and the speed step): along the stick when slow, the
 * turnaround when slow and far off, or when the stick swung round; cleared once he faces it. Kept in
 * bm_run_flags (along, turnaround), as Ratchet's. */
static inline void bm_clank_flags_update(
    bm_run_flags* f, const bm_clank_params* p, const bm_clank_flag_in* in, const bm_tick* t
) {
    const float err = fabsf(in->yaw_error);

    // Slow: along the stick, and a turnaround when far off with the stick pushed.
    if (in->flat_speed < p->along_speed * t->ft) {
        f->along = 1;

        // More than 100 degrees off.
        if (err > RAC2_CLANK_TURNAROUND_ANGLE && in->stick > RAC2_CLANK_TURNAROUND_STICK) {
            f->turnaround = 1;
        }
    }

    // The stick swung more than 150 degrees, 70 degrees off.
    if (in->stick_swing > RAC2_CLANK_FLICK_ANGLE && err > RAC2_CLANK_FLICK_ERROR
        && in->stick > RAC2_CLANK_FLICK_STICK) {
        f->turnaround = 1;
        f->along = 1;
    }

    // Facing the stick within 5 degrees: along the facing again.
    if (err < RAC2_CLANK_ALONG_CLEAR) {
        f->along = 0;
    }

    // Within 4 degrees, the turnaround is over.
    if (f->turnaround && err < RAC2_CLANK_TURNAROUND_CLEAR) {
        f->turnaround = 0;
    }
}

/* The run's speed step for rac1's call: rac1 asked target (+0x190, the stick times 3, or what another
 * mod made of it) and down (8.5, or another mod's scaling of it). The choice's: the target times its
 * top speed over rac1's, up and down its own, all by the scale `down` came with. */
static inline bm_speed bm_clank_run_speed(
    const bm_clank_params* p, float target, float down, const bm_tick* t
) {
    const float own = RAC1_CLANK_RUN_BRAKE * t->ft2;
    float scale = own > 0.0f ? down / own : 1.0f;
    bm_speed out;

    // A scale that is no number, or not above 0, is none.
    if (!isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }

    out.target = target * (p->top_speed / RAC1_CLANK_RUN_SPEED);
    out.up = p->run_accel * t->ft2 * scale;
    out.down = p->run_brake * t->ft2 * scale;
    return out;
}

/* --- The jump ---------------------------------------------------------------------------------- */

/* His jump's entry values (HeroEnterMoveState 0x49), written on its first tick before its first
 * physics tick. */
typedef struct bm_clank_jump {
    int takeoff;     /* takeoff_frame (+0x420) */
    float apex;      /* apex_anim_frame (+0x440) */
    float target;    /* target_anim_frame (+0x444) */
    float air_speed; /* air_stick_speed (+0x424), this tick's units */
} bm_clank_jump;

/* The choice's entry values where rac1 left its own (another value, a level's, is left alone).
 * The takeoff is counted as rac2's PAL release counts it (2 at 50 Hz for 3), rac1's own as
 * scale_ticks counts it (6 at 50 Hz for 5). */
static inline bm_clank_jump bm_clank_jump_entry(
    const bm_clank_params* p, bm_clank_jump in, const bm_tick* t
) {
    bm_clank_jump out = in;
    const float air = RAC1_CLANK_AIR_STICK_SPEED * t->ft;

    // Off the ground sooner.
    if (in.takeoff == bm_scale_ticks(RAC1_CLANK_TAKEOFF, t)) {
        out.takeoff = bm_rac2_ticks(p->takeoff, t);
    }

    // The animation's frames to match.
    if (in.apex == RAC1_CLANK_APEX_FRAME) {
        out.apex = p->apex_frame;
    }

    // And its landing.
    if (in.target == RAC1_CLANK_TARGET_FRAME) {
        out.target = p->target_frame;
    }

    // The stick's speed in the air.
    if (fabsf(in.air_speed - air) < 1e-4f * t->ft) {
        out.air_speed = in.air_speed * (p->jump_air_speed / RAC1_CLANK_AIR_STICK_SPEED);
    }

    return out;
}

/* --- Square remembered ------------------------------------------------------------------------- */

/* Whether a Square `age` polls back (1 the newest, 0 none) should be shown to rac1's transitions as
 * pressed this poll: within the choice's window, not within rac1's (scale_ticks of each). `landing`:
 * the jump's landing window (past the apex, near the ground), else idle's and the run's. */
static inline int bm_clank_square_buffer(
    const bm_clank_params* p, int age, int landing, const bm_tick* t
) {
    const int own =
        bm_scale_ticks(landing ? RAC1_CLANK_LAND_SQUARE_WINDOW : RAC1_CLANK_SQUARE_WINDOW, t);
    const int mine = bm_scale_ticks(landing ? p->land_square_window : p->square_window, t);
    return bm_within(age, mine) && !bm_within(age, own);
}

/* --- The punch --------------------------------------------------------------------------------- */

/* What the transitions read of a punch (state 0x51). */
typedef struct bm_clank_punch_in {
    int record;   /* attack_record: 7 or 8 his punches */
    float frame;  /* anim_frame */
    int changing; /* anim_changing: the punch is still blending in */
    float stick;  /* stick amount */
    int cross;    /* how many polls back X was pressed last (1 the newest), 0 none */
} bm_clank_punch_in;

/* What to do with rac1's transitions for a punch this tick: steer the frame (to `frame`) so that
 * rac1's own code jumps out (the steered frame is under the record's chain frame, 12: it chains
 * nothing) or stands (X hidden: the steered frame's jump window is longer than the choice's). */
typedef struct bm_clank_punch_steer {
    int steer; /* 1: anim_frame set to `frame` for the transitions */
    float frame;
    int hide_cross;
    int jump;    /* the steer is a jump out, for the trace */
    int run_out; /* the stick's way out, for the trace */
} bm_clank_punch_steer;

/* Whether X `cross` polls back jumps out of a punch at `frame` read `lead` frames ahead: past the
 * record's jump frame, within (frame - first) * 2 polls (hero_update_transitions case 0x51). */
static inline int bm_clank_punch_jumps(float frame, float lead, int cross) {
    const float f = frame + lead;
    return (float)RAC1_CLANK_REC_JUMP < f
           && bm_within(cross, bm_swing_window(f, RAC1_CLANK_REC_FIRST));
}

/* The punch's steer. rac1 stands past frame 10 and jumps out past 8 with X within (frame - 3) * 2
 * polls, the jump checked last and winning. The choice reads the frame `punch_lead` ahead for both,
 * and with the stick pushed also leaves past its run-out frame. Where rac1 would stay and the choice
 * would leave, rac1 is shown a frame where its own code does the same: for a jump the first past 8
 * whose window takes the press, to stand just past 10. */
static inline bm_clank_punch_steer bm_clank_punch(
    const bm_clank_params* p, const bm_clank_punch_in* in
) {
    bm_clank_punch_steer s = {0, 0.0f, 0, 0, 0};
    const float own = (float)RAC1_CLANK_REC_EXIT;

    // Only his punches, once blended in, while rac1 has not left them by itself.
    if ((in->record != RAC1_CLANK_REC_A && in->record != RAC1_CLANK_REC_B) || in->changing
        || in->frame > own || bm_clank_punch_jumps(in->frame, 0.0f, in->cross)) {
        return s;
    }

    // The jump out, ahead of rac1's: the first frame past 8 whose window takes the press.
    if (bm_clank_punch_jumps(in->frame, p->punch_lead, in->cross)) {
        int n = RAC1_CLANK_REC_FIRST + (in->cross + 1) / 2;

        // Past the jump frame.
        if (n < RAC1_CLANK_REC_JUMP) {
            n = RAC1_CLANK_REC_JUMP;
        }

        s.steer = 1;
        s.frame = (float)n + 0.5f;
        s.jump = 1;
        return s;
    }

    // The stick's way out, or standing, ahead of rac1's.
    s.run_out =
        p->run_out_stick > 0.0f && in->stick > p->run_out_stick && in->frame > p->run_out_frame;

    // Neither yet.
    if (!s.run_out && !(in->frame + p->punch_lead > own)) {
        return s;
    }

    s.steer = 1;
    s.frame = own + 0.5f;
    s.hide_cross = 30; /* the pad's whole history */
    return s;
}

/* The punch's lock-on turn for rac1's (turn_to(0.05, 0.2, 15.01) while a target or a heading is
 * locked): 1 and the choice's limit, scaled as the wrench's is; 0 for any other call. */
static inline int bm_clank_punch_turn(
    const bm_clank_params* p, bm_spring in, const bm_tick* t, bm_spring* out
) {
    const float limit = RAC1_WRENCH_TURN_LIMIT * t->ft;

    // rac1's own lock-on turn.
    if (!(bm_clank_near(in.accel, RAC1_WRENCH_TURN_ACCEL * t->fs2)
          && bm_clank_near(in.damp, RAC1_WRENCH_TURN_DAMP * t->fs2)
          && bm_clank_near(in.limit, limit))) {
        return 0;
    }

    out->accel = in.accel;
    out->damp = in.damp;
    out->limit = in.limit * (p->punch_turn_limit * t->ft / limit);
    return 1;
}

#endif /* RAC1_ENHANCED_MOVEMENT_CLANK_H */
