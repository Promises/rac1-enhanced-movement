/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The numbers of the `clank` setting's `enhanced` choice, kept here to be tuned by hand after
 * playing. Units: speeds in u/s, accelerations in u/s^2, turn limits in rad/s, turn spring
 * coefficients per tick at 60 Hz, counts in ticks or polls at 60 Hz, and animation frames as
 * attack_records count them. clank.h scales each to the game's tick, so the same values serve PAL.
 *
 * Each value starts from Going Commando's Clank (rac2.h) where it improved on the first game's, and
 * goes past it by no more than the step Going Commando gave Ratchet in the same quantity (PLAN.md
 * section 11 lists each anchor). Keep changes small: the camera's old `modern` choice, at three
 * times the game's speed, played far too fast.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_CLANK_TUNE_H
#define RAC1_ENHANCED_MOVEMENT_CLANK_TUNE_H

/* His top speed at full stick, in u/s; a part stick gives the same share of it, as in both games.
 * 3.45 is 15% over both games' 3.0, chosen in play over 3.13 (Ratchet's own step, x 1.044), which
 * left long walks as slow as the first game's. It stays well under Ratchet's 5.95. */
#define BM_CLANK_TUNE_TOP_SPEED 3.45f

/* Acceleration and braking in the run, in u/s^2: rac2's 18 and 20 (rac1 7.5, cut while turning,
 * and 8.5), the same step Going Commando gave Ratchet's run. At 18 he reaches 3.45 u/s in about
 * 12 ticks; quicker would feel twitchy. */
#define BM_CLANK_TUNE_RUN_ACCEL 18.0f
#define BM_CLANK_TUNE_RUN_BRAKE 20.0f

/* The run's turn spring, rac2's: accel and damping per tick at 60 Hz, and the limit in rad/s. It
 * turns him 90 degrees in 11 ticks (rac1 29 at a run). A turnaround uses its own accel and limit,
 * also rac2's; the damping is shared. */
#define BM_CLANK_TUNE_TURN_ACCEL 0.033f
#define BM_CLANK_TUNE_TURN_DAMP 0.15f
#define BM_CLANK_TUNE_TURN_LIMIT 9.948377f
#define BM_CLANK_TUNE_TURNAROUND_ACCEL 0.037f
#define BM_CLANK_TUNE_TURNAROUND_LIMIT 11.693708f

/* Under this ground speed, in u/s, he moves along the stick as he turns, as he does in any
 * turnaround: rac2's 0.7. rac1 always moves him along his facing. */
#define BM_CLANK_TUNE_ALONG_SPEED 0.7f

/* The jump's takeoff, in ticks after X: rac2's 3 (rac1 5). The apex and landing frames of his
 * jump animation are rac2's 15 and 24 for that takeoff (rac1 18 and 27). Jump heights stay the
 * game's. */
#define BM_CLANK_TUNE_TAKEOFF 3
#define BM_CLANK_TUNE_APEX_FRAME 15.0f
#define BM_CLANK_TUNE_TARGET_FRAME 24.0f

/* The air stick speed in his jump, in u/s (rac1 and rac2 2.0, two thirds of his run). Going
 * Commando raised Ratchet's double jump from 61% to 80% of his run speed (3.5 to 4.75); 2.5 is 80%
 * of 3.13, Ratchet's step applied to Clank's top speed. It stays 2.5 at the top speed of 3.45 (72%)
 * so that his jumps reach no further than when they were play-tested. His glide and fall keep the
 * game's speeds, since Ratchet's heli hover is the same in both games. */
#define BM_CLANK_TUNE_JUMP_AIR_SPEED 2.5f

/* The punch's lock-on turn limit, in rad/s: rac2's 16.93 (rac1 15.01), the wrench's step. */
#define BM_CLANK_TUNE_PUNCH_TURN_LIMIT 16.929696f

/* How long Square is remembered, in polls at 60 Hz (scale_ticks lengthens them on PAL): 13 standing
 * or running (rac1 and rac2 9), and 7 landing from a jump (both 5). Going Commando lengthened
 * Ratchet's by the same ratios, 7 to 10 and 10 to 14. */
#define BM_CLANK_TUNE_SQUARE_WINDOW 13
#define BM_CLANK_TUNE_LAND_SQUARE_WINDOW 7

/* How many frames ahead of rac1's count his punch's ways out are read. rac2's punch reaches each
 * frame 2 ticks sooner (measured: it stands after 22 ticks and jumps out after 18, rac1 after 24
 * and 21), so at 1 he stands past frame 9 and jumps out past 7. The `rac2` choice uses this too. */
#define BM_CLANK_TUNE_PUNCH_LEAD 1.0f

/* Running out of a punch: with the stick past RUN_OUT_STICK (rac2's 0.8 for running out of a swing)
 * he leaves past frame RUN_OUT_FRAME. 7 is 1 frame after the hit (frames 3 to 6, which stay rac1's,
 * as does the lunge) and 4 ticks before rac2's punch lets him stand. Going Commando let Ratchet run
 * out of his swings 10 and 16 ticks sooner than the first game. */
#define BM_CLANK_TUNE_RUN_OUT_FRAME 7.0f
#define BM_CLANK_TUNE_RUN_OUT_STICK 0.8f

#endif /* RAC1_ENHANCED_MOVEMENT_CLANK_TUNE_H */
