/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the mod uses of the game, on each disc it knows: the globals it reads and the game functions
 * it calls, by the name each disc's build of the port gives them (openrac_api.function).
 * - PAL v2.00 (SCES-50916, "pal"): rac1-decomp's own addresses and names (include/names/);
 *   OpenRAC's PAL build names functions as rac1-decomp does.
 * - NTSC-U v1.00 (SCUS-97199, "ntsc"): the same globals and functions at the US addresses, from
 *   OpenRAC's port/game/rac1-ntsc/host/rac1_version.h, rac1-decomp's config/names_ntsc.tsv and
 *   config/overlays/us_map.tsv, and the NTSC decomp's own C (DESIGN.md section 6).
 */
#ifndef RAC1_ENHANCED_MOVEMENT_DISCS_H
#define RAC1_ENHANCED_MOVEMENT_DISCS_H

#include <stdint.h>

typedef struct bm_disc {
    const char* version; /* openrac_api.version */

    /* The executable's, used as they are. */
    uint32_t hero; /* g_hero */
    uint32_t pad;  /* g_controller_state, the first pad's PadState */

    /* The executable's, through relocate_data (set_video_timing writes them). */
    uint32_t frame_scale_sq; /* g_frame_scale_sq: turn spring coefficients are scaled by it */
    uint32_t frame_time;     /* g_frame_time: seconds a tick */
    uint32_t frame_time_sq;  /* g_frame_time_sq */
    uint32_t frame_scale;    /* frame_scale: rac1's tick counts are scaled by it (scale_ticks) */
    uint32_t cam_option;     /* opt_cam_yaw_rate: the game's camera speed option, 0 to 2 */

    /* Level 0's program's, through relocate_level_data(0, ...). */
    uint32_t cinematic_mode; /* 0 in play; movies, scenes, the ship's entry otherwise */
    uint32_t camera_yaw;     /* the camera in use's yaw, which the stick is read against */
    uint32_t follow_cam;     /* g_current_follow_cam: the camera record in use (+0x86 its kind) */
    uint32_t blend_weight;   /* follow_cam_turn_blend_weight: the camera's turn to his heading */
    uint32_t pose_table;     /* hero_eases: the hero's joint pose records, 0xB0 bytes each */

    /* The follow camera's, level 0's program's too (DESIGN.md section 8). */
    uint32_t cam_gain; /* follow_cam_setting_value: the turn's gain, set from the table */

    /* The game's functions it calls: each is one function in every level (us_map.tsv). */
    const char* orbit_clamp;    /* void (int): set_follow_camera_orbit_clamp_mode */
    const char* start_slide;    /* void (void): follow_camera_start_slide */
    const char* lateral_spring; /* void (float, float): follow_camera_set_lateral_spring */
    const char* play_sound;     /* int (int sound, int flags, u32 moby): Moby_PlaySound */
} bm_disc;

static const bm_disc k_bm_discs[] = {
    {
        "pal",
        0x0013F450u, /* g_hero */
        0x0013CA40u, /* g_controller_state */
        0x0015EE64u, /* g_frame_scale_sq */
        0x0015EE6Cu, /* g_frame_time */
        0x0015EE70u, /* g_frame_time_sq */
        0x0015EE60u, /* frame_scale */
        0x0015EEE4u, /* opt_cam_yaw_rate (follow_camera_update's vendor_stock[5]) */
        0x0015F6A8u, /* D_L00_0015F6A8 cinematic_mode */
        0x00166ED8u, /* D_L00_00166ED8 camera_yaw */
        0x00166F00u, /* D_L00_00166F00 g_current_follow_cam */
        0x0015F040u, /* D_L00_0015F040 follow_cam_turn_blend_weight */
        0x0017A780u, /* D_L00_0017A780 hero_eases */
        0x0015F044u, /* D_L00_0015F044 follow_cam_setting_value */
        "func_L00_002E9A18",
        "func_L00_002E9AF8",
        "func_L00_002E9A40",
        "func_0022ED80",
    },
    {
        "ntsc",
        0x0013F350u,
        0x0013C940u,
        0x0015ED64u,
        0x0015ED6Cu,
        0x0015ED70u,
        0x0015ED60u,
        0x0015EDE4u, /* opt_cam_yaw_rate: the NTSC decomp's rnc/game/options.h */
        0x0015F5C4u,
        0x00166DD8u,
        0x00166E00u,
        0x0015EF40u,
        0x0017A680u,
        0x0015EF44u, /* the NTSC decomp's follow_camera_update (D_L00_0015EF44) */
        "FUN_L00_002e8568",
        "FUN_L00_002e8648",
        "FUN_L00_002e8590",
        "FUN_0022da68",
    },
};

#endif /* RAC1_ENHANCED_MOVEMENT_DISCS_H */
