/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The strafe's decisions (src/rac1/strafe.h) against values worked out by hand from rac2's code:
 * sectors, the backpedal, the side timers' sequence, the flip, its drift landing where rac2's does
 * after rac1's own easing, the twist across the wrap, and the backward step of a looping animation.
 */
#include "check.h"
#include "rac1/strafe.h"

/* The stick's sectors, and the stick against the facing. */
static void sectors(void) {
    // Down is back, right is right, left is left, up is forward (the pad's y grows downward).
    CHECK_EQ(bm_stick_sector(0.0f, 1.0f), BM_BACK);
    CHECK_EQ(bm_stick_sector(1.0f, 0.0f), BM_RIGHT);
    CHECK_EQ(bm_stick_sector(-1.0f, 0.0f), BM_LEFT);
    CHECK_EQ(bm_stick_sector(0.0f, -1.0f), BM_FORWARD);

    // atan2(0.70, 0.71) = 44.6 degrees: 45.4 from down (not back), 44.6 from right: right.
    CHECK_EQ(bm_stick_sector(0.71f, 0.70f), BM_RIGHT);

    // atan2(0.71, 0.70) = 45.4 degrees: 44.6 from down, which is checked first: back.
    CHECK_EQ(bm_stick_sector(0.70f, 0.71f), BM_BACK);

    // The backpedal: back and past 0.15.
    CHECK_EQ(bm_backpedal(BM_BACK, 0.2f), 1);
    CHECK_EQ(bm_backpedal(BM_BACK, 0.1f), 0);
    CHECK_EQ(bm_backpedal(BM_RIGHT, 1.0f), 0);

    // Against his facing 1.0: behind, right (yaw less), left (yaw more), ahead.
    CHECK_EQ(bm_stick_vs_facing(1.0f + BM_PI, 1.0f), BM_BACK);
    CHECK_EQ(bm_stick_vs_facing(1.0f - BM_HALF_PI, 1.0f), BM_RIGHT);
    CHECK_EQ(bm_stick_vs_facing(1.0f + BM_HALF_PI, 1.0f), BM_LEFT);
    CHECK_EQ(bm_stick_vs_facing(1.2f, 1.0f), BM_FORWARD);
}

/* The side timers: right after left goes back by quarters, left after right reverses a quarter. */
static void sides(void) {
    bm_sides s = {0, 0};

    // Left: nothing to reverse; the left timer is set.
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), 1.0, TOL);
    CHECK_EQ(s.left, 5);

    // Then right: left counts 4 first, (4 - 4) / 4 = 0; then 3, 2, 1: 0.25, 0.5, 0.75; then 0: 1.
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.0, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.5, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.75, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 1.0, TOL);

    // Then left: the right timer (5) counts 4, 3, 2, 1 with -0.25 each tick, then 1.
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), 1.0, TOL);

    // Not on foot: 1, and the timers are not set.
    s.left = 0;
    s.right = 0;
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 0), 1.0, TOL);
    CHECK_EQ(s.left, 0);

    // Forward or back sets neither.
    CHECK_NEAR(bm_side_factor(&s, BM_BACK, 1), 1.0, TOL);
    CHECK_EQ(s.left + s.right, 0);
}

/* Standing, turning, flipping and the twist. */
static void moves(void) {
    // Still turning: 0.2 rad off against 10 degrees (0.17453295).
    CHECK_EQ(bm_strafe_turning(0.2f, 0.0f, RAC2_STRAFE_START_ANGLE), 1);
    CHECK_EQ(bm_strafe_turning(0.1f, 0.0f, RAC2_STRAFE_START_ANGLE), 0);

    // The standing start: 0.5 rad off with the stick at rest; not with 0.5 (rac1 starts by itself).
    CHECK_EQ(bm_strafe_start(0.5f, 0.0f, 0.0f), 1);
    CHECK_EQ(bm_strafe_start(0.5f, 0.0f, 0.5f), 0);
    CHECK_EQ(bm_strafe_start(0.1f, 0.0f, 0.0f), 0);

    // The flip: past 0.9 and not forward.
    CHECK_EQ(bm_flip(0.95f, BM_LEFT), 1);
    CHECK_EQ(bm_flip(0.95f, BM_BACK), 1);
    CHECK_EQ(bm_flip(0.95f, BM_FORWARD), 0);
    CHECK_EQ(bm_flip(0.85f, BM_LEFT), 0);

    // The twist: going 1.0 while facing -3.0 is 4.0 - 2 pi = -2.2831853 (the short way round).
    CHECK_NEAR(bm_twist(1.0f, -3.0f), -2.2831853, 1e-5);
    CHECK_NEAR(bm_twist(0.5f, 0.2f), 0.3, 1e-6);
    CHECK_EQ(bm_twist_on(0.5f, RAC1_STATE_RUN, 0), 1);
    CHECK_EQ(bm_twist_on(0.1f, RAC1_STATE_RUN, 0), 0);
    CHECK_EQ(bm_twist_on(0.5f, RAC1_STATE_HIGH_JUMP, 0), 0);
    CHECK_EQ(bm_twist_on(0.5f, RAC1_STATE_RUN, 1), 0);
}

/* The flip's drift: rac1's easing after the preset lands on rac2's value. */
static void drift(void) {
    const bm_tick t = bm_tick_ntsc();
    const float step1 = RAC1_FLIP_DRIFT_STEP * t.ft2;
    float d = bm_flip_drift_start(&t);
    float pre;

    // Start: 5 / 60 = 0.0833333.
    CHECK_NEAR(d, 0.0833333358, TOL);

    // Full stick: rac2 holds 5 / 60. The preset is 0.0833333 + 8 / 3600 = 0.0855556, which rac1 eases
    // toward 4.2 / 60 = 0.07 by 8 / 3600 back to 0.0833333.
    pre = bm_flip_drift_preset(d, 1.0f, 0, &t);
    CHECK_NEAR(pre, 0.0855555580, TOL);
    CHECK_NEAR(bm_approach(pre, RAC1_FLIP_DRIFT * t.ft, step1), 0.0833333358, TOL);

    // Stick 0.3: rac2's goal is the floor 2 / 60 = 0.0333333; one step of 37 / 3600 = 0.0102778
    // gives 0.0730556, over rac1's 0.07: the preset is 0.0752778 and rac1 lands on 0.0730556.
    pre = bm_flip_drift_preset(d, 0.3f, 0, &t);
    d = bm_approach(pre, RAC1_FLIP_DRIFT * t.ft, step1);
    CHECK_NEAR(d, 0.0730555570, TOL);

    // The next tick: 0.0627778, now under rac1's 0.07: the preset is 0.0605556, which rac1 eases
    // up to 0.0627778.
    pre = bm_flip_drift_preset(d, 0.3f, 0, &t);
    CHECK_NEAR(pre, 0.0605555574, TOL);
    CHECK_NEAR(bm_approach(pre, RAC1_FLIP_DRIFT * t.ft, step1), 0.0627777791, TOL);

    // Near landing rac1 eases to 0: from 0.0627778 rac2 gives 0.0525, so the preset is 0.0547222.
    pre = bm_flip_drift_preset(0.0627778f, 0.3f, 1, &t);
    CHECK_NEAR(bm_approach(pre, 0.0f, step1), 0.0525000, TOL);
}

/* The backward step of a looping animation, and the sound triggers it goes over. */
static void anim(void) {
    const float even[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float mixed[4] = {1.0f, 2.0f, 0.5f, 1.0f};
    bm_anim a;

    // Within a key: 0.6 back by 0.5 is 0.1.
    a.time = 0.6f;
    a.frame = 2;
    a.next = 3;
    CHECK_EQ(bm_anim_step_back(&a, 0.5f, even, 4), 0);
    CHECK_NEAR(a.time, 0.1, TOL);

    // Past a key: 0.3 - 0.5 = -0.2 is key 1's 0.8, toward key 2.
    a.time = 0.3f;
    a.frame = 2;
    a.next = 3;
    CHECK_EQ(bm_anim_step_back(&a, 0.5f, even, 4), 1);
    CHECK_EQ(a.frame, 1);
    CHECK_EQ(a.next, 2);
    CHECK_NEAR(a.time, 0.8, TOL);

    // Rates: at key 1 (rate 2) 0.1 - 0.5 x 2 = -0.9; at key 0 (rate 1) that is -0.9 / 2 + 1 = 0.55.
    a.time = 0.1f;
    a.frame = 1;
    a.next = 2;
    bm_anim_step_back(&a, 0.5f, mixed, 4);
    CHECK_EQ(a.frame, 0);
    CHECK_NEAR(a.time, 0.55, TOL);

    // Across the loop's start: key 0's 0.1 - 0.5 is key 3's 0.6, toward key 0.
    a.time = 0.1f;
    a.frame = 0;
    a.next = 1;
    bm_anim_step_back(&a, 0.5f, even, 4);
    CHECK_EQ(a.frame, 3);
    CHECK_EQ(a.next, 0);
    CHECK_NEAR(a.time, 0.6, TOL);

    // Places: key 2 at 0.5 is 40.
    a.time = 0.5f;
    a.frame = 2;
    CHECK_EQ(bm_anim_place(&a), 40);

    // From 40 back to 30: a trigger at 35 or 40 is gone over, one at 30 or 41 is not.
    CHECK_EQ(bm_anim_crossed_back(40, 30, 35, 0), 1);
    CHECK_EQ(bm_anim_crossed_back(40, 30, 40, 0), 1);
    CHECK_EQ(bm_anim_crossed_back(40, 30, 30, 0), 0);
    CHECK_EQ(bm_anim_crossed_back(40, 30, 41, 0), 0);

    // From 5 back across the start to 60: 3 and 62 are gone over, 30 is not.
    CHECK_EQ(bm_anim_crossed_back(5, 60, 3, 1), 1);
    CHECK_EQ(bm_anim_crossed_back(5, 60, 62, 1), 1);
    CHECK_EQ(bm_anim_crossed_back(5, 60, 30, 1), 0);
}

int main(void) {
    sectors();
    sides();
    moves();
    drift();
    anim();
    CHECK_DONE("strafe");
}
