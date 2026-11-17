/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Angle helpers for the decisions, matching how both games handle yaws: radians on (-pi, pi],
 * wrapped differences (rac1's FastSubRots and FastDiffRots, rac2's func_00283BE0 and
 * func_00283C80), and ApproachValue (rac1's func_00214D28, rac2's func_002AA2E8), which moves a
 * value toward a target by at most a step without passing it.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_ANGLE_H
#define RAC1_ENHANCED_MOVEMENT_ANGLE_H

#include <math.h>

/* Pi and pi / 2 as floats. */
#define BM_PI 3.14159265358979f
#define BM_HALF_PI 1.5707963267949f

/* Returns `a` wrapped to (-pi, pi], or 0 for NaN or infinity (a bad read from game memory). */
static inline float bm_wrap(float a) {
    // A NaN or an infinity is no angle.
    if (!isfinite(a)) {
        return 0.0f;
    }

    a = fmodf(a, 2.0f * BM_PI);

    // fmodf keeps the sign, so one turn more or less brings it within (-pi, pi].
    if (a > BM_PI) {
        a -= 2.0f * BM_PI;
    } else if (a <= -BM_PI) {
        a += 2.0f * BM_PI;
    }

    return a;
}

/* Returns a - b, wrapped (FastSubRots). */
static inline float bm_sub(float a, float b) {
    return bm_wrap(a - b);
}

/* Returns how far apart two angles are, 0 to pi (FastDiffRots). */
static inline float bm_diff(float a, float b) {
    return fabsf(bm_wrap(a - b));
}

/* Returns `v` moved toward `target` by at most `step`, never past it (ApproachValue). */
static inline float bm_approach(float v, float target, float step) {
    // Below the target, step up and stop at it.
    if (v < target) {
        v += step;
        return v > target ? target : v;
    }

    v -= step;
    return v < target ? target : v;
}

/* Returns `x` limited to [lo, hi]. */
static inline float bm_clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

#endif /* RAC1_ENHANCED_MOVEMENT_ANGLE_H */
