/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The camera's decisions (src/rac1/camera.h) against values worked out by hand from both games' speed
 * tables: the gain each choice gives the follow camera and when the stick turns the camera. Then
 * rac1's follow_camera_orbit as far as the yaw goes (the orbit turned by the gain, the eye after it
 * through rope_spring_turn), to check what each choice feels like: the ramp and the stop the
 * play-test measured (PLAYTEST.md).
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

/* The gain each choice gives. Both games count it per tick, unscaled at 50 Hz, so the choice needs no
 * tick: at 50 Hz Going Commando's fastest step is 2.2 degrees a tick as at 60. */
static void gains(void) {
    const float slow = RAC1_CAMERA_SPEED_SLOW;
    const float medium = RAC1_CAMERA_SPEED_MEDIUM;
    const float fast = RAC1_CAMERA_SPEED_FAST;

    // The first game's: as it came.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC1, fast, 2) == fast, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC1, slow, 0) == slow, 1);

    // Going Commando's: its fastest step on every step of the game's option, 2.2 degrees a tick, 132
    // a second.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, slow, 0), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, medium, 1), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, fast, 2), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, medium, 1) * 60.0 / DEG, 132.0, 1e-3);

    // Another mod doubled rac1's: rac2's doubled too.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, 2.0f * fast, 2), 0.0767945, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, 2.0f * medium, 1), 0.0767945, TOL);

    // An option rac1's table does not have: as it came.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, 0.05f, 7) == 0.05f, 1);

    // A gain that is no number, 0 or negative: left alone.
    CHECK_EQ(isnan(bm_camera_gain(BM_CAMERA_RAC2, NAN, 1)), 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, 0.0f, 1) == 0.0f, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, -0.03f, 2) == -0.03f, 1);
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
 * from the eye to the orbit) on rac1's spring (0.015 / 0.2), which every choice keeps. */
static feel turn(float gain, int hold, const bm_tick* t) {
    double orbit = 0.0, eye = 0.0, v = 0.0, steps[400];
    const int n = hold + 120;
    feel f = {0.0, -1, -1, 0.0, -1};

    // Tick by tick: the stick's turn, then the eye's spring.
    for (int i = 0; i < n; i++) {
        const double k = RAC1_CAMERA_SPRING_ACCEL, c = RAC1_CAMERA_SPRING_DAMP;
        double d;

        orbit += i < hold ? gain : 0.0f;
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
    feel f;

    // Both games at their default step (1.3 degrees a tick): 78 degrees a second, half of it after 11
    // ticks, 90% after 24, and some 15 degrees coasted after letting go, stopping in 30 ticks. The
    // play-test measured 11, 24, 14.7 and 30 in both games' ports.
    f = turn(bm_camera_gain(BM_CAMERA_RAC1, RAC1_CAMERA_SPEED_MEDIUM, 1), 90, &ntsc);
    CHECK_NEAR(f.rate, 78.0, 1e-3);
    CHECK_EQ(f.half, 11);
    CHECK_EQ(f.most, 24);
    CHECK_NEAR(f.coast, 16.0, 1.0);
    CHECK_NEAR(f.stop, 31, 1);

    // Going Commando's fastest step on the same spring: 132 degrees a second, the same ramp in ticks,
    // 2.2 / 1.3 times the coast (some 27 degrees). The play-test measured 131.2, 11, 24 and 27.3, as
    // Going Commando's own port on that step.
    f = turn(bm_camera_gain(BM_CAMERA_RAC2, RAC1_CAMERA_SPEED_MEDIUM, 1), 90, &ntsc);
    CHECK_NEAR(f.rate, 132.0, 1e-3);
    CHECK_EQ(f.half, 11);
    CHECK_EQ(f.most, 24);
    CHECK_NEAR(f.coast, 27.0, 1.0);
    CHECK_NEAR(f.stop, 31, 1);
}

int main(void) {
    tables();
    gains();
    turning();
    feels();
    CHECK_DONE("camera");
}
