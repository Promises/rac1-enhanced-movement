/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests the camera decisions (src/rac1/camera.h) against values worked out by hand from both games'
 * speed tables: the gain each choice gives the follow camera, and when the stick turns the camera.
 * It then runs the yaw of rac1's follow_camera_orbit (the orbit turned by the gain, the eye
 * following through rope_spring_turn) to check how each choice feels against the ramp and the stop
 * the play-test measured (PLAYTEST.md).
 */
#include <math.h>

#include "check.h"
#include "rac1/camera.h"

/* Radians in a degree. */
#define DEG (3.14159265358979 / 180.0)

/* Tests the speed tables: rac1's 1.0, 1.3 and 1.6 degrees a tick, and rac2's 1.0, 1.3 and 2.2. */
static void tables(void) {
    CHECK_NEAR(bm_rac1_camera_speed(0) / DEG, 1.0, 1e-5);
    CHECK_NEAR(bm_rac1_camera_speed(1) / DEG, 1.3, 1e-5);
    CHECK_NEAR(bm_rac1_camera_speed(2) / DEG, 1.6, 1e-5);
    CHECK_NEAR(bm_rac2_camera_speed(0) / DEG, 1.0, 1e-5);
    CHECK_NEAR(bm_rac2_camera_speed(1) / DEG, 1.3, 1e-5);
    CHECK_NEAR(bm_rac2_camera_speed(2) / DEG, 2.2, 1e-5);

    // Neither table has a fourth step.
    CHECK_NEAR(bm_rac1_camera_speed(3), 0.0, TOL);
    CHECK_NEAR(bm_rac2_camera_speed(-1), 0.0, TOL);

    // The rest of rac2's turn, the pitch steps and the eye's springs, is rac1's to float precision.
    CHECK_NEAR(RAC2_CAMERA_PITCH_TRACKING, RAC1_CAMERA_PITCH_TRACKING, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_RETURN, RAC1_CAMERA_PITCH_RETURN, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_STEP, RAC1_CAMERA_PITCH_STEP, 1e-8);
    CHECK_NEAR(RAC2_CAMERA_YAW_SPRING_ACCEL, RAC1_CAMERA_SPRING_ACCEL, TOL);
    CHECK_NEAR(RAC2_CAMERA_YAW_SPRING_DAMP, RAC1_CAMERA_SPRING_DAMP, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_SPRING_ACCEL, RAC1_CAMERA_SPRING_ACCEL, TOL);
    CHECK_NEAR(RAC2_CAMERA_PITCH_SPRING_DAMP, RAC1_CAMERA_SPRING_DAMP, TOL);
}

/* Tests the gain each choice gives. Both games count it per tick, unscaled at 50 Hz, so no tick is
 * needed: Going Commando's fastest step is 2.2 degrees a tick at 50 Hz as at 60. */
static void gains(void) {
    const float slow = RAC1_CAMERA_SPEED_SLOW;
    const float medium = RAC1_CAMERA_SPEED_MEDIUM;
    const float fast = RAC1_CAMERA_SPEED_FAST;

    // rac1 keeps the gain as it came.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC1, fast, 2) == fast, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC1, slow, 0) == slow, 1);

    // rac2 keeps the slow and middle steps exactly, and turns its fastest at 2.2 degrees a tick,
    // 132 a second, for rac1's 1.6.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, slow, 0) == slow, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, medium, 1) == medium, 1);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, fast, 2), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, fast, 2) * 60.0 / DEG, 132.0, 1e-3);

    // Enhanced scales every step by 2.2 / 1.3: about 101.5, 132 and 162.5 degrees a second.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_ENHANCED, slow, 0) * 60.0 / DEG, 60.0 * 2.2 / 1.3, 1e-3);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_ENHANCED, medium, 1), 0.03839725, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_ENHANCED, medium, 1) * 60.0 / DEG, 132.0, 1e-3);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_ENHANCED, fast, 2) * 60.0 / DEG, 96.0 * 2.2 / 1.3, 1e-3);

    // When another mod has doubled rac1's gain, the choice's is doubled too.
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_RAC2, 2.0f * fast, 2), 0.0767945, TOL);
    CHECK_NEAR(bm_camera_gain(BM_CAMERA_ENHANCED, 2.0f * medium, 1), 0.0767945, TOL);

    // An option missing from rac1's table keeps the gain.
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, 0.05f, 7) == 0.05f, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_ENHANCED, 0.05f, 7) == 0.05f, 1);

    // A gain that is NaN, 0 or negative stays as it is.
    CHECK_EQ(isnan(bm_camera_gain(BM_CAMERA_RAC2, NAN, 1)), 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, 0.0f, 1) == 0.0f, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_RAC2, -0.03f, 2) == -0.03f, 1);
    CHECK_EQ(bm_camera_gain(BM_CAMERA_ENHANCED, -0.03f, 1) == -0.03f, 1);
}

/* Tests when the stick turns the camera, as rac1's camera reads it. */
static void turning(void) {
    // Any x past rac1's dead zone turns it (the pad already zeroed it inside).
    CHECK_EQ(bm_camera_turning(0.05f, 0.0f, 0), 1);
    CHECK_EQ(bm_camera_turning(-1.0f, 0.0f, 0), 1);
    CHECK_EQ(bm_camera_turning(0.0f, 0.0f, 0), 0);

    // The pitch turns only past 0.3.
    CHECK_EQ(bm_camera_turning(0.0f, 0.29f, 0), 0);
    CHECK_EQ(bm_camera_turning(0.0f, -0.31f, 0), 1);

    // The camera's flags shut an axis (1 the turn, 2 the pitch).
    CHECK_EQ(bm_camera_turning(1.0f, 0.0f, 1), 0);
    CHECK_EQ(bm_camera_turning(0.0f, 1.0f, 2), 0);
    CHECK_EQ(bm_camera_turning(1.0f, 1.0f, 2), 1);

    // A bad read turns nothing.
    CHECK_EQ(bm_camera_turning(NAN, NAN, 0), 0);
}

/* ---- The follow camera's yaw ---- */

/* How a turn feels: the eye's rate, its ramp and its stop. */
typedef struct feel {
    double rate;  /* degrees a second, held */
    int half;     /* ticks to half that */
    int most;     /* ticks to 90% of it */
    double coast; /* degrees turned after letting go */
    int stop;     /* ticks after letting go to under 1% of the rate */
} feel;

/* Turns the orbit `gain` radians a tick for `hold` ticks, lets go, and returns how the eye
 * followed. The eye follows through rope_spring_turn (follow_camera_orbit): each tick
 * v += k d - c v, kept within d either way, where d is the angle from the eye to the orbit. The
 * spring is rac1's 0.015 / 0.2, which every choice keeps. */
static feel turn(float gain, int hold, const bm_tick* t) {
    double orbit = 0.0, eye = 0.0, v = 0.0, steps[400];
    // 120 ticks after letting go, within the 400 the steps hold.
    const int n = hold + 120;
    feel f = {0.0, -1, -1, 0.0, -1};

    // Each tick the stick turns the orbit, then the eye's spring follows it.
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

    // The ramp: the first ticks at half and at 90% of the held rate.
    for (int i = 0; i < hold; i++) {
        // The first tick at half the rate.
        if (f.half < 0 && steps[i] >= 0.5 * gain) {
            f.half = i;
        }

        // The first tick at 90% of it.
        if (f.most < 0 && steps[i] >= 0.9 * gain) {
            f.most = i;
        }
    }

    // The stop: the coast after letting go, until the eye slows under 1% of the rate.
    for (int i = hold; i < n; i++) {
        // Still turning: add to the coast.
        if (f.stop < 0 && steps[i] >= 0.01 * gain) {
            f.coast += steps[i] / DEG;
            continue;
        }

        // The first tick under 1% is the stop.
        if (f.stop < 0) {
            f.stop = i - hold;
        }
    }

    return f;
}

/* Tests each choice's turn at full stick. */
static void feels(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    feel f;

    // Both games at their default step (1.3 degrees a tick) turn 78 degrees a second, reach half of
    // it after 11 ticks and 90% after 24, and coast about 16 degrees after letting go, stopping in
    // about 31 ticks. The play-test measured 11, 24, 14.7 and 30 in both games' ports.
    f = turn(bm_camera_gain(BM_CAMERA_RAC1, RAC1_CAMERA_SPEED_MEDIUM, 1), 90, &ntsc);
    CHECK_NEAR(f.rate, 78.0, 1e-3);
    CHECK_EQ(f.half, 11);
    CHECK_EQ(f.most, 24);
    CHECK_NEAR(f.coast, 16.0, 1.0);
    CHECK_NEAR(f.stop, 31, 1);

    // Enhanced at the default step turns at Going Commando's fastest, 132 degrees a second, on the
    // same spring: the same ramp in ticks and 2.2 / 1.3 times the coast (about 27 degrees). The
    // play-test measured 131.2, 11, 24 and 27.3, as in Going Commando's own port on its fastest step.
    f = turn(bm_camera_gain(BM_CAMERA_ENHANCED, RAC1_CAMERA_SPEED_MEDIUM, 1), 90, &ntsc);
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
