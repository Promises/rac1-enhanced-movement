/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests the wrench decisions (src/rac1/wrench.h) at 60 Hz and 50 Hz, against values worked out by
 * hand from both games' attack records and transitions: what each game does with a swing in a tick
 * and the steer that makes rac1 do what rac2 does, Square's windows on foot and in a jump, the
 * lock-on turn's arguments and the slam's drive.
 */
#include "check.h"
#include "rac1/wrench.h"

/* Returns a swing on its 20th tick with nothing pressed, the stick centred, no blend, on the
 * ground. */
static bm_swing_in swing(int state, int record, float frame) {
    bm_swing_in in = {0};
    in.state = state;
    in.record = record;
    in.frame = frame;
    in.timer = 20;
    return in;
}

/* Tests the attack records, rac1's and rac2's frames side by side. */
static void records(void) {
    const bm_attack a0 = bm_rac1_attack(0);
    const bm_attack b0 = bm_rac2_attack(0);
    const bm_attack b3 = bm_rac2_attack(3);
    const bm_attack none = bm_rac1_attack(4);

    // The first swing's frames are rac1's 19 / 26 / 24 / 31 and rac2's 19 / 25 / 24 / 26.
    CHECK_EQ(a0.first, 19);
    CHECK_EQ(a0.open, 26);
    CHECK_EQ(a0.exit, 31);
    CHECK_EQ(b0.open, 25);
    CHECK_EQ(b0.exit, 26);

    // The second ends at 15 in rac2, 23 in rac1.
    CHECK_EQ(bm_rac2_attack(1).exit, 15);
    CHECK_EQ(bm_rac1_attack(1).exit, 23);

    // The throw allows a jump after frame 35 in rac2 and 87 in rac1.
    CHECK_EQ(b3.jump, 35);
    CHECK_EQ(bm_rac1_attack(3).jump, 87);

    // A record the mod does not steer (the slam's, 4) is all zeros.
    CHECK_EQ(none.first + none.open + none.jump + none.exit, 0);
}

/* Tests the combo's swings at 60 Hz. */
static void combo(void) {
    const bm_tick t = bm_tick_ntsc();
    bm_swing_in in;
    bm_swing_steer s;

    // In the first swing at frame 25.5 with Square 5 polls back, rac2 chains ((25 - 19) * 2 = 12
    // polls from frame 25) but rac1 not before 26. The steer shows frame 26, where rac1's window is
    // (26 - 19) * 2 = 14.
    in = swing(0x13, 0, 25.5f);
    in.square = 5;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_CHAIN);
    CHECK_EQ(s.rac1, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 26.0, TOL);
    CHECK_EQ(s.hide_square, 0);

    // Square 13 polls back is outside rac2's 12, so both stay.
    in.square = 13;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // At frame 27 with a full stick rac2 runs (past its exit 26, stick past 0.8) and rac1 stays
    // until 31. The steer shows 31.5, half a frame past rac1's exit, with Square and X hidden (15
    // polls and the whole history).
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

    // While a new animation blends in, neither game leaves.
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

    // rac1 already stands on the way to rac2's run: nothing to steer, as the run comes next tick.
    in.stick = 1.0f;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_RUN);
    CHECK_EQ(s.rac1, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // In the second swing at 20.5 with the stick centred, rac2 stands (15 + 5 = 20) and rac1 not
    // before 23, so the steer shows 23.5.
    in = swing(0x13, 1, 20.5f);
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_FRAME);
    CHECK_NEAR(s.frame, 23.5, TOL);

    // In the third swing at 22 rac1 stands (past 21) but rac2 waits for 26, so the swing is held.
    in = swing(0x13, 2, 22.0f);
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_STAY);
    CHECK_EQ(s.rac1, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_HOLD);

    // Once the animation has ended, both stand.
    in.done = 1;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_IDLE);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // X in the first swing past frame 24 has the same window in both games, (25 - 19) * 2 = 12.
    in = swing(0x13, 0, 25.0f);
    in.cross = 12;
    s = bm_swing_steer_for(&in, &t);
    CHECK_EQ(s.rac2, BM_SWING_JUMP);
    CHECK_EQ(s.rac1, BM_SWING_JUMP);
    CHECK_EQ(s.kind, BM_STEER_NONE);

    // With the wrench out rac2 takes no Square, nor can rac1 before 26.
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

/* Tests the throw (state 0x15, record 3). */
static void throw_(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    bm_swing_in in;
    bm_swing_steer s;

    // At frame 36 with X 3 polls back rac2 jumps (past 35, within 8) but rac1 not before 87. The
    // steer shows frame 88, past 87, where rac1's window is (88 - 83) * 2 = 10, with Square hidden.
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

    // At frame 34.5 with the stick at 0.95 rac2 runs (past 34, past 0.9) while the wrench flies,
    // and rac1 stays until 91. The steer shows 91.5.
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

    // At frame 34 exactly rac2 is not yet past it.
    in.stick = 1.0f;
    in.frame = 34.0f;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_STAY);

    // The throw again, after 30 ticks, from frame 88, with R1 held and Square within 15, chains in
    // both games.
    in = swing(0x15, 3, 88.5f);
    in.timer = 31;
    in.crouch = 1;
    in.square = 15;
    s = bm_swing_steer_for(&in, &ntsc);
    CHECK_EQ(s.rac2, BM_SWING_CHAIN);
    CHECK_EQ(s.rac1, BM_SWING_CHAIN);
    CHECK_EQ(s.kind, BM_STEER_NONE);
}

/* Tests Square's windows for a first swing or the slam. */
static void windows(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    bm_reach_in in = {0};
    bm_windows w;

    // Standing, the windows are 7 and 10. At 50 Hz rac1 counts 8 (7 x 1.2 + 0.5, truncated) and
    // rac2's PAL release 8 ((10 x 5 + 2) / 6): the same, so nothing is buffered.
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 7);
    CHECK_EQ(w.rac2, 10);
    w = bm_wrench_windows(&in, &pal);
    CHECK_EQ(w.rac1, 8);
    CHECK_EQ(w.rac2, 8);

    // Slip ground has no window.
    in.slip = 1;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1 + w.rac2, 0);

    // In the jump after its 10th tick the windows are 10 and 14; on its 10th there are none.
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

    // At 50 Hz rac1's 10 ticks become 12, so on tick 13 the windows are rac1's 12 and rac2's 12
    // ((14 x 5 + 2) / 6).
    in.timer = 13;
    w = bm_wrench_windows(&in, &pal);
    CHECK_EQ(w.rac1, 12);
    CHECK_EQ(w.rac2, 12);

    // A landing on a rail in the jump's control mode counts as on foot.
    in.rail_land = 1;
    w = bm_wrench_windows(&in, &ntsc);
    CHECK_EQ(w.rac1, 7);
    CHECK_EQ(w.rac2, 10);

    // The double jump past frame 32 gives 18 in both games, and none before. At 50 Hz rac1 gives 22
    // and rac2 15.
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

    // A Square 8 to 10 polls back is inside rac2's window only.
    w.rac1 = 7;
    w.rac2 = 10;
    CHECK_EQ(bm_wrench_buffer(w, 8), 1);
    CHECK_EQ(bm_wrench_buffer(w, 10), 1);
    CHECK_EQ(bm_wrench_buffer(w, 7), 0);
    CHECK_EQ(bm_wrench_buffer(w, 11), 0);
    CHECK_EQ(bm_wrench_buffer(w, 0), 0);
}

/* Tests the lock-on turn and the slam's drive. */
static void turn_and_slam(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    bm_spring in = {0.05f, 0.2f, 15.009831f / 60.0f};
    bm_spring out = {0, 0, 0};

    // In the swings 0.05 / 0.2 / 15.01 rad/s become 0.05 / 0.2 / 0.2821616 a tick.
    CHECK_EQ(bm_wrench_turn(0x13, in, &ntsc, &out), 1);
    CHECK_NEAR(out.accel, 0.05, TOL);
    CHECK_NEAR(out.damp, 0.2, TOL);
    CHECK_NEAR(out.limit, 0.2821616, TOL);

    // The throw's accel is 0.057.
    CHECK_EQ(bm_wrench_turn(0x15, in, &ntsc, &out), 1);
    CHECK_NEAR(out.accel, 0.057, TOL);

    // At 50 Hz the spring is 0.05 x 1.44 = 0.072 and 0.2 x 1.44 = 0.288, the limit 0.2821616 x 1.2
    // = 0.33859392.
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

    // The slam drives on more than 4 units above the ground, in its first 70 ticks or toward a
    // target.
    CHECK_EQ(bm_slam_drive(4.5f, 0, 10, &ntsc), 1);
    CHECK_EQ(bm_slam_drive(4.0f, 0, 10, &ntsc), 0);
    CHECK_EQ(bm_slam_drive(5.0f, 0, 69, &ntsc), 1);
    CHECK_EQ(bm_slam_drive(5.0f, 0, 70, &ntsc), 0);
    CHECK_EQ(bm_slam_drive(5.0f, 1, 200, &ntsc), 1);

    // At 50 Hz the 70 ticks are rac2's PAL 58 ((70 x 5 + 2) / 6).
    CHECK_EQ(bm_slam_drive(5.0f, 0, 57, &pal), 1);
    CHECK_EQ(bm_slam_drive(5.0f, 0, 58, &pal), 0);

    // The drive of 4.5 u/s is 0.075 a tick at 60 Hz and 0.09 at 50.
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
