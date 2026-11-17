/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Going Commando's movement numbers as rac2-decomp writes them (USA v1.01, level 0's shared hero
 * code; DESIGN.md gives each one's role). They are for rac2's 60 Hz tick: speeds per tick,
 * accelerations per tick squared, turn accel and damping per tick, and turn limits in radians a
 * tick. The rac2_* helpers at the end convert them to this tick's units as rac1 scales its own.
 *
 * Each RAC2_ literal is also a row of tests/data/rac2_constants.tsv. tools/rac2_constants.py reads
 * the rows out of rac2-decomp with their file and line, or out of level 0's data for the run speed,
 * the follow camera and the attack records, and tests/rac2_constants_test.py checks that the two
 * agree. Counts of ticks and polls are integers; where the mod scales them to the tick it uses
 * bm_rac2_ticks, as rac2's PAL release counts them.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_RAC2_H
#define RAC1_ENHANCED_MOVEMENT_RAC2_H

/* ---- The run ---- */

/* The run speed in u/s, times the stick: level 0 data D_001A7DB0[4], read by func_L00_002B3D10. */
#define RAC2_RUN_SPEED 5.95f
/* The wading scale and its floor per tick (func_L00_002B3D10), and the stick under which the run
 * asks for no speed (func_L00_002BA7D8 case 2, as for the rest of this section). */
#define RAC2_WADING_SCALE 0.8f
#define RAC2_WADING_MIN 0.041666668f
#define RAC2_STICK_RELEASE 0.2f
/* The speed step: acceleration and braking per tick squared, k's stick and angle, the slow stick's
 * cut, and the turnaround's cap, scale and braking. */
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
/* The along and turnaround flags: the speed under which he moves along the stick, the angles and
 * stick swing that start a turnaround, and the angles that clear each flag. */
#define RAC2_ALONG_SPEED 0.028333336f
#define RAC2_TURNAROUND_SLOW_ANGLE 2.0943954f
#define RAC2_TURNAROUND_FLICK_ANGLE 1.9198624f
#define RAC2_TURNAROUND_FLICK_ERROR 1.2217306f
#define RAC2_ALONG_CLEAR 0.087266475f
#define RAC2_TURNAROUND_CLEAR 0.06981318f
/* The run band's turn spring. */
#define RAC2_RUNBAND_TURN_ACCEL 0.02f
#define RAC2_RUNBAND_TURN_DAMP 0.17f
#define RAC2_RUNBAND_TURN_LIMIT 0.16580631f

/* ---- Braking ---- */

/* Braking per tick squared in the idle group, STOP, the crouch, and the crouch on ice
 * (func_L00_002BA7D8 cases 0, 1, 3, 4 and the rest of the idle group). */
#define RAC2_IDLE_BRAKE 0.004916667f
#define RAC2_STOP_BRAKE 0.0075000003f
#define RAC2_CROUCH_BRAKE 0.0075000003f
#define RAC2_CROUCH_ICE_BRAKE 0.00047222225f

/* ---- Walk, turnaround and firing turns ---- */

/* The turn springs of func_L00_002B3F68: the walk band's, which grow with the stick, the
 * turnaround's, and the slower turn while firing a hold-to-fire weapon. */
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

/* ---- Leaving the run ---- */

/* The stick under which the run ends, the angle a strafing turn must be within, the speed he is
 * clamped to on leaving (per tick), and the stick past which X gives the running jump
 * (func_L00_002CB3A8 case 2). */
#define RAC2_EXIT_STICK 0.17f
#define RAC2_EXIT_TURNED 0.03490659f
#define RAC2_EXIT_CLAMP 0.025000002f
#define RAC2_RUN_JUMP_STICK 0.85f

/* ---- Jumps ---- */

/* The air stick speeds per tick a jump sets on entry, and the flip's starting drift (the jump arm
 * of func_L00_002C42E0). */
#define RAC2_AIR_STICK_SPEED 0.099166668951511383f
#define RAC2_DOUBLE_JUMP_AIR_SPEED 0.079166673123836517f
#define RAC2_FLIP_DRIFT_START 0.083333335816860199f

/* ---- The strafe ---- */

/* The strafe's turn spring and side-change factor, its stick sectors, backpedal and standing start,
 * and the flip and its drift (func_L00_002B35A0, func_L00_002B39F0, func_L00_002CB3A8,
 * func_L00_002C9FC8 and func_L00_002B6BC8). */
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

/* The side timers' length in rac2 ticks (func_L00_002B35A0 stores 5). */
#define RAC2_STRAFE_SIDE_TICKS 5

/* ---- The hip twist ---- */

/* The stick past which the twist applies, and the hip and body springs' accel and damping per tick
 * (func_L00_002B7B80). */
#define RAC2_TWIST_STICK 0.15f
#define RAC2_TWIST_HIP_ACCEL 0.037f
#define RAC2_TWIST_HIP_DAMP 0.27f
#define RAC2_TWIST_BODY_ACCEL 0.025f
#define RAC2_TWIST_BODY_DAMP 0.27f

/* ---- The wrench ---- */

/* The lock-on turn of the swings and the slam (func_L00_002BA7D8 cases 0x13 and 0x14) and the
 * throw's accel (case 0x15), DESIGN.md section 5. rac1 uses 0.05 / 0.2 / 15.01 rad/s for each. */
#define RAC2_WRENCH_TURN_ACCEL 0.05f
#define RAC2_WRENCH_TURN_DAMP 0.2f
#define RAC2_WRENCH_TURN_LIMIT 0.2821616f
#define RAC2_THROW_TURN_ACCEL 0.057f

/* Leaving a swing (func_L00_002CB3A8 cases 0x13 and 0x15): past frame 34 a stick past 0.9 runs.
 * Past the record's exit frame a stick past 0.8 runs; otherwise he stands 5 frames later or when
 * the animation ends. rac1 stands at the exit frame. */
#define RAC2_THROW_RUN_FRAME 34.0f
#define RAC2_THROW_RUN_STICK 0.9f
#define RAC2_RUN_OUT_STICK 0.8f
#define RAC2_EXIT_IDLE_FRAMES 5

/* Square's windows in polls (func_L00_002C9448): 10 standing or running (rac1 7), 14 in a jump
 * (rac1 10), 18 in the high and double jumps (both games), and (frame - first) * 2 up to 15 in a
 * swing, as in rac1. The throw repeats from its 31st tick (rac1 after its 30th). X's window in the
 * throw is 8 polls from frame 35 (func_L00_002CB3A8 case 0x15); rac1's is (frame - 83) * 2 from
 * frame 88, after the catch. */
#define RAC2_WRENCH_WINDOW 10
#define RAC2_WRENCH_AIR_WINDOW 14
#define RAC2_WRENCH_AIR_LATE_WINDOW 18
#define RAC2_CHAIN_WINDOW 15
#define RAC2_THROW_AGAIN_TICKS 31
#define RAC2_THROW_JUMP_WINDOW 8

/* The slam (func_L00_002BA7D8 case 0x14): more than 4 units above the ground it drives forward at
 * 4.5 u/s, for 70 ticks without a target. rac1 gives that speed only toward a target 2 to 5 units
 * away. */
#define RAC2_SLAM_DRIVE 0.075f
#define RAC2_SLAM_DRIVE_HEIGHT 4.0f
#define RAC2_SLAM_DRIVE_TICKS 0x46

/* The attack records (level 0 data D_L00_0025D990, 0x2C bytes each; at 0x25D910 on the v2.00 disc),
 * with the frames at +0xC, +0x10, +0x14 and +0x18 as in rac1's attack_records (rac1.h). Records 0
 * to 2 are the combo's swings and 3 the throw. Against rac1's: the first swing chains from frame 25
 * (rac1 26) and ends at 26 (31), the second ends at 15 (23), and the throw allows a jump after
 * frame 35 (87). */
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

/* ---- The follow camera ---- */

/* The camera speed table, 1.0, 1.3 and 2.2 degrees a tick against rac1's 1.0, 1.3 and 1.6: the
 * games differ only in the fastest step (DESIGN.md section 8). It is level 0 data D_L00_001B15B8,
 * at gp + 0x2568 = 0x1B1558 on the v2.00 disc. func_L00_004263B8 (follow_camera_update) reads the
 * entry for the camera speed option (D_001A7BAC, 1 by default) into the gain that
 * func_L00_00424F98 (follow_camera_read_stick) gives the turn. */
#define RAC2_CAMERA_SPEED_SLOW 0.017453294f
#define RAC2_CAMERA_SPEED_MEDIUM 0.022689281f
#define RAC2_CAMERA_SPEED_FAST 0.03839725f
/* The rest of the turn, number for number rac1's. func_L00_00425118 (follow_camera_turn_and_pitch)
 * aims the pitch with steps of 0.02 out, 0.01 back and 1.75 degrees a tick (0x1B1458, 0x1B145C,
 * 0x1B1460), and func_L00_00425CD0 (follow_camera_orbit) eases the eye's yaw and pitch at
 * 0.015 / 0.2 (0x1B1488 to 0x1B1494). Only camera_test.c uses these, to show that nothing else
 * needs porting. */
#define RAC2_CAMERA_PITCH_TRACKING 0.02f
#define RAC2_CAMERA_PITCH_RETURN 0.01f
#define RAC2_CAMERA_PITCH_STEP 0.030543264f
#define RAC2_CAMERA_YAW_SPRING_ACCEL 0.015f
#define RAC2_CAMERA_YAW_SPRING_DAMP 0.2f
#define RAC2_CAMERA_PITCH_SPRING_ACCEL 0.015f
#define RAC2_CAMERA_PITCH_SPRING_DAMP 0.2f

/* ---- Clank ---- */

/* His run (func_L00_002BA7D8 cases 0x40 and 0x4C; rac1's 0x44 and 0x50; DESIGN.md section 9).
 * The speed is the stick times 3 u/s, as in rac1 (func_L00_002B39F0(0.05)). rac2 turns at
 * 0.033 / 0.15 / 9.95 rad/s at any stick and speed (0.037 / 0.15 / 11.69 in a turnaround), and
 * speeds up at 18 and slows at 20 u/s^2 (rac1 7.5, cut by the turn, and 8.5). */
#define RAC2_CLANK_RUN_SPEED 0.050000004f
#define RAC2_CLANK_TURN_ACCEL 0.033f
#define RAC2_CLANK_TURN_DAMP 0.15f
#define RAC2_CLANK_TURN_LIMIT 0.16580631f
#define RAC2_CLANK_TURNAROUND_ACCEL 0.037f
#define RAC2_CLANK_TURNAROUND_LIMIT 0.19489513f
#define RAC2_CLANK_RUN_ACCEL 0.005f
#define RAC2_CLANK_RUN_BRAKE 0.0055555557f
/* His run flags. Along the stick (+0x40C) is set under 0.7 u/s, and a turnaround (+0x40F) there
 * when more than 100 degrees off with the stick past 0.8. A turnaround also starts when the stick's
 * angles 0 polls back (the ring's oldest, 30 polls ago) and 2 back are more than 150 degrees apart,
 * 70 degrees off, with the stick past 0.8. Along clears under 5 degrees, the turnaround under 4.
 * rac2's cap on the turnaround's speed is written after the speed step and overwritten by the next
 * tick's stick, so it changes nothing. */
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

/* Numbers both games share, which clank_test.c checks against rac1.h: idle's braking 12.6 (case
 * 0x3F), the punch's speed step 37 / 28 (0x4D), and the glide's stick speed 2 and turn limit 8.73
 * (0x4B). The punch's lock-on turn is the wrench's, 16.93 rad/s (rac1 15.01 for both). */
#define RAC2_CLANK_IDLE_BRAKE 0.0035f
#define RAC2_CLANK_PUNCH_TURN_LIMIT 0.2821616f
#define RAC2_CLANK_PUNCH_ACCEL 0.010277778f
#define RAC2_CLANK_PUNCH_BRAKE 0.007777778f
#define RAC2_CLANK_GLIDE_SPEED 0.033333335f
#define RAC2_CLANK_GLIDE_TURN_LIMIT 0.14544412f

/* His jump's entry values (func_L00_002C42E0 state 0x45; rac1's 0x49): off the ground after 3 ticks
 * (rac1 5), with the animation's apex and landing at frames 15 and 24 (rac1 18 and 27). The heights
 * 1.3 / 1.35, gravity 25 and air stick speed 2 are rac1's. rac2's PAL release counts the takeoff as
 * 2 ticks. */
#define RAC2_CLANK_JUMP_HEIGHT 1.3f
#define RAC2_CLANK_JUMP_HEIGHT_HELD 1.35f
#define RAC2_CLANK_TAKEOFF 3
#define RAC2_CLANK_APEX_FRAME 15.0f
#define RAC2_CLANK_TARGET_FRAME 24.0f
#define RAC2_CLANK_JUMP_GRAVITY 0.0069444445f
#define RAC2_CLANK_AIR_STICK_SPEED 0.033333335f

/* His windows in polls (func_L00_002CB3A8 cases 0x3F, 0x40 and 0x45): X 8 and Square 9 standing or
 * running, and Square 5 landing from a jump, all as in rac1. */
#define RAC2_CLANK_JUMP_WINDOW 8
#define RAC2_CLANK_SQUARE_WINDOW 9
#define RAC2_CLANK_LAND_SQUARE_WINDOW 5

/* His punches, attack records 7 and 8 (level 0 data D_L00_0025D990, like the wrench's): the next
 * punch chains under frame 16, the windows count from frame 3, Square chains from 12, he jumps out
 * after 8 and stands after 10, and the punch hits from frame 3 to 6. rac1's level data is the same
 * (DESIGN.md 9.1). */
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

/* ---- This tick's units ---- */

/* The tick rac1 runs at, as set_video_timing sets it: 1/60, 1/3600, 1 and 1 on NTSC, and 1/50,
 * 1/2500, 1.44 and 1.2 on PAL. */
typedef struct bm_tick {
    float ft;  /* g_frame_time, seconds a tick */
    float ft2; /* g_frame_time_sq */
    float fs2; /* g_frame_scale_sq */
    float fs;  /* frame_scale */
} bm_tick;

/* Converts a rac2 speed per 60 Hz tick to this tick's units. */
static inline float rac2_speed(float per_tick, const bm_tick* t) {
    return per_tick * 60.0f * t->ft;
}

/* Converts a rac2 acceleration per 60 Hz tick squared to this tick's units. */
static inline float rac2_accel(float per_tick2, const bm_tick* t) {
    return per_tick2 * 3600.0f * t->ft2;
}

/* Converts a rac2 turn limit in radians per 60 Hz tick to this tick's units. */
static inline float rac2_turn_limit(float per_tick, const bm_tick* t) {
    return per_tick * 60.0f * t->ft;
}

/* Converts a rac2 turn spring coefficient (accel or damping) to this tick's units, as rac1 scales
 * its own. */
static inline float rac2_spring(float coefficient, const bm_tick* t) {
    return coefficient * t->fs2;
}

/* Scales a count of ticks as rac1's scale_ticks (func_001F9850) does: n * frame_scale + 0.5,
 * truncated. The mod counts the rac1 windows it mirrors this way (the jump's, the run's exit).
 * rac2's PAL release does not scale its counts so (rac2-decomp src/pal, L00_002B3210.c); those use
 * bm_rac2_ticks. */
static inline int bm_scale_ticks(int n, const bm_tick* t) {
    return (int)(0.5f + (float)n * t->fs);
}

/* Scales a count of rac2's polls or ticks to this tick as rac2's PAL release counts the wrench's:
 * (n * 5 + 2) / 6 at 50 Hz and n at 60, with 1e-4 against float rounding. Its PAL deciders use 7
 * for the throw's window of 8 and 12 for the crouch's Square window of 15 (rac2-decomp
 * src/pal/levels/03_endako/L03_002BF860-sw.c). Unlike scale_ticks, this keeps the same time in
 * fewer polls. */
static inline int bm_rac2_ticks(int n, const bm_tick* t) {
    return (int)((double)n / (60.0 * (double)t->ft) + 1.0 / 3.0 + 1e-4);
}

/* Returns the NTSC tick (60 Hz) as rac1 sets it. */
static inline bm_tick bm_tick_ntsc(void) {
    bm_tick t = {1.0f / 60.0f, 1.0f / 3600.0f, 1.0f, 1.0f};
    return t;
}

/* Returns the PAL tick (50 Hz) as rac1 sets it. */
static inline bm_tick bm_tick_pal(void) {
    bm_tick t = {1.0f / 50.0f, 1.0f / 2500.0f, 1.44f, 1.2f};
    return t;
}

#endif /* RAC1_ENHANCED_MOVEMENT_RAC2_H */
