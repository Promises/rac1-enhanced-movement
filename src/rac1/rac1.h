/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ratchet & Clank's own movement numbers that the mod compares with or replaces, in units a second
 * (rac1 writes them times g_frame_time or g_frame_time_sq). Each comes from rac1-decomp's C, named
 * beside it (PAL names; DESIGN.md sections 2 and 3).
 */
#ifndef RAC1_BETTER_MOVEMENT_RAC1_H
#define RAC1_BETTER_MOVEMENT_RAC1_H

/* hero_update_movement, the run: speed_step(7.5 * k, 8.5), and 17 down with the turnaround. */
#define RAC1_RUN_BRAKE 8.5f
#define RAC1_TURNAROUND_BRAKE 17.0f
/* The idle group's speed_step(0, ...): idle 12.6, STOP 12 * t, crouch 6.48 (ice 1.7). */
#define RAC1_IDLE_BRAKE 12.6f
#define RAC1_STOP_BRAKE 12.0f
#define RAC1_CROUCH_BRAKE 6.48f
/* HeroEnterMoveState's jump arm: air_stick_speed 5.7 by default, 3.5 in the double jump. */
#define RAC1_AIR_STICK_SPEED 5.7f
#define RAC1_DOUBLE_JUMP_AIR_SPEED 3.5f
/* The tick counts below are rac1's, before scale_ticks (bm_scale_ticks): more ticks on PAL. */
/* hero_update_transitions, the run: STOP when speed_xy is over 2.7, else idle after 25 ticks. */
#define RAC1_EXIT_STOP_SPEED 2.7f
#define RAC1_EXIT_IDLE_TICKS 25
/* hero_move_state_velocity, state 0xB: the drift eases to 4.2 by 8, or to 0 within 12 ticks of
 * landing past the apex. */
#define RAC1_FLIP_DRIFT 4.2f
#define RAC1_FLIP_DRIFT_STEP 8.0f
#define RAC1_FLIP_DRIFT_LANDING_TICKS 12
/* hero_update_transitions: X counts for a jump within 9 ticks in the run, 7 standing (1 with
 * jump_buffer_lockout). */
#define RAC1_RUN_JUMP_WINDOW 9
#define RAC1_IDLE_JUMP_WINDOW 7
/* hero_update_transitions, the run: the jump is taken only within 4 ticks off the ground. */
#define RAC1_RUN_JUMP_AIR_TICKS 4

/* Hero states (enums/hero_state.h). */
enum {
    RAC1_STATE_IDLE = 0x0,
    RAC1_STATE_LOOK = 0x1,
    RAC1_STATE_RUN = 0x2,
    RAC1_STATE_STOP = 0x3,
    RAC1_STATE_CROUCH = 0x4,
    RAC1_STATE_FALL = 0x6,
    RAC1_STATE_JUMP = 0x7,
    RAC1_STATE_RUN_JUMP = 0x9,
    RAC1_STATE_PACK_JUMP = 0xA,
    RAC1_STATE_HIGH_JUMP = 0xB,
    RAC1_STATE_DOUBLE_JUMP = 0xE,
    RAC1_STATE_PACK_LONG_JUMP = 0x10,
    RAC1_STATE_ITEM_BUTTON_RELEASE_WAIT = 0x17,
    RAC1_STATE_LOOK_RELEASE_WAIT = 0x1E,
    RAC1_STATE_RUN_WADING = 0x73,
    RAC1_STATE_OWN_STRAFE = 0x81,
};

/* Control modes (HeroState.control_mode) on foot, where the strafe may take L2/R2. */
enum {
    RAC1_MODE_IDLE = 0x0,
    RAC1_MODE_RUN = 0x1,
    RAC1_MODE_FALL = 0x2,
    RAC1_MODE_JUMP = 0x4,
    RAC1_MODE_LAND = 0x5,
    RAC1_MODE_ATTACK = 0x6,
    RAC1_MODE_ITEM_RELEASE_WAIT = 0x8,
    RAC1_MODE_CROUCH = 0xC,
};

#endif /* RAC1_BETTER_MOVEMENT_RAC1_H */
