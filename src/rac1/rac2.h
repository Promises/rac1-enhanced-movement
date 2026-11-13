/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's movement numbers, as rac2-decomp writes them (USA v1.01, level 0's shared hero
 * code; DESIGN.md has each one's role). rac2 writes them for its 60 Hz tick: a speed per tick, an
 * acceleration per tick squared, a turn's accel and damping per tick, its limit in radians per tick.
 * The rac2_* helpers below turn them into this tick's units the way rac1 scales its own numbers.
 *
 * Every RAC2_ literal is also a row of tests/data/rac2_constants.tsv, which tools/rac2_constants.py
 * reads out of rac2-decomp with its file and line; tests/rac2_constants_test.py checks the two agree.
 */
#ifndef RAC1_BETTER_MOVEMENT_RAC2_H
#define RAC1_BETTER_MOVEMENT_RAC2_H

/* --- The run (func_L00_002BA7D8 case 2 and 0x70, func_L00_002B3D10) ------------------------- */

/* The run's speed, in units a second: level data D_001A7DB0[4], times the stick. */
#define RAC2_RUN_SPEED 5.95f
#define RAC2_WADING_SCALE 0.8f
#define RAC2_WADING_MIN 0.041666668f
#define RAC2_STICK_RELEASE 0.2f
#define RAC2_RUN_ACCEL 0.005f
#define RAC2_RUN_BRAKE 0.0055555557f
#define RAC2_TURNAROUND_BRAKE 0.004722222f
#define RAC2_K_STICK 0.9f
#define RAC2_K_ANGLE 0.78539824f
#define RAC2_SLOW_STICK 0.75f
#define RAC2_SLOW_ANGLE 0.6108653f
#define RAC2_SLOW_FACTOR 0.4f
#define RAC2_TURNAROUND_CAP 0.09916667f
#define RAC2_TURNAROUND_CAP_SLOPE 0.029178405f
#define RAC2_TURNAROUND_SCALE_BASE 2.617994f
#define RAC2_ALONG_SPEED 0.028333336f
#define RAC2_TURNAROUND_SLOW_ANGLE 2.0943954f
#define RAC2_TURNAROUND_FLICK_ANGLE 1.9198624f
#define RAC2_TURNAROUND_FLICK_ERROR 1.2217306f
#define RAC2_ALONG_CLEAR 0.087266475f
#define RAC2_TURNAROUND_CLEAR 0.06981318f
#define RAC2_RUNBAND_TURN_ACCEL 0.02f
#define RAC2_RUNBAND_TURN_DAMP 0.17f
#define RAC2_RUNBAND_TURN_LIMIT 0.16580631f

/* --- Braking in the idle group (func_L00_002BA7D8 case 0/1/3/4/...) ----------------------------- */

#define RAC2_IDLE_BRAKE 0.004916667f
#define RAC2_STOP_BRAKE 0.0075000003f
#define RAC2_CROUCH_BRAKE 0.0075000003f
#define RAC2_CROUCH_ICE_BRAKE 0.00047222225f

/* --- The walk band's, the turnaround's and the firing turn (func_L00_002B3F68) ------------------ */

#define RAC2_WALK_TURN_ACCEL_MAX 0.022f
#define RAC2_WALK_TURN_ACCEL_PER_STICK 0.02f
#define RAC2_WALK_TURN_ACCEL_MIN 0.01f
#define RAC2_WALK_TURN_LIMIT_MAX 0.09308424592018127f
#define RAC2_WALK_TURN_LIMIT_PER_STICK 0.06399541348218918f
#define RAC2_WALK_TURN_LIMIT_MIN 0.03199770674109459f
#define RAC2_WALK_TURN_DAMP 0.27f
#define RAC2_TURNAROUND_TURN_ACCEL 0.035f
#define RAC2_TURNAROUND_TURN_LIMIT 0.19489513337612152f
#define RAC2_FIRING_FULL_STICK 0.95f
#define RAC2_FIRING_ACCEL 0.017f
#define RAC2_FIRING_DAMP 0.18f
#define RAC2_FIRING_LIMIT 0.1454441249370575f
#define RAC2_FIRING_ACCEL_PER_STICK 0.009f
#define RAC2_FIRING_DAMP_SLOW 0.15f
#define RAC2_FIRING_LIMIT_PER_STICK 0.07853982597589493f

/* --- Leaving the run, and the jump X gives (func_L00_002CB3A8 case 2) --------------------------- */

#define RAC2_EXIT_STICK 0.17f
#define RAC2_EXIT_TURNED 0.03490659f
#define RAC2_EXIT_CLAMP 0.025000002f
#define RAC2_RUN_JUMP_STICK 0.85f

/* --- Jumps (func_L00_002C42E0's jump arm) ---------------------------------------------------- */

#define RAC2_AIR_STICK_SPEED 0.099166668951511383f
#define RAC2_DOUBLE_JUMP_AIR_SPEED 0.079166673123836517f
#define RAC2_FLIP_DRIFT_START 0.083333335816860199f

/* --- The strafe (func_L00_002B35A0, func_L00_002B39F0, func_L00_002C9FC8, func_L00_002B6BC8) --- */

#define RAC2_STRAFE_TURN_ACCEL 0.037f
#define RAC2_STRAFE_TURN_DAMP 0.25f
#define RAC2_STRAFE_TURN_LIMIT 0.2036217749118805f
#define RAC2_STRAFE_REVERSE -0.25f
#define RAC2_STRAFE_SECTOR 0.7853982448577881f
#define RAC2_STRAFE_BACKPEDAL_STICK 0.15f
#define RAC2_STRAFE_START_ANGLE 0.17453295f
#define RAC2_STRAFE_START_STICK 0.22f
#define RAC2_FLIP_STICK 0.9f
#define RAC2_FLIP_DRIFT_PER_STICK 0.0833333358168602f
#define RAC2_FLIP_DRIFT_MIN 0.03333333507180214f
#define RAC2_FLIP_DRIFT_STEP 0.010277777910232544f

/* The side timers' length, in rac2 ticks (func_L00_002B35A0 stores 5). Not a float literal, so not in
 * the table. */
#define RAC2_STRAFE_SIDE_TICKS 5

/* --- The hip twist (func_L00_002B7B80) --------------------------------------------------------- */

#define RAC2_TWIST_STICK 0.15f
#define RAC2_TWIST_HIP_ACCEL 0.037f
#define RAC2_TWIST_HIP_DAMP 0.27f
#define RAC2_TWIST_BODY_ACCEL 0.025f
#define RAC2_TWIST_BODY_DAMP 0.27f

/* --- This tick's units ---------------------------------------------------------------------- */

/* The tick rac1 runs: g_frame_time (seconds a tick), g_frame_time_sq, g_frame_scale_sq and
 * frame_scale (set_video_timing: 1/60, 1/3600, 1, 1 on NTSC; 1/50, 1/2500, 1.44, 1.2 on PAL). */
typedef struct bm_tick {
    float ft;
    float ft2;
    float fs2;
    float fs;
} bm_tick;

/* A rac2 speed per tick (60 Hz) in this tick's units. */
static inline float rac2_speed(float per_tick, const bm_tick* t) {
    return per_tick * 60.0f * t->ft;
}

/* A rac2 acceleration per tick squared in this tick's units. */
static inline float rac2_accel(float per_tick2, const bm_tick* t) {
    return per_tick2 * 3600.0f * t->ft2;
}

/* A rac2 turn limit (radians a tick) in this tick's units. */
static inline float rac2_turn_limit(float per_tick, const bm_tick* t) {
    return per_tick * 60.0f * t->ft;
}

/* A rac2 turn spring coefficient (accel or damping) in this tick's units, as rac1 scales its own. */
static inline float rac2_spring(float coefficient, const bm_tick* t) {
    return coefficient * t->fs2;
}

/* A count of ticks as rac1 scales its own (scale_ticks, func_001F9850): n * frame_scale + 0.5, cut
 * to an integer. rac1's windows the mod mirrors (the jump's, the run's exit) are counted so. rac2's own
 * counts are not: its PAL release keeps them in ticks (rac2-decomp src/pal, L00_002B3210.c). */
static inline int bm_scale_ticks(int n, const bm_tick* t) {
    return (int)(0.5f + (float)n * t->fs);
}

/* The NTSC tick (60 Hz): rac1's g_frame_time, g_frame_time_sq, g_frame_scale_sq, frame_scale there. */
static inline bm_tick bm_tick_ntsc(void) {
    bm_tick t = {1.0f / 60.0f, 1.0f / 3600.0f, 1.0f, 1.0f};
    return t;
}

/* The PAL tick (50 Hz). */
static inline bm_tick bm_tick_pal(void) {
    bm_tick t = {1.0f / 50.0f, 1.0f / 2500.0f, 1.44f, 1.2f};
    return t;
}

#endif /* RAC1_BETTER_MOVEMENT_RAC2_H */
