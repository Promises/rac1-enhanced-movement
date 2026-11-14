/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The camera's decisions (src/rac1/camera.h) against values worked out by hand from both games' speed
 * tables: the gain each choice gives the follow camera, the pitch's steps, when the stick turns the
 * camera, modern's spring and its window, at 60 Hz and at 50 Hz. Then rac1's follow_camera_orbit as
 * far as the yaw goes (the orbit turned by the gain, the eye after it through rope_spring_turn), to
 * check what each choice feels like: the ramp and the stop the play-test measured (PLAYTEST.md).
 */
#include <math.h>

#include "check.h"
#include "rac1/camera.h"

#define DEG (3.14159265358979 / 180.0)

/* The tables: rac1's 1.0, 1.3, 1.6 degrees a tick, rac2's 1.0, 1.3, 2.2. */
static void tables(void) {
    CHECK_NEAR(bm_rac1_camera_speed(0) / DEG, 1.0, 1e-5);
    CHECK_NEAR(bm_rac1_camera_speed(1) / DEG, 1.3, 1e-5);
    CHECK_NEAR(bm_rac1_camera_speed(2) / DEG, 1.6, 1e-5);
    CHECK_NEAR(bm_rac2_camera_speed(0) / DEG, 1.0, 1e-5);
    CHECK_NEAR(bm_rac2_camera_speed(1) / DEG, 1.3, 1e-5);
    CHECK_NEAR(bm_rac2_camera_speed(2) / DEG, 2.2, 1e-5);

    // No fourth step.
    CHECK_NEAR(bm_rac1_camera_speed(3), 0.0, TOL);
    CHECK_NEAR(bm_rac2_camera_speed(-1), 0.0, TOL);

    // The rest of rac2's turn is rac1's: the pitch's steps and the eye's springs, within a float.
    CHECK_NEAR(RAC2_CAMERA_PITCH_TRACKING, RAC1_CAMERA_PITCH_TRACKING, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_RETURN, RAC1_CAMERA_PITCH_RETURN, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_STEP, RAC1_CAMERA_PITCH_STEP, 1e-8);
    CHECK_NEAR(RAC2_CAMERA_YAW_SPRING_ACCEL, RAC1_CAMERA_SPRING_ACCEL, TOL);
    CHECK_NEAR(RAC2_CAMERA_YAW_SPRING_DAMP, RAC1_CAMERA_SPRING_DAMP, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_SPRING_ACCEL, RAC1_CAMERA_SPRING_ACCEL, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_SPRING_DAMP, RAC1_CAMERA_SPRING_DAMP, TOL);
}

/* The gain each choice gives. */
static void gains(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    const float slow = RAC1_CAMERA_SPEED_SLOW;
    const float medium = RAC1_CAMERA_SPEED_MEDIUM;
    const float fast = RAC1_CAMERA_SPEED_FAST;

    // The first game's: as it came.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC1, fast, 2, &ntsc) == fast, 1);

    // Going Commando's: its fastest step on every step of the game's option, 2.2 degrees a tick, 132
    // a second.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, slow, 0, &ntsc), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, medium, 1, &ntsc), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, fast, 2, &ntsc), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, medium, 1, &ntsc) * 60.0 / DEG, 132.0, 1e-3);

    // Another mod doubled rac1's: rac2's doubled too.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, 2.0f * fast, 2, &ntsc), 0.0767945, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, 2.0f * medium, 1, &ntsc), 0.0767945, TOL);

    // An option rac1's table does not have: as it came.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, 0.05f, 7, &ntsc) == 0.05f, 1);

    // Modern: three times the game's, 1.3 x 3 = 3.9 degrees a tick, 234 degrees a second; 180 and
    // 288 on the other steps.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_MODERN, medium, 1, &ntsc), 0.06806784, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_MODERN, medium, 1, &ntsc) * 60.0 / DEG, 234.0, 1e-3);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_MODERN, slow, 0, &ntsc) * 60.0 / DEG, 180.0, 1e-3);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_MODERN, fast, 2, &ntsc) * 60.0 / DEG, 288.0, 1e-3);

    // At 50 Hz rac1 turns 1.3 degrees a tick still (65 a second); modern keeps 234 a second: 4.68
    // degrees a tick.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_MODERN, medium, 1, &pal) / DEG, 4.68, 1e-4);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_MODERN, medium, 1, &pal) * 50.0 / DEG, 234.0, 1e-3);

    // Going Commando's at 50 Hz: per tick as rac1 counts it, its fastest step.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, medium, 1, &pal), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, fast, 2, &pal), 0.03839725, TOL);

    // A gain that is no number, 0 or negative: left alone.
    CHECK_EQ(isnan(bm_camera_gain(BM_CAMERA_MODERN, NAN, 1, &ntsc)), 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_MODERN, 0.0f, 1, &ntsc) == 0.0f, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, -0.03f, 2, &ntsc) == -0.03f, 1);
}

/* The pitch's steps: modern three times as quick, the others as they came. */
static void pitch(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();

    // 0.02 out, 0.01 back, 1.75 degrees: 0.06, 0.03, 5.25 degrees.
    CHECK_NEAR(
        bm_camera_pitch_step(BM_CAMERA_MODERN, RAC1_CAMERA_PITCH_TRACKING, &ntsc), 0.06, TOL
    );
    CHECK_NEAR(bm_camera_pitch_step(BM_CAMERA_MODERN, RAC1_CAMERA_PITCH_RETURN, &ntsc), 0.03, TOL);
    CHECK_NEAR(
        bm_camera_pitch_step(BM_CAMERA_MODERN, RAC1_CAMERA_PITCH_STEP, &ntsc) / DEG, 5.25, 1e-4
    );

    // At 50 Hz, by 3.6: as quick a second.
    CHECK_NEAR(
        bm_camera_pitch_step(BM_CAMERA_MODERN, RAC1_CAMERA_PITCH_TRACKING, &pal), 0.072, TOL
    );

    // The other choices leave them.
    CHECK_EQ(
        bm_camera_pitch_step(BM_CAMERA_RAC2, RAC1_CAMERA_PITCH_STEP, &ntsc)
            == RAC1_CAMERA_PITCH_STEP,
        1
    );
    CHECK_EQ(bm_camera_pitch_step(BM_CAMERA_RAC1, 0.02f, &ntsc) == 0.02f, 1);

    // Not a step: left alone.
    CHECK_EQ(bm_camera_pitch_step(BM_CAMERA_MODERN, 0.0f, &ntsc) == 0.0f, 1);
}

/* When the stick turns the camera, as rac1's camera reads it. */
static void turning(void) {
    // Any x past rac1's dead zone turns it (the pad already zeroed it inside).
    CHECK_EQ(bm_camera_turning(0.05f, 0.0f, 0), 1);
    CHECK_EQ(bm_camera_turning(-1.0f, 0.0f, 0), 1);
    CHECK_EQ(bm_camera_turning(0.0f, 0.0f, 0), 0);

    // The pitch only past 0.3.
    CHECK_EQ(bm_camera_turning(0.0f, 0.29f, 0), 0);
    CHECK_EQ(bm_camera_turning(0.0f, -0.31f, 0), 1);

    // The camera's flags shut an axis (1 the turn, 2 the pitch).
    CHECK_EQ(bm_camera_turning(1.0f, 0.0f, 1), 0);
    CHECK_EQ(bm_camera_turning(0.0f, 1.0f, 2), 0);
    CHECK_EQ(bm_camera_turning(1.0f, 1.0f, 2), 1);

    // A bad read is no turn.
    CHECK_EQ(bm_camera_turning(NAN, NAN, 0), 0);

    // The pitch is the player's while the stick's y is past 0.3, or its aim is not back at 0.
    CHECK_EQ(bm_camera_pitching(0.5f, 0, 0.0f), 1);
    CHECK_EQ(bm_camera_pitching(0.2f, 0, 0.0f), 0);
    CHECK_EQ(bm_camera_pitching(0.0f, 0, -0.25f), 1);
    CHECK_EQ(bm_camera_pitching(0.0f, 0, 0.0f), 0);

    // The flags shut the stick's pitch, not an aim on its way back.
    CHECK_EQ(bm_camera_pitching(1.0f, 2, 0.0f), 0);
    CHECK_EQ(bm_camera_pitching(0.0f, 2, 0.1f), 1);
    CHECK_EQ(bm_camera_pitching(NAN, 0, NAN), 0);
}

/* Modern's spring and its window. */
static void spring(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();

    CHECK_NEAR(bm_camera_spring_accel(&ntsc), 0.2, TOL);
    CHECK_NEAR(bm_camera_spring_damp(&ntsc), 0.7, TOL);

    // At 50 Hz: 0.2 x 1.44 and 0.7 x 1.2.
    CHECK_NEAR(bm_camera_spring_accel(&pal), 0.288, TOL);
    CHECK_NEAR(bm_camera_spring_damp(&pal), 0.84, TOL);

    // 20 ticks after the stick lets go (24 at 50 Hz), counted down, never below 0.
    CHECK_EQ(bm_camera_settle(3, 1, &ntsc), 20);
    CHECK_EQ(bm_camera_settle(0, 1, &pal), 24);
    CHECK_EQ(bm_camera_settle(20, 0, &ntsc), 19);
    CHECK_EQ(bm_camera_settle(0, 0, &ntsc), 0);
}

/* ---- rac1's follow camera, as far as the yaw goes ------------------------------------------- */

/* What a turn looks like: the eye's rate, its ramp and its stop. */
typedef struct feel {
    double rate;  /* degrees a second, held */
    int half;     /* ticks to half that */
    int most;     /* ticks to 90% of it */
    double coast; /* degrees turned after letting go */
    int stop;     /* ticks after letting go to under 1% of the rate */
} feel;

/* The orbit turned `gain` radians a tick for `hold` ticks, then let go. The eye follows it through
 * rope_spring_turn (follow_camera_orbit: the step v += k d - c v, at most d either way, d the angle
 * from the eye to the orbit): rac1's spring (0.015 / 0.2), or for `settle` > 0 modern's while turning
 * and `settle` ticks after. */
static feel turn(float gain, int hold, int settle, const bm_tick* t) {
    double orbit = 0.0, eye = 0.0, v = 0.0, steps[400];
    int left = 0;
    const int n = hold + 120;
    feel f = {0.0, -1, -1, 0.0, -1};

    // Tick by tick: the stick's turn, then the eye's spring.
    for (int i = 0; i < n; i++) {
        const int on = i < hold;
        double k = RAC1_CAMERA_SPRING_ACCEL, c = RAC1_CAMERA_SPRING_DAMP, d;

        const int stiff = settle > 0 && (on || left > 0);

        orbit += on ? gain : 0.0f;
        left = settle > 0 ? bm_camera_settle(left, on, t) : 0;

        // Modern's spring in its window.
        if (stiff) {
            k = bm_camera_spring_accel(t);
            c = bm_camera_spring_damp(t);
        }

        d = orbit - eye;
        v = v + (k * d - c * v);
        v = v > fabs(d) ? fabs(d) : (v < -fabs(d) ? -fabs(d) : v);
        eye += v;
        steps[i] = v;
    }

    f.rate = gain / t->ft / DEG;

    // The ramp.
    for (int i = 0; i < hold; i++) {
        // Half the rate.
        if (f.half < 0 && steps[i] >= 0.5 * gain) {
            f.half = i;
        }

        // 90%.
        if (f.most < 0 && steps[i] >= 0.9 * gain) {
            f.most = i;
        }
    }

    // The stop.
    for (int i = hold; i < n; i++) {
        // Still turning.
        if (f.stop < 0 && steps[i] >= 0.01 * gain) {
            f.coast += steps[i] / DEG;
            continue;
        }

        // Stopped.
        if (f.stop < 0) {
            f.stop = i - hold;
        }
    }

    return f;
}

/* Each choice's turn at full stick. */
static void feels(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();
    feel f;

    // Both games at their default step (1.3 degrees a tick): 78 degrees a second, half of it after 11
    // ticks, 90% after 24, and some 15 degrees coasted after letting go, stopping in 30 ticks. The
    // play-test measured 11, 24, 14.7 and 30 in both games' ports.
    f = turn(RAC1_CAMERA_SPEED_MEDIUM, 90, 0, &ntsc);
    CHECK_NEAR(f.rate, 78.0, 1e-3);
    CHECK_EQ(f.half, 11);
    CHECK_EQ(f.most, 24);
    CHECK_NEAR(f.coast, 16.0, 1.0);
    CHECK_NEAR(f.stop, 31, 1);

    // The same spring at three times the gain: the same ramp in ticks, three times the coast (48
    // degrees after letting go). That is why modern holds the eye closer.
    f = turn(bm_camera_gain(BM_CAMERA_MODERN, RAC1_CAMERA_SPEED_MEDIUM, 1, &ntsc), 90, 0, &ntsc);
    CHECK_EQ(f.most, 24);
    CHECK_NEAR(f.coast, 48.0, 2.0);

    // Modern: 234 degrees a second, 90% of it within 6 ticks (a tenth of a second), and stopped
    // within 10 ticks of letting go, coasting under 10 degrees.
    f = turn(bm_camera_gain(BM_CAMERA_MODERN, RAC1_CAMERA_SPEED_MEDIUM, 1, &ntsc), 90, 1, &ntsc);
    CHECK_NEAR(f.rate, 234.0, 1e-3);
    CHECK_EQ(f.half <= 2, 1);
    CHECK_EQ(f.most <= 6, 1);
    CHECK_EQ(f.coast < 10.0, 1);
    CHECK_EQ(f.stop <= 10, 1);

    // At 50 Hz, as quick in seconds: 90% within 5 ticks (0.1 s), stopped within 9 (0.18 s).
    f = turn(bm_camera_gain(BM_CAMERA_MODERN, RAC1_CAMERA_SPEED_MEDIUM, 1, &pal), 75, 1, &pal);
    CHECK_NEAR(f.rate, 234.0, 1e-3);
    CHECK_EQ(f.most <= 5, 1);
    CHECK_EQ(f.coast < 10.0, 1);
    CHECK_EQ(f.stop <= 9, 1);

    // A short flick (6 ticks): modern's spring stays for the eye to catch up, and it stops cleanly.
    f = turn(bm_camera_gain(BM_CAMERA_MODERN, RAC1_CAMERA_SPEED_MEDIUM, 1, &ntsc), 6, 1, &ntsc);
    CHECK_EQ(f.stop <= 10, 1);
}

int main(void) {
    tables();
    gains();
    pitch();
    turning();
    spring();
    feels();
    CHECK_DONE("camera");
}
