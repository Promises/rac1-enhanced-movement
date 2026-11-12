/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the mod uses of the game, on each disc it knows: the globals it reads and the game functions
 * it calls, by the name each disc's build of the port gives them (openrac_api.function). Only
 * NTSC-U (SCUS-97199) is listed: the disc the mod is tested on (DESIGN.md section 5, which names
 * each row's source: OpenRAC's port/game/rac1-ntsc/host/rac1_version.h, rac1-decomp's
 * config/names_ntsc.tsv and config/overlays/us_map.tsv, the NTSC decomp's own C).
 */
#ifndef RAC1_BETTER_MOVEMENT_DISCS_H
#define RAC1_BETTER_MOVEMENT_DISCS_H

#include <stdint.h>

typedef struct bm_disc {
    const char* version; /* openrac_api.version */

    /* The executable's, used as they are. */
    uint32_t hero; /* g_hero (PAL 0x0013F450) */
    uint32_t pad;  /* g_controller_state, the first pad's PadState (PAL 0x0013CA40) */

    /* The executable's, through relocate_data. */
    uint32_t frame_scale_sq; /* g_frame_scale_sq: turn spring coefficients are scaled by it */
    uint32_t frame_time;     /* g_frame_time: seconds a tick */
    uint32_t frame_time_sq;  /* g_frame_time_sq */

    /* Level 0's program's, through relocate_level_data(0, ...). */
    uint32_t cinematic_mode; /* 0 in play; movies, scenes, the ship's entry otherwise */
    uint32_t camera_yaw;     /* the camera in use's yaw, which the stick is read against */
    uint32_t follow_cam;     /* g_current_follow_cam: the camera record in use (+0x86 its kind) */
    uint32_t blend_weight;   /* follow_cam_turn_blend_weight: the camera's turn to his heading */
    uint32_t pose_table;     /* hero_eases: the hero's joint pose records, 0xB0 bytes each */

    /* The game's functions it calls: each is one function in every level (us_map.tsv). */
    const char* orbit_clamp;    /* void (int): set_follow_camera_orbit_clamp_mode */
    const char* start_slide;    /* void (void): follow_camera_start_slide */
    const char* lateral_spring; /* void (float, float): follow_camera_set_lateral_spring */
    const char* play_sound;     /* int (int sound, int flags, u32 moby): Moby_PlaySound */
} bm_disc;

static const bm_disc k_bm_discs[] = {
    {
        "ntsc",
        0x0013F350u,
        0x0013C940u,
        0x0015ED64u,
        0x0015ED6Cu,
        0x0015ED70u,
        0x0015F5C4u,
        0x00166DD8u,
        0x00166E00u,
        0x0015EF40u,
        0x0017A680u,
        "FUN_L00_002e8568",
        "FUN_L00_002e8648",
        "FUN_L00_002e8590",
        "FUN_0022da68",
    },
};

#endif /* RAC1_BETTER_MOVEMENT_DISCS_H */
