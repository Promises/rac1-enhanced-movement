/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The wrench's decisions (src/rac1/wrench.h) against values worked out by hand from both games'
 * attack records and transitions: what rac2 and rac1 do with a swing in a tick and the steer that
 * makes rac1 do rac2's, Square's windows on foot and in a jump, the lock-on turn's arguments and
 * the slam's drive, at 60 Hz and at 50 Hz.
 */
#include "check.h"
#include "rac1/wrench.h"

/* A swing with nothing pressed, the stick centred, no blend, on the ground. */
static bm_swing_in swing(int state, int record, float frame) {
    bm_swing_in in = {0};
    in.state = state;
    in.record = record;
    in.frame = frame;
    in.timer = 20;
    return in;
}

/* The records: rac1's and rac2's frames side by side. */
static void records(void) {
    const bm_attack a0 = bm_rac1_attack(0);
    const bm_attack b0 = bm_rac2_attack(0);
    const bm_attack b3 = bm_rac2_attack(3);
    const bm_attack none = bm_rac1_attack(4);

    // The first swing: rac1 19 / 26 / 24 / 31, rac2 19 / 25 / 24 / 26.
    CHECK_EQ(a0.first, 19);
    CHECK_EQ(a0.open, 26);
    CHECK_EQ(a0.exit, 31);
    CHECK_EQ(b0.open, 25);
    CHECK_EQ(b0.exit, 26);

    // The second ends at 15 in rac2, 23 in rac1.
    CHECK_EQ(bm_rac2_attack(1).exit, 15);
    CHECK_EQ(bm_rac1_attack(1).exit, 23);

    // The throw: rac2's jump from 35, rac1's from 87.
    CHECK_EQ(b3.jump, 35);
    CHECK_EQ(bm_rac1_attack(3).jump, 87);

    // A record the mod does not steer (the slam's, 4) is all zeros.
    CHECK_EQ(none.first + none.open + none.jump + none.exit, 0);
}

/* The combo's swings at 60 Hz. */
static void combo(void) {
    const bm_tick t = bm_tick_ntsc();
    bm_swing_in in;
    bm_swing_steer s;

    // First swing at frame 25.5, Square 5 polls back: rac2 chains ((25 - 19) * 2 = 12 polls from 25),
    // rac1 not before 26. The steer: frame 26, where rac1's window is (26 - 19) * 2 = 14.
    in = swing(0x13, 0, 25.5f);
    in.square = 5;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_CHAIN);
    CHECK_EQ(s.rac1, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 26.0, TOL);
    CHECK_EQ(s.hide_square, 0);

    // Square 13 polls back is outside rac2's 12: both stay.
    in.square = 13;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // Frame 27 with a full stick: rac2 runs (past its exit 26, stick past 0.8), rac1 stays until 31.
    // The steer: just past rac1's exit, 31.5, with Square and X hidden (15 polls and the history).
    in = swing(0x13, 0, 27.0f);
    in.stick = 1.0f;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_RUN);
    CHECK_EQ(s.rac1, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 31.5, TOL);
    CHECK_EQ(s.hide_square, 15);
    CHECK_EQ(s.hide_cross, 30);

    // A stick of 0.8 is not past 0.8: rac2 waits for 26 + 5 = 31, as rac1 does.
    in.stick = 0.8f;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // A new animation blending in: neither leaves.
    in.stick = 1.0f;
    in.changing = 1;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // Past 31 both stand.
    in = swing(0x13, 0, 31.5f);
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_IDLE);
    CHECK_EQ(s.rac1, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // Already standing (rac1) on the way to rac2's run: nothing to steer, the run comes next tick.
    in.stick = 1.0f;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_RUN);
    CHECK_EQ(s.rac1, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // Second swing at 20.5, stick centred: rac2 stands (15 + 5 = 20), rac1 not before 23: frame 23.5.
    in = swing(0x13, 1, 20.5f);
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 23.5, TOL);

    // Third swing at 22: rac1 stands (past 21), rac2 waits for 26: held.
    in = swing(0x13, 2, 22.0f);
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.rac1, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_HOLD);

    // ... unless the animation came to its end: both stand.
    in.done = 1;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // X in the first swing past 24: the same window in both games, (25 - 19) * 2 = 12 polls.
    in = swing(0x13, 0, 25.0f);
    in.cross = 12;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_JUMP);
    CHECK_EQ(s.rac1, BM_SWING_JUMP);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // The wrench out: rac2 takes no Square (rac1 cannot either before 26).
    in = swing(0x13, 0, 25.5f);
    in.square = 5;
    in.out = 1;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // The slam and other states are not steered.
    in = swing(0x14, 4, 40.0f);
    in.stick = 1.0f;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.kind, BM_STEER_NONE);
    CHECK_EQ(s.rac2, s.rac1);
}

/* The throw (state 0x15, record 3). */
static void throw_(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    bm_swing_in in;
    bm_swing_steer s;

    // Frame 36, X 3 polls back: rac2 jumps (past 35, within 8); rac1 not before 87. The steer:
    // frame 88, past 87, where rac1's window is (88 - 83) * 2 = 10, with Square hidden.
    in = swing(0x15, 3, 36.0f);
    in.cross = 3;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_JUMP);
    CHECK_EQ(s.rac1, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 88.0, TOL);
    CHECK_EQ(s.hide_square, 15);
    CHECK_EQ(s.hide_cross, 0);

    // X 9 polls back is outside rac2's 8 at 60 Hz.
    in.cross = 9;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_STAY);

    // At 50 Hz rac2's PAL release counts 7 polls ((8 x 5 + 2) / 6): 8 back is outside, 7 inside.
    in.cross = 8;
    s = bm_swing_steer_for(&in, &pal);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    in.cross = 7;
    s = bm_swing_steer_for(&in, &pal);
    CHECK_EQ(s.rac2, BM_SWING_JUMP);
    CHECK_EQ(s.kind, BM_STEER_FRAME);

    // On magnet ground (unk308) the throw takes no jump in either game.
    in.cross = 3;
    in.unk308 = 1;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // Frame 34.5 with the stick at 0.95: rac2 runs (past 34, past 0.9) while the wrench flies; rac1
    // stays until 91. The steer: 91.5.
    in = swing(0x15, 3, 34.5f);
    in.stick = 0.95f;
    in.out = 1;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_RUN);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 91.5, TOL);

    // At 0.85 rac2 waits for the catch too.
    in.stick = 0.85f;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // At 34 exactly, not past it.
    in.stick = 1.0f;
    in.frame = 34.0f;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_STAY);

    // The throw again: after 30 ticks, from 88, R1 held, Square within 15: both.
    in = swing(0x15, 3, 88.5f);
    in.timer = 31;
    in.crouch = 1;
    in.square = 15;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_CHAIN);
    CHECK_EQ(s.rac1, BM_SWING_CHAIN);
    CHECK_EQ(s.kind, BM_STEER_NONE);
}

/* Square's windows for a first swing or the slam. */
static void windows(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    bm_reach_in in = {0};
    bm_windows w;

    // Standing: 7 and 10. At 50 Hz rac1 counts 8 (7 x 1.2 + 0.5, cut) and rac2's PAL release 8
    // ((10 x 5 + 2) / 6): the same, nothing to buffer.
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 7);
    CHECK_EQ(w.rac2, 10);
    w = bm_wrench_windows(&in, &pal);
    CHECK_EQ(w.rac1, 8);
    CHECK_EQ(w.rac2, 8);

    // On slip ground, none.
    in.slip = 1;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1 + w.rac2, 0);

    // The jump after its 10th tick: 10 and 14; on its 10th, none.
    in.slip = 0;
    in.control_mode = RAC1_MODE_JUMP;
    in.state = RAC1_STATE_JUMP;
    in.timer = 11;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 10);
    CHECK_EQ(w.rac2, 14);
    in.timer = 10;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1 + w.rac2, 0);

    // At 50 Hz rac1's 10 ticks are 12: tick 13 takes rac1's 12 and rac2's 12 ((14 x 5 + 2) / 6).
    in.timer = 13;
    w = bm_wrench_windows(&in, &pal);
    CHECK_EQ(w.rac1, 12);
    CHECK_EQ(w.rac2, 12);

    // Landed on a rail in the jump's mode: as on foot.
    in.rail_land = 1;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 7);
    CHECK_EQ(w.rac2, 10);

    // The double jump past frame 32: 18 in both; before it, none. At 50 Hz rac1 22, rac2 15.
    in.rail_land = 0;
    in.state = RAC1_STATE_DOUBLE_JUMP;
    in.timer = 20;
    in.frame = 33.0f;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 18);
    CHECK_EQ(w.rac2, 18);
    w = bm_wrench_windows(&in, &pal);
    CHECK_EQ(w.rac1, 22);
    CHECK_EQ(w.rac2, 15);
    in.frame = 30.0f;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1 + w.rac2, 0);

    // The high jump with unk450 3 waits for frame 20.
    in.state = RAC1_STATE_HIGH_JUMP;
    in.unk450 = 3;
    in.frame = 19.0f;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 0);
    in.frame = 21.0f;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 18);

    // A Square 8 to 10 polls back is rac2's only.
    w.rac1 = 7;
    w.rac2 = 10;
    CHECK_EQ(bm_wrench_buffer(w, 8), 1);
    CHECK_EQ(bm_wrench_buffer(w, 10), 1);
    CHECK_EQ(bm_wrench_buffer(w, 7), 0);
    CHECK_EQ(bm_wrench_buffer(w, 11), 0);
    CHECK_EQ(bm_wrench_buffer(w, 0), 0);
}

/* The lock-on turn and the slam's drive. */
static void turn_and_slam(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    bm_spring in = {0.05f, 0.2f, 15.009831f / 60.0f};
    bm_spring out = {0, 0, 0};

    // The swings: 0.05 / 0.2 / 15.01 a second become 0.05 / 0.2 / 0.2821616 a tick.
    CHECK_EQ(bm_wrench_turn(0x13, in, &ntsc, &out), 1);
    CHECK_NEAR(out.accel, 0.05, TOL);
    CHECK_NEAR(out.damp, 0.2, TOL);
    CHECK_NEAR(out.limit, 0.2821616, TOL);

    // The throw's accel is 0.057.
    CHECK_EQ(bm_wrench_turn(0x15, in, &ntsc, &out), 1);
    CHECK_NEAR(out.accel, 0.057, TOL);

    // At 50 Hz: 0.05 x 1.44 = 0.072, 0.2 x 1.44 = 0.288, 0.2821616 x 1.2 = 0.33859392.
    in.accel = 0.05f * 1.44f;
    in.damp = 0.2f * 1.44f;
    in.limit = 15.009831f / 50.0f;
    CHECK_EQ(bm_wrench_turn(0x14, in, &pal, &out), 1);
    CHECK_NEAR(out.accel, 0.072, TOL);
    CHECK_NEAR(out.damp, 0.288, TOL);
    CHECK_NEAR(out.limit, 0.33859392, TOL);

    // The ice's turn (0.02 / 0.2 / 3.665) and a call scaled by another mod are not this turn.
    in.accel = 0.02f;
    in.damp = 0.2f;
    in.limit = 3.66519141f / 60.0f;
    CHECK_EQ(bm_wrench_turn(0x13, in, &ntsc, &out), 0);
    in.accel = 0.05f;
    in.limit = 2.0f * 15.009831f / 60.0f;
    CHECK_EQ(bm_wrench_turn(0x13, in, &ntsc, &out), 0);

    // The slam's drive: more than 4 above the ground, in its first 70 ticks or toward a target.
    CHECK_EQ(bm_slam_drive(4.5f, 0, 10, &ntsc), 1);
    CHECK_EQ(bm_slam_drive(4.0f, 0, 10, &ntsc), 0);
    CHECK_EQ(bm_slam_drive(5.0f, 0, 69, &ntsc), 1);
    CHECK_EQ(bm_slam_drive(5.0f, 0, 70, &ntsc), 0);
    CHECK_EQ(bm_slam_drive(5.0f, 1, 200, &ntsc), 1);

    // At 50 Hz the 70 ticks are rac2's PAL 58 ((70 x 5 + 2) / 6).
    CHECK_EQ(bm_slam_drive(5.0f, 0, 57, &pal), 1);
    CHECK_EQ(bm_slam_drive(5.0f, 0, 58, &pal), 0);

    // 4.5 units a second: 0.075 a tick at 60 Hz, 0.09 at 50.
    CHECK_NEAR(bm_slam_drive_speed(&ntsc), 0.075, TOL);
    CHECK_NEAR(bm_slam_drive_speed(&pal), 0.09, TOL);
}

int main(void) {
    records();
    combo();
    throw_();
    windows();
    turn_and_slam();
    CHECK_DONE("wrench");
}
