/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Angles and easing as the games use them: yaws in radians on (-pi, pi], wrapped differences
 * (rac1's FastSubRots / FastDiffRots, rac2's func_00283BE0 / func_00283C80), and ApproachValue
 * (rac1's and rac2's func_002AA2E8: a value moved toward a target by at most a step, never past it).
 */
#ifndef RAC1_BETTER_MOVEMENT_ANGLE_H
#define RAC1_BETTER_MOVEMENT_ANGLE_H

#include <math.h>

#define BM_PI 3.14159265358979f
#define BM_HALF_PI 1.5707963267949f

/* An angle on (-pi, pi]; 0 for NaN or infinity (a bad read from game memory). */
static inline float bm_wrap(float a) {
    // A NaN or an infinity is no angle.
    if (!isfinite(a)) {
        return 0.0f;
    }

    a = fmodf(a, 2.0f * BM_PI);

    // fmodf keeps the sign: one turn more or less brings it within (-pi, pi].
    if (a > BM_PI) {
        a -= 2.0f * BM_PI;
    } else if (a <= -BM_PI) {
        a += 2.0f * BM_PI;
    }

    return a;
}

/* a - b, wrapped (FastSubRots). */
static inline float bm_sub(float a, float b) {
    return bm_wrap(a - b);
}

/* How far apart two angles are, 0 to pi (FastDiffRots). */
static inline float bm_diff(float a, float b) {
    return fabsf(bm_wrap(a - b));
}

/* v moved toward target by at most step (ApproachValue). */
static inline float bm_approach(float v, float target, float step) {
    // Below: up by the step, not past the target.
    if (v < target) {
        v += step;
        return v > target ? target : v;
    }

    v -= step;
    return v < target ? target : v;
}

/* x within [lo, hi]. */
static inline float bm_clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

#endif /* RAC1_BETTER_MOVEMENT_ANGLE_H */
