/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's movement numbers, as rac2-decomp writes them (USA v1.01, level 0's shared hero
 * code; DESIGN.md has each one's role). rac2 writes them for its 60 Hz tick: a speed per tick, an
 * acceleration per tick squared, a turn's accel and damping per tick, its limit in radians per tick.
 * The rac2_* helpers below turn them into this tick's units the way rac1 scales its own numbers.
 *
 * Every RAC2_ literal is also a row of tests/data/rac2_constants.tsv, which tools/rac2_constants.py
 * reads out of rac2-decomp with its file and line (or out of level 0's data, for the run speed and
 * the attack records); tests/rac2_constants_test.py checks the two agree. Counts of ticks and polls
 * are integers; wrench.h counts them as rac1 counts the windows they replace (bm_scale_ticks).
 */
#ifndef RAC1_ENHANCED_MOVEMENT_RAC2_H
#define RAC1_ENHANCED_MOVEMENT_RAC2_H

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

/* The side timers' length, in rac2 ticks (func_L00_002B35A0 stores 5). */
#define RAC2_STRAFE_SIDE_TICKS 5

/* --- The hip twist (func_L00_002B7B80) --------------------------------------------------------- */

#define RAC2_TWIST_STICK 0.15f
#define RAC2_TWIST_HIP_ACCEL 0.037f
#define RAC2_TWIST_HIP_DAMP 0.27f
#define RAC2_TWIST_BODY_ACCEL 0.025f
#define RAC2_TWIST_BODY_DAMP 0.27f

/* --- The wrench (DESIGN.md section 5) -------------------------------------------------------- */

/* The lock-on turn of the swings and the slam (func_L00_002BA7D8 cases 0x13, 0x14), and the
 * throw's accel (case 0x15). rac1 turns at 0.05 / 0.2 / 15.01 radians a second in all three. */
#define RAC2_WRENCH_TURN_ACCEL 0.05f
#define RAC2_WRENCH_TURN_DAMP 0.2f
#define RAC2_WRENCH_TURN_LIMIT 0.2821616f
#define RAC2_THROW_TURN_ACCEL 0.057f

/* Leaving a swing (func_L00_002CB3A8 cases 0x13 and 0x15): past frame 34 with the stick past 0.9,
 * the run; past the record's exit frame, the run with the stick past 0.8, else idle 5 frames later
 * or when the animation ends. rac1 goes to idle at the exit frame. */
#define RAC2_THROW_RUN_FRAME 34.0f
#define RAC2_THROW_RUN_STICK 0.9f
#define RAC2_RUN_OUT_STICK 0.8f
#define RAC2_EXIT_IDLE_FRAMES 5

/* Square's windows in polls (func_L00_002C9448): standing or running 10 (rac1 7), in a jump 14
 * (rac1 10; both 18 in the high and double jumps), in a swing (frame - first) * 2 up to 15 (as
 * rac1's); the throw again from its 31st tick (rac1 after its 30th). X's window in the throw
 * (func_L00_002CB3A8 case 0x15): 8, from frame 35 on (rac1: (frame - 83) * 2 from frame 88, after
 * the catch). */
#define RAC2_WRENCH_WINDOW 10
#define RAC2_WRENCH_AIR_WINDOW 14
#define RAC2_WRENCH_AIR_LATE_WINDOW 18
#define RAC2_CHAIN_WINDOW 15
#define RAC2_THROW_AGAIN_TICKS 31
#define RAC2_THROW_JUMP_WINDOW 8

/* The slam (func_L00_002BA7D8 case 0x14): more than 4 above the ground, it drives on at 4.5 units
 * a second (a target speed rac1 gives only toward a target 2 to 5 away), for 70 ticks without a
 * target. */
#define RAC2_SLAM_DRIVE 0.075f
#define RAC2_SLAM_DRIVE_HEIGHT 4.0f
#define RAC2_SLAM_DRIVE_TICKS 0x46

/* The attack records (level 0 data D_L00_0025D990, 0x2C bytes each; D_L00_0025D910 on the v2.00
 * disc): the frames rac1's attack_records hold at +0xC, +0x10, +0x14 and +0x18 (rac1.h). Records 0
 * to 2 are the combo's swings, 3 the throw. Against rac1: the first swing chains from frame 25 (26)
 * and ends at 26 (31), the second ends at 15 (23), and the throw takes a jump from frame 35 (87). */
#define RAC2_REC0_FIRST 19
#define RAC2_REC0_OPEN 25
#define RAC2_REC0_JUMP 24
#define RAC2_REC0_EXIT 26
#define RAC2_REC1_FIRST 7
#define RAC2_REC1_OPEN 12
#define RAC2_REC1_JUMP 13
#define RAC2_REC1_EXIT 15
#define RAC2_REC2_FIRST 16
#define RAC2_REC2_OPEN 23
#define RAC2_REC2_JUMP 19
#define RAC2_REC2_EXIT 21
#define RAC2_REC3_FIRST 83
#define RAC2_REC3_OPEN 88
#define RAC2_REC3_JUMP 35
#define RAC2_REC3_EXIT 91

/* --- The follow camera (DESIGN.md section 8) --------------------------------------------------- */

/* The camera speed table (level 0 data D_L00_001B15B8, gp + 0x2568 = 0x1B1558 on the v2.00 disc),
 * read by func_L00_004263B8 (follow_camera_update) for its camera speed option (D_001A7BAC, 1 by
 * default) into the gain func_L00_00424F98 (follow_camera_read_stick) hands the turn: 1.0, 1.3 and
 * 2.2 degrees a tick. rac1's are 1.0, 1.3 and 1.6, so the two games differ only on the fastest. */
#define RAC2_CAMERA_SPEED_SLOW 0.017453294f
#define RAC2_CAMERA_SPEED_MEDIUM 0.022689281f
#define RAC2_CAMERA_SPEED_FAST 0.03839725f
/* The rest of the turn is rac1's, number for number: func_L00_00425118 (follow_camera_turn_and_pitch)
 * aims the pitch with steps of 0.02 out, 0.01 back and 1.75 degrees a tick (0x1B1458, 0x1B145C,
 * 0x1B1460), and func_L00_00425CD0 (follow_camera_orbit) eases the eye's yaw and pitch at 0.015 / 0.2
 * (0x1B1488 to 0x1B1494). Only camera_test.c uses these: they show the mod has nothing else to port. */
#define RAC2_CAMERA_PITCH_TRACKING 0.02f
#define RAC2_CAMERA_PITCH_RETURN 0.01f
#define RAC2_CAMERA_PITCH_STEP 0.030543264f
#define RAC2_CAMERA_YAW_SPRING_ACCEL 0.015f
#define RAC2_CAMERA_YAW_SPRING_DAMP 0.2f
#define RAC2_CAMERA_PITCH_SPRING_ACCEL 0.015f
#define RAC2_CAMERA_PITCH_SPRING_DAMP 0.2f

/* --- Clank (DESIGN.md section 9) --------------------------------------------------------------- */

/* His run (func_L00_002BA7D8 cases 0x40 and 0x4C; rac1's 0x44 and 0x50). The speed is the stick
 * times 3 units a second, as rac1's (func_L00_002B39F0(0.05)). rac2 then turns at 0.033 / 0.15 /
 * 9.95 radians a second at any stick and speed (0.037 / 0.15 / 11.69 in a turnaround), speeds up at
 * 18 and slows at 20 units a second squared (rac1: 7.5 cut by the turn, and 8.5). Its flags: along
 * the stick (+0x40C) under 0.7 units a second; a turnaround (+0x40F) there more than 100 degrees off
 * with the stick past 0.8, or when the stick's angle 0 polls back (the ring's oldest, 30 polls ago)
 * and 2 back are more than 150 degrees apart, 70 degrees off, the stick past 0.8; along cleared under
 * 5 degrees, the turnaround under 4. Its cap on the turnaround's speed is written after the speed
 * step, which the next tick's stick overwrites: it changes nothing. */
#define RAC2_CLANK_RUN_SPEED 0.050000004f
#define RAC2_CLANK_TURN_ACCEL 0.033f
#define RAC2_CLANK_TURN_DAMP 0.15f
#define RAC2_CLANK_TURN_LIMIT 0.16580631f
#define RAC2_CLANK_TURNAROUND_ACCEL 0.037f
#define RAC2_CLANK_TURNAROUND_LIMIT 0.19489513f
#define RAC2_CLANK_RUN_ACCEL 0.005f
#define RAC2_CLANK_RUN_BRAKE 0.0055555557f
#define RAC2_CLANK_ALONG_SPEED 0.011666667f
#define RAC2_CLANK_TURNAROUND_ANGLE 1.7453294f
#define RAC2_CLANK_TURNAROUND_STICK 0.8f
#define RAC2_CLANK_FLICK_FROM 0
#define RAC2_CLANK_FLICK_TO 2
#define RAC2_CLANK_FLICK_ANGLE 2.617994f
#define RAC2_CLANK_FLICK_ERROR 1.2217306f
#define RAC2_CLANK_FLICK_STICK 0.8f
#define RAC2_CLANK_ALONG_CLEAR 0.087266475f
#define RAC2_CLANK_TURNAROUND_CLEAR 0.06981318f

/* The same in both games (clank_test.c checks each against rac1.h): idle's braking 12.6 (0x3F),
 * the punch's speed step 37 / 28 (0x4D), the glide's stick speed 2 and turn limit 8.73 (0x4B). The
 * punch's lock-on turn is the wrench's, 16.93 radians a second (rac1 15.01 in both). */
#define RAC2_CLANK_IDLE_BRAKE 0.0035f
#define RAC2_CLANK_PUNCH_TURN_LIMIT 0.2821616f
#define RAC2_CLANK_PUNCH_ACCEL 0.010277778f
#define RAC2_CLANK_PUNCH_BRAKE 0.007777778f
#define RAC2_CLANK_GLIDE_SPEED 0.033333335f
#define RAC2_CLANK_GLIDE_TURN_LIMIT 0.14544412f

/* His jump's entry (func_L00_002C42E0 state 0x45; rac1's 0x49): off the ground after 3 ticks (rac1
 * 5), its animation's apex and landing frames 15 and 24 (rac1 18 and 27). The heights 1.3 / 1.35,
 * the gravity 25 and the air stick speed 2 are rac1's. rac2's PAL release counts the takeoff as 2. */
#define RAC2_CLANK_JUMP_HEIGHT 1.3f
#define RAC2_CLANK_JUMP_HEIGHT_HELD 1.35f
#define RAC2_CLANK_TAKEOFF 3
#define RAC2_CLANK_APEX_FRAME 15.0f
#define RAC2_CLANK_TARGET_FRAME 24.0f
#define RAC2_CLANK_JUMP_GRAVITY 0.0069444445f
#define RAC2_CLANK_AIR_STICK_SPEED 0.033333335f

/* His windows in polls (func_L00_002CB3A8 cases 0x3F, 0x40 and 0x45): X 8 and Square 9 standing or
 * running, Square 5 landing from a jump. All rac1's. */
#define RAC2_CLANK_JUMP_WINDOW 8
#define RAC2_CLANK_SQUARE_WINDOW 9
#define RAC2_CLANK_LAND_SQUARE_WINDOW 5

/* His punches' records 7 and 8 (level 0 data D_L00_0025D990, as the wrench's): the next punch chains
 * under frame 16, the windows count from frame 3, it jumps out after 8 and stands after 10, it hits
 * from 3 to 6. rac1's level data is the same (DESIGN.md 9.1). */
#define RAC2_REC7_CHAIN 16
#define RAC2_REC7_FIRST 3
#define RAC2_REC7_OPEN 12
#define RAC2_REC7_JUMP 8
#define RAC2_REC7_EXIT 10
#define RAC2_REC7_HIT_FROM 3
#define RAC2_REC7_HIT_TO 6
#define RAC2_REC8_CHAIN 16
#define RAC2_REC8_FIRST 3
#define RAC2_REC8_OPEN 12
#define RAC2_REC8_JUMP 8
#define RAC2_REC8_EXIT 10
#define RAC2_REC8_HIT_FROM 3
#define RAC2_REC8_HIT_TO 6

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

/* A count of rac2's own polls or ticks at this tick, as rac2's PAL release counts the wrench's: (n x 5
 * + 2) / 6 at 50 Hz, n at 60 (its PAL deciders' throw window is 7 for 8 and the crouch's Square
 * window 12 for 15: rac2-decomp src/pal/levels/03_endako/L03_002BF860-sw.c). Unlike rac1's
 * scale_ticks, the same time, in fewer polls. */
static inline int bm_rac2_ticks(int n, const bm_tick* t) {
    return (int)((double)n / (60.0 * (double)t->ft) + 1.0 / 3.0 + 1e-4);
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

#endif /* RAC1_ENHANCED_MOVEMENT_RAC2_H */
