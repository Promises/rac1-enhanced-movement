/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Clank's decisions (src/rac1/clank.h) against values worked out by hand from both games' code: what
 * is the same in both games, each choice's numbers, rac1's run turn recognised and replaced, rac2's
 * run flags, the run's speed step with another mod's scaling, the jump's entry values, Square
 * remembered, the punch's way out and its lock-on turn, at 60 Hz and at 50 Hz.
 */
#include "check.h"
#include "rac1/clank.h"

/* The same in both games: what the mod leaves as rac1's (DESIGN.md 9.3). Each rac2 literal is per
 * tick (x 60) or per tick squared (x 3600). */
static void same_in_both(void) {
    CHECK_NEAR(RAC2_CLANK_RUN_SPEED * 60.0f, RAC1_CLANK_RUN_SPEED, 1e-5);
    CHECK_NEAR(RAC2_CLANK_AIR_STICK_SPEED * 60.0f, RAC1_CLANK_AIR_STICK_SPEED, 1e-5);
    CHECK_NEAR(RAC2_CLANK_GLIDE_SPEED * 60.0f, RAC1_CLANK_GLIDE_SPEED, 1e-5);
    CHECK_NEAR(RAC2_CLANK_GLIDE_TURN_LIMIT * 60.0f, RAC1_CLANK_GLIDE_TURN_LIMIT, 1e-4);
    CHECK_NEAR(RAC2_CLANK_JUMP_HEIGHT, RAC1_CLANK_JUMP_HEIGHT, TOL);
    CHECK_NEAR(RAC2_CLANK_JUMP_HEIGHT_HELD, RAC1_CLANK_JUMP_HEIGHT_HELD, TOL);
    CHECK_NEAR(RAC2_CLANK_JUMP_GRAVITY * 3600.0f, RAC1_CLANK_JUMP_GRAVITY, 1e-4);
    CHECK_NEAR(RAC2_CLANK_IDLE_BRAKE * 3600.0f, RAC1_CLANK_IDLE_BRAKE, 1e-4);
    CHECK_NEAR(RAC2_CLANK_PUNCH_ACCEL * 3600.0f, RAC1_CLANK_PUNCH_ACCEL, 1e-3);
    CHECK_NEAR(RAC2_CLANK_PUNCH_BRAKE * 3600.0f, RAC1_CLANK_PUNCH_BRAKE, 1e-3);
    CHECK_EQ(RAC2_CLANK_JUMP_WINDOW, RAC1_CLANK_JUMP_WINDOW);
    CHECK_EQ(RAC2_CLANK_SQUARE_WINDOW, RAC1_CLANK_SQUARE_WINDOW);
    CHECK_EQ(RAC2_CLANK_LAND_SQUARE_WINDOW, RAC1_CLANK_LAND_SQUARE_WINDOW);

    // His punches' records, 7 and 8, in both games.
    CHECK_EQ(RAC2_REC7_CHAIN, RAC1_CLANK_REC_CHAIN);
    CHECK_EQ(RAC2_REC7_FIRST, RAC1_CLANK_REC_FIRST);
    CHECK_EQ(RAC2_REC7_OPEN, RAC1_CLANK_REC_OPEN);
    CHECK_EQ(RAC2_REC7_JUMP, RAC1_CLANK_REC_JUMP);
    CHECK_EQ(RAC2_REC7_EXIT, RAC1_CLANK_REC_EXIT);
    CHECK_EQ(RAC2_REC7_HIT_FROM, RAC1_CLANK_REC_HIT_FROM);
    CHECK_EQ(RAC2_REC7_HIT_TO, RAC1_CLANK_REC_HIT_TO);
    CHECK_EQ(RAC2_REC8_CHAIN, RAC2_REC7_CHAIN);
    CHECK_EQ(
        RAC2_REC8_FIRST + RAC2_REC8_OPEN + RAC2_REC8_JUMP + RAC2_REC8_EXIT,
        RAC2_REC7_FIRST + RAC2_REC7_OPEN + RAC2_REC7_JUMP + RAC2_REC7_EXIT
    );
    CHECK_EQ(
        RAC2_REC8_HIT_FROM * 100 + RAC2_REC8_HIT_TO, RAC2_REC7_HIT_FROM * 100 + RAC2_REC7_HIT_TO
    );

    // The punch's lock-on turn is the wrench's in both.
    CHECK_NEAR(RAC2_CLANK_PUNCH_TURN_LIMIT, RAC2_WRENCH_TURN_LIMIT, TOL);
}

/* Each choice's numbers, a second. */
static void params(void) {
    const bm_clank_params r1 = bm_clank_params_for(BM_CLANK_RAC1);
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    const bm_clank_params en = bm_clank_params_for(BM_CLANK_ENHANCED);
    const bm_clank_params other = bm_clank_params_for(7);

    // rac2: 3 u/s, 18 / 20 u/s^2, 0.033 / 0.15 / 9.95, 0.037 / 11.69, along under 0.7, takeoff 3,
    // frames 15 / 24, air 2, lock-on 16.93, windows 9 / 5, the punch read a frame ahead, no run out.
    CHECK_NEAR(r2.top_speed, 3.0, 1e-5);
    CHECK_NEAR(r2.run_accel, 18.0, 1e-4);
    CHECK_NEAR(r2.run_brake, 20.0, 1e-4);
    CHECK_NEAR(r2.turn_accel, 0.033, TOL);
    CHECK_NEAR(r2.turn_damp, 0.15, TOL);
    CHECK_NEAR(r2.turn_limit, 9.948378, 1e-5);
    CHECK_NEAR(r2.turnaround_accel, 0.037, TOL);
    CHECK_NEAR(r2.turnaround_limit, 11.693708, 1e-5);
    CHECK_NEAR(r2.along_speed, 0.7, 1e-6);
    CHECK_EQ(r2.takeoff, 3);
    CHECK_NEAR(r2.apex_frame, 15.0, TOL);
    CHECK_NEAR(r2.target_frame, 24.0, TOL);
    CHECK_NEAR(r2.jump_air_speed, 2.0, 1e-5);
    CHECK_NEAR(r2.punch_turn_limit, 16.929696, 1e-4);
    CHECK_EQ(r2.square_window, 9);
    CHECK_EQ(r2.land_square_window, 5);
    CHECK_NEAR(r2.punch_lead, 1.0, TOL);
    CHECK_NEAR(r2.run_out_stick, 0.0, TOL);

    // enhanced: clank_tune.h's.
    CHECK_NEAR(en.top_speed, BM_CLANK_TUNE_TOP_SPEED, TOL);
    CHECK_NEAR(en.jump_air_speed, BM_CLANK_TUNE_JUMP_AIR_SPEED, TOL);
    CHECK_EQ(en.square_window, BM_CLANK_TUNE_SQUARE_WINDOW);
    CHECK_EQ(en.land_square_window, BM_CLANK_TUNE_LAND_SQUARE_WINDOW);
    CHECK_NEAR(en.run_out_frame, BM_CLANK_TUNE_RUN_OUT_FRAME, TOL);
    CHECK_NEAR(en.run_out_stick, BM_CLANK_TUNE_RUN_OUT_STICK, TOL);

    // rac1, and a value the mod does not know: the game's own.
    CHECK_NEAR(r1.top_speed, 3.0, TOL);
    CHECK_EQ(r1.takeoff, 5);
    CHECK_EQ(r1.square_window, 9);
    CHECK_NEAR(r1.punch_lead, 0.0, TOL);
    CHECK_NEAR(other.top_speed, r1.top_speed, TOL);
}

/* The yaw spring as rac1 runs it (advance_damped_scalar_velocity, then spring_angle's step): ticks to
 * turn `deg` degrees to within 5, at 60 Hz. */
static int turn_ticks(float accel, float damp, float limit_per_second, float deg) {
    const float limit = limit_per_second / 60.0f;
    const float target = deg * BM_PI / 180.0f;
    float p = 0.0f;
    float v = 0.0f;

    // A tick at a time.
    for (int t = 1; t < 400; t++) {
        const float err = target - p;
        v = v + (accel * err - damp * v);
        v = bm_clamp(v, -limit, limit);
        v = bm_clamp(v, -fabsf(err), fabsf(err));
        p += v;

        // Within 5 degrees.
        if (fabsf(target - p) < 5.0f * BM_PI / 180.0f) {
            return t;
        }
    }

    return -1;
}

/* rac1's run turn recognised, rac2's given (DESIGN.md 9.2): 90 degrees in 29 ticks at a run in rac1, 11
 * in rac2. */
static void run_turn(void) {
    const bm_tick t = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    bm_spring in;
    bm_spring out;

    // Full stick, running (speed 0.05 a tick, over half of 3 u/s): t = 1.35, 0.0108 / 0.15 / 13.43.
    in = bm_clank_rac1_turn(1.0f, 0.05f, &t);
    CHECK_NEAR(in.accel, 0.0108, TOL);
    CHECK_NEAR(in.damp, 0.15, TOL);
    CHECK_NEAR(in.limit, 9.948377 * 1.35 / 60.0, TOL);
    CHECK_EQ(bm_clank_turn(&r2, in, 1.0f, 0, &t, &out), 1);
    CHECK_NEAR(out.accel, 0.033, TOL);
    CHECK_NEAR(out.damp, 0.15, TOL);
    CHECK_NEAR(out.limit, 0.16580631, TOL);

    // Slow (0.02 a tick), half stick: t = 0.85, 0.02975 / 0.15 / 8.159; rac2's turnaround.
    in = bm_clank_rac1_turn(0.5f, 0.02f, &t);
    CHECK_NEAR(in.accel, 0.035 * 0.85, TOL);
    CHECK_NEAR(in.limit, 9.599311 * 0.85 / 60.0, TOL);
    CHECK_EQ(bm_clank_turn(&r2, in, 0.5f, 1, &t, &out), 1);
    CHECK_NEAR(out.accel, 0.037, TOL);
    CHECK_NEAR(out.limit, 0.19489513, TOL);

    // Off by under 1% (a float rounded another way): recognised, the difference kept.
    in = bm_clank_rac1_turn(1.0f, 0.05f, &t);
    in.limit *= 1.005f;
    CHECK_EQ(bm_clank_turn(&r2, in, 1.0f, 0, &t, &out), 1);
    CHECK_NEAR(out.limit, 0.16580631 * 1.005, TOL);

    // Another mod doubled rac1's limit: not rac1's call, so it passes as it came (as the wrench's
    // lock-on turn does).
    in.limit *= 2.0f;
    CHECK_EQ(bm_clank_turn(&r2, in, 1.0f, 0, &t, &out), 0);

    // A call that is none of rac1's run turns (the stick read differently, a level's own): as it came.
    in = bm_clank_rac1_turn(1.0f, 0.05f, &t);
    CHECK_EQ(bm_clank_turn(&r2, in, 0.4f, 0, &t, &out), 0);
    in.damp = 0.3f;
    CHECK_EQ(bm_clank_turn(&r2, in, 1.0f, 0, &t, &out), 0);

    // 50 Hz: rac1's spring x 1.44 and limit x 1.2 recognised; rac2's scaled the same way.
    in = bm_clank_rac1_turn(1.0f, 0.06f, &pal);
    CHECK_NEAR(in.accel, 0.0108 * 1.44, TOL);
    CHECK_EQ(bm_clank_turn(&r2, in, 1.0f, 0, &pal, &out), 1);
    CHECK_NEAR(out.accel, 0.033 * 1.44, TOL);
    CHECK_NEAR(out.damp, 0.15 * 1.44, TOL);
    CHECK_NEAR(out.limit, 0.16580631 * 1.2, 1e-6);

    // The turns themselves: 90 degrees at a run, rac1 29 ticks, rac2 11; 180 in a turnaround 17.
    CHECK_EQ(turn_ticks(0.0108f, 0.15f, 9.948377f * 1.35f, 90.0f), 29);
    CHECK_EQ(turn_ticks(0.033f, 0.15f, 9.948378f, 90.0f), 11);
    CHECK_EQ(turn_ticks(0.037f, 0.15f, 11.693708f, 180.0f), 17);
}

/* rac2's run flags (DESIGN.md 9.2). */
static void flags(void) {
    const bm_tick t = bm_tick_ntsc();
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    bm_run_flags f = {0, 0};
    bm_clank_flag_in in = {1.0f, 2.0f, 0.005f, 0.0f};

    // Slow (0.3 u/s) and 115 degrees off at full stick: along the stick, and a turnaround.
    bm_clank_flags_update(&f, &r2, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 1);

    // Within 4 degrees: both over.
    in.yaw_error = 0.05f;
    bm_clank_flags_update(&f, &r2, &in, &t);
    CHECK_EQ(f.along, 0);
    CHECK_EQ(f.turnaround, 0);

    // Slow, 90 degrees off: along the stick only.
    in.yaw_error = -1.57f;
    bm_clank_flags_update(&f, &r2, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 0);

    // Running (3 u/s), 80 degrees off, the stick swung 160 degrees: a turnaround, along the stick.
    f.along = 0;
    in.flat_speed = 0.05f;
    in.yaw_error = 1.4f;
    in.stick_swing = 2.8f;
    bm_clank_flags_update(&f, &r2, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 1);

    // The same at 0.7 of the stick: nothing new.
    f.along = 0;
    f.turnaround = 0;
    in.stick = 0.7f;
    bm_clank_flags_update(&f, &r2, &in, &t);
    CHECK_EQ(f.along + f.turnaround, 0);

    // Running, the stick swung only 140 degrees: nothing.
    in.stick = 1.0f;
    in.stick_swing = 2.44f;
    bm_clank_flags_update(&f, &r2, &in, &t);
    CHECK_EQ(f.along + f.turnaround, 0);
}

/* The run's speed step. */
static void run_speed(void) {
    const bm_tick t = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    const bm_clank_params en = bm_clank_params_for(BM_CLANK_ENHANCED);
    bm_speed s;

    // rac1 asked 3 u/s (0.05) and 8.5: rac2 keeps the target, 18 and 20 u/s^2.
    s = bm_clank_run_speed(&r2, 0.05f, 8.5f / 3600.0f, &t);
    CHECK_NEAR(s.target, 0.05, TOL);
    CHECK_NEAR(s.up, 0.005, TOL);
    CHECK_NEAR(s.down, 0.0055555557, TOL);

    // Enhanced: the target x 3.45 / 3; half stick 1.5 u/s becomes 1.725.
    s = bm_clank_run_speed(&en, 0.025f, 8.5f / 3600.0f, &t);
    CHECK_NEAR(s.target, 0.025 * BM_CLANK_TUNE_TOP_SPEED / 3.0, TOL);
    CHECK_NEAR(s.up, BM_CLANK_TUNE_RUN_ACCEL / 3600.0, TOL);

    // Another mod's x 1.5 on rac1's (its target and down): kept on rac2's.
    s = bm_clank_run_speed(&r2, 0.075f, 1.5f * 8.5f / 3600.0f, &t);
    CHECK_NEAR(s.target, 0.075, TOL);
    CHECK_NEAR(s.up, 1.5 * 0.005, TOL);
    CHECK_NEAR(s.down, 1.5 * 0.0055555557, TOL);

    // A down that is no number: no scaling.
    s = bm_clank_run_speed(&r2, 0.05f, NAN, &t);
    CHECK_NEAR(s.down, 0.0055555557, TOL);

    // 50 Hz: 18 x (1/50)^2.
    s = bm_clank_run_speed(&r2, 0.06f, 8.5f / 2500.0f, &pal);
    CHECK_NEAR(s.up, 18.0 / 2500.0, TOL);
    CHECK_NEAR(s.down, 20.0 / 2500.0, TOL);
}

/* His jump's entry values. */
static void jump(void) {
    const bm_tick t = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    const bm_clank_params en = bm_clank_params_for(BM_CLANK_ENHANCED);
    bm_clank_jump in = {5, 18.0f, 27.0f, 2.0f / 60.0f};
    bm_clank_jump out;

    // rac2: off the ground after 3, frames 15 / 24, the air speed rac1's.
    out = bm_clank_jump_entry(&r2, in, &t);
    CHECK_EQ(out.takeoff, 3);
    CHECK_NEAR(out.apex, 15.0, TOL);
    CHECK_NEAR(out.target, 24.0, TOL);
    CHECK_NEAR(out.air_speed, 2.0 / 60.0, TOL);

    // Enhanced: and 2.5 u/s in the air.
    out = bm_clank_jump_entry(&en, in, &t);
    CHECK_NEAR(out.air_speed, BM_CLANK_TUNE_JUMP_AIR_SPEED / 60.0, TOL);

    // Values that are not rac1's own are left alone.
    in.takeoff = 9;
    in.apex = 20.0f;
    in.air_speed = 4.0f / 60.0f;
    out = bm_clank_jump_entry(&en, in, &t);
    CHECK_EQ(out.takeoff, 9);
    CHECK_NEAR(out.apex, 20.0, TOL);
    CHECK_NEAR(out.air_speed, 4.0 / 60.0, TOL);

    // 50 Hz: rac1's takeoff scale_ticks(5) = 6; rac2's PAL release counts 3 as 2.
    in.takeoff = 6;
    in.air_speed = 2.0f / 50.0f;
    out = bm_clank_jump_entry(&r2, in, &pal);
    CHECK_EQ(out.takeoff, 2);
    CHECK_NEAR(out.air_speed, 2.0 / 50.0, TOL);
}

/* Square remembered. */
static void square(void) {
    const bm_tick t = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    const bm_clank_params r1 = bm_clank_params_for(BM_CLANK_RAC1);
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    const bm_clank_params en = bm_clank_params_for(BM_CLANK_ENHANCED);

    // Standing: 9 polls back rac1 takes itself; 10 to 13 enhanced shows it; 14 is too old.
    CHECK_EQ(bm_clank_square_buffer(&en, 9, 0, &t), 0);
    CHECK_EQ(bm_clank_square_buffer(&en, 10, 0, &t), 1);
    CHECK_EQ(bm_clank_square_buffer(&en, 13, 0, &t), 1);
    CHECK_EQ(bm_clank_square_buffer(&en, 14, 0, &t), 0);
    CHECK_EQ(bm_clank_square_buffer(&en, 0, 0, &t), 0);

    // rac1's and rac2's windows are the game's: nothing shown.
    CHECK_EQ(bm_clank_square_buffer(&r2, 10, 0, &t), 0);
    CHECK_EQ(bm_clank_square_buffer(&r1, 10, 0, &t), 0);

    // Landing: rac1 5, enhanced 7.
    CHECK_EQ(bm_clank_square_buffer(&en, 5, 1, &t), 0);
    CHECK_EQ(bm_clank_square_buffer(&en, 7, 1, &t), 1);
    CHECK_EQ(bm_clank_square_buffer(&en, 8, 1, &t), 0);

    // 50 Hz, counted as rac1 counts its own: 11 and 16.
    CHECK_EQ(bm_clank_square_buffer(&en, 11, 0, &pal), 0);
    CHECK_EQ(bm_clank_square_buffer(&en, 16, 0, &pal), 1);
    CHECK_EQ(bm_clank_square_buffer(&en, 17, 0, &pal), 0);
}

/* The punch's way out. */
static void punch(void) {
    const bm_clank_params r1 = bm_clank_params_for(BM_CLANK_RAC1);
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    const bm_clank_params en = bm_clank_params_for(BM_CLANK_ENHANCED);
    bm_clank_punch_in in = {7, 9.5f, 0, 0.0f, 0};
    bm_clank_punch_steer s;

    // rac2 at frame 9.5 (10.5 a frame ahead): it stands, rac1 not before 10: frame 10.5 for the
    // transitions, X hidden.
    s = bm_clank_punch(&r2, &in);
    CHECK_EQ(s.steer, 1);
    CHECK_NEAR(s.frame, 10.5, TOL);
    CHECK_EQ(s.hide_cross, 30);
    CHECK_EQ(s.jump + s.run_out, 0);

    // At 9.0 and at 10.5 nothing: rac2 not yet, rac1 by itself.
    in.frame = 9.0f;
    CHECK_EQ(bm_clank_punch(&r2, &in).steer, 0);
    in.frame = 10.5f;
    CHECK_EQ(bm_clank_punch(&r2, &in).steer, 0);

    // X 11 polls back at 9.5: rac1's window there is (9 - 3) * 2 = 12, so rac1 jumps itself.
    in.frame = 9.5f;
    in.cross = 11;
    CHECK_EQ(bm_clank_punch(&r2, &in).steer, 0);

    // X 9 back at 7.5: rac1 not before 8; rac2 a frame ahead (8.5, window 10) jumps: rac1 is shown
    // 8.5, whose window takes it. X 12 back at 8.5: rac1's window 10, rac2's (9.5) 12: shown 9.5.
    in.frame = 7.5f;
    in.cross = 9;
    s = bm_clank_punch(&r2, &in);
    CHECK_EQ(s.steer, 1);
    CHECK_EQ(s.jump, 1);
    CHECK_NEAR(s.frame, 8.5, TOL);
    CHECK_EQ(s.hide_cross, 0);
    in.frame = 8.5f;
    in.cross = 12;
    s = bm_clank_punch(&r2, &in);
    CHECK_EQ(s.jump, 1);
    CHECK_NEAR(s.frame, 9.5, TOL);

    // X 11 back at 7.5 is past rac2's window (10) too: nothing.
    in.frame = 7.5f;
    in.cross = 11;
    CHECK_EQ(bm_clank_punch(&r2, &in).steer, 0);
    in.cross = 0;

    // Enhanced, full stick at 7.5: the run out, a frame after the hit (frames 3 to 6).
    in.stick = 1.0f;
    s = bm_clank_punch(&en, &in);
    CHECK_EQ(s.steer, 1);
    CHECK_EQ(s.run_out, 1);
    CHECK_NEAR(s.frame, 10.5, TOL);

    // At 7.0, at half stick, or under rac2: not yet.
    in.frame = 7.0f;
    CHECK_EQ(bm_clank_punch(&en, &in).steer, 0);
    in.frame = 7.5f;
    in.stick = 0.5f;
    CHECK_EQ(bm_clank_punch(&en, &in).steer, 0);
    in.stick = 1.0f;
    CHECK_EQ(bm_clank_punch(&r2, &in).steer, 0);

    // Still blending in, a wrench record, or rac1's own numbers: nothing.
    in.changing = 1;
    CHECK_EQ(bm_clank_punch(&en, &in).steer, 0);
    in.changing = 0;
    in.record = 0;
    CHECK_EQ(bm_clank_punch(&en, &in).steer, 0);
    in.record = 8;
    in.frame = 9.5f;
    CHECK_EQ(bm_clank_punch(&r1, &in).steer, 0);
    CHECK_EQ(bm_clank_punch(&en, &in).steer, 1);
}

/* The punch's lock-on turn. */
static void punch_turn(void) {
    const bm_tick t = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    const bm_clank_params r2 = bm_clank_params_for(BM_CLANK_RAC2);
    bm_spring in = {0.05f, 0.2f, 15.009831f / 60.0f};
    bm_spring out;

    // 15.01 radians a second becomes 16.93 (0.2821616 a tick).
    CHECK_EQ(bm_clank_punch_turn(&r2, in, &t, &out), 1);
    CHECK_NEAR(out.accel, 0.05, TOL);
    CHECK_NEAR(out.damp, 0.2, TOL);
    CHECK_NEAR(out.limit, 0.2821616, TOL);

    // Another turn: none.
    in.limit = 0.1f;
    CHECK_EQ(bm_clank_punch_turn(&r2, in, &t, &out), 0);

    // 50 Hz: the limit x 1.2, the spring x 1.44.
    in.accel = 0.05f * 1.44f;
    in.damp = 0.2f * 1.44f;
    in.limit = 15.009831f / 50.0f;
    CHECK_EQ(bm_clank_punch_turn(&r2, in, &pal, &out), 1);
    CHECK_NEAR(out.limit, 0.2821616 * 1.2, 1e-6);
}

int main(void) {
    same_in_both();
    params();
    run_turn();
    flags();
    run_speed();
    jump();
    square();
    punch();
    punch_turn();
    CHECK_DONE("clank");
}
