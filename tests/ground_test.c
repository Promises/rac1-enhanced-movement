/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The run's decisions (src/rac1/ground.h) against values worked out by hand from rac2's literals
 * (src/rac1/rac2.h). At NTSC's tick a rac2 per-tick value is used as it is (x 60 x 1/60); at PAL's
 * (1/50 s) a speed or turn limit is x 1.2 and a turn coefficient x 1.44 (g_frame_scale_sq).
 */
#include "check.h"
#include "rac1/ground.h"

/* A run input with everything at rest but what a test sets. */
static bm_run_in run(float stick, float yaw_error, float flat, int step) {
    bm_run_in in = {0};
    in.stick = stick;
    in.yaw_error = yaw_error;
    in.flat_speed = flat;
    in.step = step;
    return in;
}

/* The turn's three branches and the firing turn, at NTSC's tick. */
static void turns(void) {
    const bm_tick t = bm_tick_ntsc();
    bm_run_flags none = {0, 0};
    bm_run_flags around = {0, 1};
    bm_run_in in;
    bm_turn turn;

    // The run band: 0.02 / 0.17 / 0.16580631 (9.95 rad/s).
    in = run(1.0f, 0.0f, 0.09f, 1);
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.02, TOL);
    CHECK_NEAR(turn.damp, 0.17, TOL);
    CHECK_NEAR(turn.limit, 0.16580631, TOL);

    // The walk band at full stick: 0.022 / 0.27 / 0.093084246 (5.585 rad/s).
    in = run(1.0f, 0.0f, 0.05f, 0);
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.022, TOL);
    CHECK_NEAR(turn.damp, 0.27, TOL);
    CHECK_NEAR(turn.limit, 0.093084246, TOL);

    // Stick 0.8: accel 0.8 x 0.02 = 0.016, limit 0.8 x 0.063995413 = 0.05119633.
    in = run(0.8f, 0.0f, 0.05f, 0);
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.016, TOL);
    CHECK_NEAR(turn.limit, 0.05119633, TOL);

    // Stick 0.3: 0.006 and 0.0191987 are under the floors 0.01 and 0.031997707.
    in = run(0.3f, 0.0f, 0.05f, 0);
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.01, TOL);
    CHECK_NEAR(turn.limit, 0.031997707, TOL);

    // The turnaround, even in the run band: 0.035 / 0.27 / 0.19489513 (11.69 rad/s).
    in = run(1.0f, 2.5f, 0.09f, 1);
    turn = bm_run_turn(&in, &around, &t);
    CHECK_NEAR(turn.accel, 0.035, TOL);
    CHECK_NEAR(turn.damp, 0.27, TOL);
    CHECK_NEAR(turn.limit, 0.19489513, TOL);

    // Firing at full stick: 0.017 / 0.18 / 0.14544412.
    in = run(1.0f, 0.0f, 0.05f, 0);
    in.firing = 1;
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.017, TOL);
    CHECK_NEAR(turn.damp, 0.18, TOL);
    CHECK_NEAR(turn.limit, 0.14544412, TOL);

    // Firing at 0.6: 0.6 x 0.009 = 0.0054, 0.15, 0.6 x 0.078539826 = 0.047123896.
    in = run(0.6f, 0.0f, 0.05f, 0);
    in.firing = 1;
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.0054, TOL);
    CHECK_NEAR(turn.damp, 0.15, TOL);
    CHECK_NEAR(turn.limit, 0.047123896, TOL);

    // An item's lock takes the run band's turn away: the walk band's at full stick.
    in = run(1.0f, 0.0f, 0.09f, 1);
    in.item_flag = 1;
    turn = bm_run_turn(&in, &none, &t);
    CHECK_NEAR(turn.accel, 0.022, TOL);
}

/* The turn at PAL's tick, scaled as rac1 scales its own. */
static void pal_turn(void) {
    const bm_tick t = bm_tick_pal();
    bm_run_flags none = {0, 0};
    bm_run_in in = run(1.0f, 0.0f, 0.09f, 1);
    bm_turn turn = bm_run_turn(&in, &none, &t);

    // 0.02 x 1.44 = 0.0288; 0.17 x 1.44 = 0.2448; 0.16580631 x 60 / 50 = 0.198967572.
    CHECK_NEAR(turn.accel, 0.0288, TOL);
    CHECK_NEAR(turn.damp, 0.2448, TOL);
    CHECK_NEAR(turn.limit, 0.198967572, TOL);
}

/* k: no speeding up while turning hard, but in the run band. */
static void k_factor(void) {
    bm_run_flags none = {0, 0};
    bm_run_flags around = {0, 1};
    bm_run_in in;

    // Half stick, 1 rad off (over 45 degrees): 0.
    in = run(0.5f, 1.0f, 0.05f, 0);
    CHECK_NEAR(bm_run_k(&in, &none), 0.0, TOL);

    // Full stick: 1, however far off.
    in = run(1.0f, 1.0f, 0.05f, 0);
    CHECK_NEAR(bm_run_k(&in, &none), 1.0, TOL);

    // The run band: 1 at half stick.
    in = run(0.5f, 1.0f, 0.09f, 1);
    CHECK_NEAR(bm_run_k(&in, &none), 1.0, TOL);

    // An item's lock at full stick: 0; in a turnaround: 1.
    in = run(1.0f, 1.0f, 0.05f, 0);
    in.item_flag = 1;
    CHECK_NEAR(bm_run_k(&in, &none), 0.0, TOL);
    CHECK_NEAR(bm_run_k(&in, &around), 1.0, TOL);

    // Under 45 degrees: 1.
    in = run(0.5f, 0.7f, 0.05f, 0);
    CHECK_NEAR(bm_run_k(&in, &none), 1.0, TOL);
}

/* The target speed and the step's up and down. */
static void speeds(void) {
    const bm_tick t = bm_tick_ntsc();
    bm_run_flags none = {0, 0};
    bm_run_flags around = {0, 1};
    bm_run_in in;
    bm_speed s;

    // Full stick: 5.95 / 60 = 0.0991666667; up 0.005, down 0.0055555557.
    in = run(1.0f, 0.0f, 0.05f, 0);
    s = bm_run_speed(&in, &none, 1.0f, &t);
    CHECK_NEAR(s.target, 0.0991666667, TOL);
    CHECK_NEAR(s.up, 0.005, TOL);
    CHECK_NEAR(s.down, 0.0055555557, TOL);

    // Half stick: 0.0495833333; k 0 takes the up away.
    in = run(0.5f, 0.0f, 0.05f, 0);
    s = bm_run_speed(&in, &none, 0.0f, &t);
    CHECK_NEAR(s.target, 0.0495833333, TOL);
    CHECK_NEAR(s.up, 0.0, TOL);

    // Under 0.2: no target.
    in = run(0.15f, 0.0f, 0.05f, 0);
    s = bm_run_speed(&in, &none, 1.0f, &t);
    CHECK_NEAR(s.target, 0.0, TOL);

    // Wading at 0.3: 0.02975 x 0.8 = 0.0238, under the floor 0.041666668.
    in = run(0.3f, 0.0f, 0.05f, 0);
    in.wading = 1;
    s = bm_run_speed(&in, &none, 1.0f, &t);
    CHECK_NEAR(s.target, 0.041666668, TOL);

    // Wading at full stick: 0.0991666667 x 0.8 = 0.0793333333.
    in = run(1.0f, 0.0f, 0.05f, 0);
    in.wading = 1;
    s = bm_run_speed(&in, &none, 1.0f, &t);
    CHECK_NEAR(s.target, 0.0793333333, TOL);

    // Turnaround at +2.0 rad: cap 0.09916667 - 2 x 0.029178405 = 0.04080986, then x (2.617994 - 2.0)
    // = 0.617994: 0.025220024; down 0.004722222.
    in = run(1.0f, 2.0f, 0.05f, 0);
    s = bm_run_speed(&in, &around, 1.0f, &t);
    CHECK_NEAR(s.target, 0.025220024, TOL);
    CHECK_NEAR(s.down, 0.004722222, TOL);

    // Turnaround at -2.0 rad: the same cap; the scale 2.617994 + 2 clamps to 1: 0.04080986.
    in = run(1.0f, -2.0f, 0.05f, 0);
    s = bm_run_speed(&in, &around, 1.0f, &t);
    CHECK_NEAR(s.target, 0.04080986, TOL);

    // Turnaround at 3.5 rad: the cap is under 0, so 0.
    in = run(1.0f, 3.5f, 0.05f, 0);
    s = bm_run_speed(&in, &around, 1.0f, &t);
    CHECK_NEAR(s.target, 0.0, TOL);

    // Half stick 1 rad off (over 35 degrees): 0.0495833333 x (1 - 0.4) = 0.02975.
    in = run(0.5f, 1.0f, 0.05f, 0);
    s = bm_run_speed(&in, &none, 1.0f, &t);
    CHECK_NEAR(s.target, 0.02975, TOL);

    // Half stick 3 rad off: 1 - 1.2 is under 0, so 0.
    in = run(0.5f, 3.0f, 0.05f, 0);
    s = bm_run_speed(&in, &none, 1.0f, &t);
    CHECK_NEAR(s.target, 0.0, TOL);
}

/* The flags along and turnaround. */
static void flags(void) {
    const bm_tick t = bm_tick_ntsc();
    bm_run_flags f = {0, 0};
    bm_run_in in;

    // Slow (0.02 under 0.028333336) with a full stick 2.2 rad off (over 2.0943954): both.
    in = run(1.0f, 2.2f, 0.02f, 0);
    bm_run_flags_update(&f, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 1);

    // Facing the stick within 0.05 rad: along ends (under 5 degrees), the turnaround (under 4).
    in = run(1.0f, 0.05f, 0.09f, 0);
    bm_run_flags_update(&f, &in, &t);
    CHECK_EQ(f.along, 0);
    CHECK_EQ(f.turnaround, 0);

    // Slow with a part stick far off: along only.
    in = run(0.6f, 2.2f, 0.02f, 0);
    bm_run_flags_update(&f, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 0);

    // Fast, the stick swung 2.0 rad (over 1.9198624) at full stick 1.3 rad off (over 1.2217306): both.
    f.along = 0;
    in = run(1.0f, 1.3f, 0.09f, 0);
    in.stick_swing = 2.0f;
    bm_run_flags_update(&f, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 1);

    // Strafing: nothing is set, along is kept; the turnaround still ends under 4 degrees.
    f.along = 1;
    f.turnaround = 1;
    in = run(1.0f, 0.0f, 0.02f, 0);
    in.strafing = 1;
    bm_run_flags_update(&f, &in, &t);
    CHECK_EQ(f.along, 1);
    CHECK_EQ(f.turnaround, 0);
}

/* Braking, the exit and the jump rule. */
static void stopping(void) {
    const bm_tick t = bm_tick_ntsc();

    // STOP: 0.0075000003 x 3600 / 12 = 2.25; crouch 27 / 6.48 = 4.1666667; ice 1; idle 17.7 / 12.6.
    CHECK_NEAR(bm_brake_scale(RAC1_STATE_STOP, 0), 2.25, 1e-5);
    CHECK_NEAR(bm_brake_scale(RAC1_STATE_CROUCH, 0), 4.1666667, 1e-5);
    CHECK_NEAR(bm_brake_scale(RAC1_STATE_CROUCH, 1), 1.0, TOL);
    CHECK_NEAR(bm_brake_scale(RAC1_STATE_IDLE, 0), 1.4047619, 1e-5);
    CHECK_NEAR(bm_brake_scale(RAC1_STATE_LOOK_RELEASE_WAIT, 0), 1.4047619, 1e-5);
    CHECK_NEAR(bm_brake_scale(RAC1_STATE_RUN, 0), 1.0, TOL);
    CHECK_NEAR(bm_brake_scale(0x3D, 0), 1.0, TOL);

    // The exit: under 0.17, and while strafing only within 2 degrees (0.0349 rad) of the camera.
    CHECK_EQ(bm_run_exit(0.1f, 0, 1.0f), 1);
    CHECK_EQ(bm_run_exit(0.2f, 0, 0.0f), 0);
    CHECK_EQ(bm_run_exit(0.1f, 1, 0.05f), 0);
    CHECK_EQ(bm_run_exit(0.1f, 1, 0.02f), 1);
    CHECK_NEAR(bm_exit_clamp(&t), 0.025000002, TOL);

    // The running jump: past 0.85 and not strafing.
    CHECK_EQ(bm_rac2_running_jump(0.9f, 0), 1);
    CHECK_EQ(bm_rac2_running_jump(0.9f, 1), 0);
    CHECK_EQ(bm_rac2_running_jump(0.85f, 0), 0);

    // rac1's own down: 8.5 / 3600 and 17 / 3600.
    CHECK_NEAR(bm_rac1_run_down(0, &t), 0.0023611111, TOL);
    CHECK_NEAR(bm_rac1_run_down(1, &t), 0.0047222222, TOL);
}

/* rac1's tick counts as scale_ticks gives them: n * frame_scale + 0.5, cut. */
static void ticks(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();

    // NTSC: frame_scale 1, the counts as written.
    CHECK_EQ(bm_scale_ticks(9, &ntsc), 9);
    CHECK_EQ(bm_scale_ticks(25, &ntsc), 25);

    // PAL: frame_scale 1.2. 9 x 1.2 + 0.5 = 11.3: 11; 7: 8.9: 8; 25: 30.5: 30; 12: 14.9: 14; 4: 5.3: 5.
    CHECK_EQ(bm_scale_ticks(9, &pal), 11);
    CHECK_EQ(bm_scale_ticks(7, &pal), 8);
    CHECK_EQ(bm_scale_ticks(25, &pal), 30);
    CHECK_EQ(bm_scale_ticks(12, &pal), 14);
    CHECK_EQ(bm_scale_ticks(4, &pal), 5);
}

int main(void) {
    ticks();
    turns();
    pal_turn();
    k_factor();
    speeds();
    flags();
    stopping();
    CHECK_DONE("ground");
}
