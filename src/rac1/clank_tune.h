/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Every number the `clank` setting's `enhanced` choice plays with, in one place, to be adjusted after
 * playing it by hand. Units a second (u/s, u/s^2, rad/s), turn spring coefficients as at 60 Hz, counts
 * in polls or ticks at 60 Hz, animation frames as attack_records count them; clank.h scales each to
 * the game's tick as it scales rac2's.
 *
 * Each starts from Going Commando's Clank (rac2.h) where it improved on the first game's, and goes past
 * it only by the size of the step Going Commando itself took for Ratchet in the same quantity (PLAN.md
 * section 11 has each anchor). Raising one past its anchor is the kind of change the camera's `modern`
 * choice was (three times the game's speed), which played far too fast: keep steps moderate.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_CLANK_TUNE_H
#define RAC1_ENHANCED_MOVEMENT_CLANK_TUNE_H

/* His top speed at full stick, u/s (rac1 and rac2 3.0). 3.45, 15% over both games: the user's choice
 * (2026-11-14) over 3.13, Ratchet's own step (5.7 to 5.95, x 1.044) applied to Clank, which left him
 * as slow as the first game's on a long walk. Still well under Ratchet's 5.95. The speed still follows
 * the stick, as in both games. */
#define BM_CLANK_TUNE_TOP_SPEED 3.45f

/* Getting going and stopping in the run, u/s^2: rac2's 18 and 20 (rac1 7.5, cut while turning, and
 * 8.5), the same step Going Commando gave Ratchet's run. At 18 he reaches 3.45 in about 12 ticks: quicker
 * would be twitchy. */
#define BM_CLANK_TUNE_RUN_ACCEL 18.0f
#define BM_CLANK_TUNE_RUN_BRAKE 20.0f

/* The run's turn, rac2's: accel / damping per tick at 60 Hz and the limit in rad/s, 90 degrees in 11
 * ticks (rac1 29 at a run); the turnaround's. Under 0.7 u/s, or in a turnaround, he moves along the
 * stick as he turns (rac1 never). */
#define BM_CLANK_TUNE_TURN_ACCEL 0.033f
#define BM_CLANK_TUNE_TURN_DAMP 0.15f
#define BM_CLANK_TUNE_TURN_LIMIT 9.948377f
#define BM_CLANK_TUNE_TURNAROUND_ACCEL 0.037f
#define BM_CLANK_TUNE_TURNAROUND_LIMIT 11.693708f
#define BM_CLANK_TUNE_ALONG_SPEED 0.7f

/* The jump: off the ground after rac2's 3 ticks (rac1 5), with rac2's animation frames for it (apex 15,
 * landing 24). Heights stay the game's. */
#define BM_CLANK_TUNE_TAKEOFF 3
#define BM_CLANK_TUNE_APEX_FRAME 15.0f
#define BM_CLANK_TUNE_TARGET_FRAME 24.0f

/* The stick's speed in his jump, u/s (rac1 and rac2 2.0, two thirds of his run). Going Commando raised
 * Ratchet's double jump from 61% of his run speed to 80% (3.5 to 4.75): Clank's jump goes from 67% of
 * his run speed to 80% of 3.13 (Ratchet's step applied to Clank's top speed), 2.5. It stays 2.5 with
 * the top speed raised to 3.45, so his jumps reach no further than they were played (72% of it). His
 * glide and fall keep the game's (Ratchet's heli hover is the same in both games). */
#define BM_CLANK_TUNE_JUMP_AIR_SPEED 2.5f

/* The punch's lock-on turn, rad/s: rac2's 16.93 (rac1 15.01), the wrench's step. */
#define BM_CLANK_TUNE_PUNCH_TURN_LIMIT 16.929696f

/* Square remembered, polls before rac1's scale_ticks: standing or running 13 (rac1 and rac2 9), landing
 * from a jump 7 (5). Going Commando remembered Ratchet's Square 10/7 and 14/10 as long. */
#define BM_CLANK_TUNE_SQUARE_WINDOW 13
#define BM_CLANK_TUNE_LAND_SQUARE_WINDOW 7

/* Leaving a punch. Going Commando's punch reaches each of its frames 2 ticks sooner than the first
 * game's (measured: it stands after 22 ticks and jumps out after 18, rac1 24 and 21), so its way out
 * reads one frame ahead of rac1's frame count: standing past 9, jumping out past 7. The hits and the
 * lunge stay rac1's (frames 3 to 6; the reach). With the stick past rac2's 0.8 (its run out of a
 * swing) he also runs out from where he can jump out, past frame 7: 1 frame after the hit, 4 ticks
 * before rac2's punch lets him stand (Going Commando ran Ratchet out of his swings 10 and 16 ticks
 * sooner than the first game). */
#define BM_CLANK_TUNE_PUNCH_LEAD 1.0f
#define BM_CLANK_TUNE_RUN_OUT_FRAME 7.0f
#define BM_CLANK_TUNE_RUN_OUT_STICK 0.8f

#endif /* RAC1_ENHANCED_MOVEMENT_CLANK_TUNE_H */
