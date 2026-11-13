/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The air control's decision (src/rac1/air.h): rac1's default air stick speeds become rac2's on a
 * jump's first tick, every other value stays.
 */
#include "check.h"
#include "rac1/air.h"

int main(void) {
    const bm_tick ntsc = bm_tick_ntsc();
    const bm_tick pal = bm_tick_pal();

    // The plain jump at NTSC: 5.7 / 60 = 0.095 becomes 0.099166669 (5.95 / 60).
    CHECK_NEAR(bm_air_stick_speed(0x07, 0.095f, &ntsc), 0.099166669, TOL);

    // The running jump, the same.
    CHECK_NEAR(bm_air_stick_speed(0x09, 0.095f, &ntsc), 0.099166669, TOL);

    // The double jump: 3.5 / 60 = 0.0583333 becomes 0.079166673 (4.75 / 60).
    CHECK_NEAR(bm_air_stick_speed(0x0E, 3.5f / 60.0f, &ntsc), 0.079166673, TOL);

    // A value rac2 shares (1.9 in state 0xD, 3.3 in 0xF) stays.
    CHECK_NEAR(bm_air_stick_speed(0x0D, 1.9f / 60.0f, &ntsc), 1.9 / 60.0, TOL);
    CHECK_NEAR(bm_air_stick_speed(0x0F, 3.3f / 60.0f, &ntsc), 3.3 / 60.0, TOL);

    // PAL: 5.7 / 50 = 0.114 becomes 0.099166669 x 60 / 50 = 0.119.
    CHECK_NEAR(bm_air_stick_speed(0x07, 0.114f, &pal), 0.119, TOL);

    // The jump group's states.
    CHECK_EQ(bm_is_jump_state(0x07), 1);
    CHECK_EQ(bm_is_jump_state(0x0E), 1);
    CHECK_EQ(bm_is_jump_state(0x69), 1);
    CHECK_EQ(bm_is_jump_state(0x02), 0);
    CHECK_EQ(bm_is_jump_state(0x08), 0);

    CHECK_DONE("air");
}
