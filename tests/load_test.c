/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The built library loaded the way the port loads it (GUIDE.md section 12), against a host table of
 * the test's own: its query, its refusals, its hooks and their signatures, and each hook's work on
 * game memory laid out at each of src/rac1/discs.h's rows' addresses, with the game's own code
 * standing in as counting stubs.
 *
 *     rac1_enhanced_movement_load_test <path to the library>
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define LIB_OPEN(p) ((void*)LoadLibraryA(p))
#define LIB_SYM(h, n) ((void*)GetProcAddress((HMODULE)(h), (n)))
#else
#include <dlfcn.h>
#define LIB_OPEN(p) dlopen((p), RTLD_NOW | RTLD_LOCAL)
#define LIB_SYM(h, n) dlsym((h), (n))
#endif

#include "check.h"
#include "openrac/games/rac1.h"
#include "openrac/openrac_mod.h"
#include "rac1/discs.h"
#include "rac1/hero.h"
#include "version.h"

/* ---- The game's memory and the host's state -------------------------------------------- */

#define MEMORY_SIZE 0x00400000u
static uint8_t* g_mem;
static uint8_t* g_before; /* a copy of it, to see what a hook wrote */

static const char* g_game = "rac1";
static const char* g_version = "ntsc";
static int g_level = 0;
static char g_fail[512];
static char g_last_log[1024];
static int g_warned;

/* The settings the next load reads. */
static int s_ground, s_air, s_flips, s_twist, s_wrench, s_trace;
static const char* s_strafe;
static const char* s_camera;
static const char* s_clank;
static int g_no_approach; /* the port lacks rac1.player.approach_speed */

/* The NTSC row. */
static const bm_disc* D;

/* Where the test puts the rest of the game. */
#define CAM_RECORD 0x00300000u
#define MOBY 0x00310000u
#define CLASS 0x00320000u
#define SEQ_RECORD 0x00330000u
#define FRAMES 0x00340000u
#define CAMERA_MOBY 0x00350000u
#define CAMERA_VARS 0x00360000u

static float getf(uint32_t a) {
    float v;
    memcpy(&v, g_mem + a, 4);
    return v;
}

static void putf(uint32_t a, float v) {
    memcpy(g_mem + a, &v, 4);
}

static uint32_t getu(uint32_t a) {
    uint32_t v;
    memcpy(&v, g_mem + a, 4);
    return v;
}

static void putu(uint32_t a, uint32_t v) {
    memcpy(g_mem + a, &v, 4);
}

static void puts16(uint32_t a, int16_t v) {
    memcpy(g_mem + a, &v, 2);
}

static int16_t gets16(uint32_t a) {
    int16_t v;
    memcpy(&v, g_mem + a, 2);
    return v;
}

#define H(off) (D->hero + (off))
#define P(off) (D->pad + (off))

/* A hero in play at NTSC's tick: a level, his moby, no scene, on foot, the follow camera. */
static void reset_game(void) {
    memset(g_mem, 0, MEMORY_SIZE);
    g_level = 0;
    putf(D->frame_time, 1.0f / 60.0f);
    putf(D->frame_time_sq, 1.0f / 3600.0f);
    putf(D->frame_scale_sq, 1.0f);
    putf(D->frame_scale, 1.0f);
    putu(H(BM_HERO_MOBY), MOBY);
    putu(D->follow_cam, CAM_RECORD);
    puts16(CAM_RECORD + BM_CAMERA_KIND, 0);
    putu(P(BM_PAD_HIST_COUNT), 30);
    puts16(P(BM_PAD_HIST_IDX), 5);
    putu(H(BM_HERO_ANIM_BLEND_CURVE), (uint32_t)-1);
    putu(H(BM_HERO_ANIM_LOOP_FRAME), (uint32_t)-1);
    putu(CAMERA_MOBY + BM_CAMERA_VARS, CAMERA_VARS);

    // The follow camera's numbers as the game has them: its middle speed step.
    putu(D->cam_option, 1);
    putf(D->cam_gain, 0.02268928f);
}

/* How many bytes of the game's memory differ from g_before, besides the word at `skip`. */
static int changed_besides(uint32_t skip) {
    int n = 0;

    // Every byte.
    for (uint32_t i = 0; i < MEMORY_SIZE; i++) {
        // Another word's, changed.
        if ((i < skip || i >= skip + 4) && g_mem[i] != g_before[i]) {
            n++;
        }
    }

    return n;
}

/* ---- The hooks the mod takes, and the game's code behind each -------------------------- */

typedef struct taken {
    const char* id;
    const char* alias;
    const char* signature;
    void* replacement;
    void* original;
} taken;

static int g_turn_calls, g_approach_calls, g_planar_calls, g_update_calls, g_advance_calls,
    g_pad_calls, g_camera_calls;
static float g_turn_args[3], g_turn_move_yaw, g_approach_args[2], g_approach_target, g_planar_angle,
    g_advance_speed, g_advance_frame;
static int32_t g_advance_changing;
static float g_turn_error = 0.25f; /* the yaw error the game's turn leaves */
static void (*g_inside)(void);     /* what the state machine does inside hero_update */

static void orig_pad(uint32_t p, uint32_t r, int32_t n) {
    (void)p;
    (void)r;
    (void)n;
    g_pad_calls++;
}

static void orig_update(void) {
    g_update_calls++;

    // The state machine's calls the test stands in for.
    if (g_inside != NULL) {
        g_inside();
    }
}

static void orig_turn(int32_t m, float a, float d, float l) {
    (void)m;
    g_turn_calls++;
    g_turn_args[0] = a;
    g_turn_args[1] = d;
    g_turn_args[2] = l;
    g_turn_move_yaw = getf(H(BM_HERO_MOVE_YAW));
    putf(H(BM_HERO_YAW_RATE), 0.5f); /* the game's turn moves the yaw rate */
    putf(H(BM_HERO_YAW_ERROR), g_turn_error);
}

static void orig_planar(float a) {
    g_planar_calls++;
    g_planar_angle = a;
}

static void orig_camera(uint32_t c) {
    (void)c;
    g_camera_calls++;
    putf(D->blend_weight, 0.75f); /* the recentring writes its weight */
    puts16(CAMERA_VARS + 0x12, 7);
}

static void orig_advance(void) {
    g_advance_calls++;
    g_advance_speed = getf(H(BM_HERO_ANIM_SPEED));
    g_advance_frame = getf(H(BM_HERO_ANIM_FRAME));
    g_advance_changing = (int32_t)getu(H(BM_HERO_ANIM_CHANGING));
}

static void orig_approach(float up, float down) {
    g_approach_calls++;
    g_approach_args[0] = up;
    g_approach_args[1] = down;
    g_approach_target = getf(H(BM_HERO_TARGET_SPEED));
}

static taken g_hooks[] = {
    {"rac1.pad.process_report",
     OPENRAC_RAC1_HOOK_Pad_ProcessReport,
     OPENRAC_RAC1_SIG_Pad_ProcessReport,
     NULL,
     (void*)orig_pad},
    {"rac1.player.update_motion_input",
     OPENRAC_RAC1_HOOK_Hero_UpdateMotionInput,
     OPENRAC_RAC1_SIG_Hero_UpdateMotionInput,
     NULL,
     (void*)orig_update},
    {"rac1.player.turn_to_heading",
     OPENRAC_RAC1_HOOK_Hero_TurnToHeading,
     OPENRAC_RAC1_SIG_Hero_TurnToHeading,
     NULL,
     (void*)orig_turn},
    {"rac1.player.set_planar_velocity",
     OPENRAC_RAC1_HOOK_Hero_SetPlanarVelocity,
     OPENRAC_RAC1_SIG_Hero_SetPlanarVelocity,
     NULL,
     (void*)orig_planar},
    {"rac1.camera.follow.apply_overrides",
     OPENRAC_RAC1_HOOK_FollowCamera_ApplyOverrides,
     OPENRAC_RAC1_SIG_FollowCamera_ApplyOverrides,
     NULL,
     (void*)orig_camera},
    {"rac1.player.advance_animation",
     OPENRAC_RAC1_HOOK_Hero_AdvanceAnimation,
     OPENRAC_RAC1_SIG_Hero_AdvanceAnimation,
     NULL,
     (void*)orig_advance},
    {"rac1.player.approach_speed",
     OPENRAC_RAC1_HOOK_Hero_ApproachSpeed,
     OPENRAC_RAC1_SIG_Hero_ApproachSpeed,
     NULL,
     (void*)orig_approach},
};
#define HOOK_COUNT ((int)(sizeof g_hooks / sizeof g_hooks[0]))

enum {
    PAD,
    UPDATE,
    TURN,
    PLANAR,
    CAMERA,
    ADVANCE,
    APPROACH
};

static int g_hooks_taken;

static int fake_hook(
    openrac_mod* mod, const char* name, const char* signature, void* replacement, void** original
) {
    (void)mod;

    // The hook point asked for, by id or alias, as the port's loader takes either.
    for (int i = 0; i < HOOK_COUNT; i++) {
        // This one.
        if (strcmp(name, g_hooks[i].id) == 0 || strcmp(name, g_hooks[i].alias) == 0) {
            // A port older than approach_speed, for the test that asks.
            if (i == APPROACH && g_no_approach) {
                return OPENRAC_HOOK_UNKNOWN;
            }

            // The SDK's signature, or a refusal as the port's.
            if (strcmp(signature, g_hooks[i].signature) != 0) {
                return OPENRAC_HOOK_SIGNATURE;
            }

            g_hooks[i].replacement = replacement;
            *original = g_hooks[i].original;
            g_hooks_taken++;
            return OPENRAC_HOOK_OK;
        }
    }

    return OPENRAC_HOOK_UNKNOWN;
}

/* ---- The game functions the mod calls --------------------------------------------------- */

static int g_orbit_calls, g_slide_calls, g_spring_calls, g_sound_calls, g_orbit_mode, g_sound_id;
static float g_spring_args[2];

static void fn_orbit(int32_t mode) {
    g_orbit_calls++;
    g_orbit_mode = mode;
}

static void fn_slide(void) {
    g_slide_calls++;
}

static void fn_spring(float a, float b) {
    g_spring_calls++;
    g_spring_args[0] = a;
    g_spring_args[1] = b;
}

static int32_t fn_sound(int32_t id, int32_t flags, uint32_t moby) {
    (void)flags;
    (void)moby;
    g_sound_calls++;
    g_sound_id = id;
    return 0;
}

/* A game function by this disc's name: the stubs above, or none. */
static void* fake_function(const char* name) {
    const struct {
        const char* name;
        void* fn;
    } fns[] = {
        {D->orbit_clamp, (void*)fn_orbit},
        {D->start_slide, (void*)fn_slide},
        {D->lateral_spring, (void*)fn_spring},
        {D->play_sound, (void*)fn_sound},
    };

    // The one asked for.
    for (int i = 0; i < 4; i++) {
        // Its name, as the port's NTSC build has it.
        if (strcmp(name, fns[i].name) == 0) {
            return fns[i].fn;
        }
    }

    return NULL;
}

/* ---- The rest of the host table --------------------------------------------------------- */

static void fake_log(openrac_mod* mod, int level, const char* text) {
    (void)mod;
    snprintf(g_last_log, sizeof g_last_log, "%s", text);

    // Warnings and errors are counted.
    if (level >= OPENRAC_LOG_WARN) {
        g_warned++;
    }
}

static void fake_fail(openrac_mod* mod, const char* reason) {
    (void)mod;
    snprintf(g_fail, sizeof g_fail, "%s", reason);
}

static int fake_bool(openrac_mod* mod, const char* key) {
    const struct {
        const char* key;
        const int* value;
    } settings[] = {
        {"ground", &s_ground},
        {"air", &s_air},
        {"flips", &s_flips},
        {"twist", &s_twist},
        {"wrench", &s_wrench},
        {"trace", &s_trace},
    };

    (void)mod;

    // The setting asked for; 0 for a key the manifest has not.
    for (int i = 0; i < 6; i++) {
        // This one.
        if (strcmp(key, settings[i].key) == 0) {
            return *settings[i].value;
        }
    }

    return 0;
}

static int64_t fake_int(openrac_mod* mod, const char* key) {
    (void)mod;
    (void)key;
    return 0;
}

static double fake_float(openrac_mod* mod, const char* key) {
    (void)mod;
    (void)key;
    return 0.0;
}

static const char* fake_string(openrac_mod* mod, const char* key) {
    (void)mod;

    // The camera's choice.
    if (strcmp(key, "camera") == 0) {
        return s_camera;
    }

    // Clank's.
    if (strcmp(key, "clank") == 0) {
        return s_clank;
    }

    return strcmp(key, "strafe") == 0 ? s_strafe : "";
}

static openrac_gaddr identity(openrac_gaddr a) {
    return a;
}

static openrac_gaddr identity_level(int level, openrac_gaddr a) {
    (void)level;
    return a;
}

static int fake_loaded_level(void) {
    return g_level;
}

typedef void (*query_fn)(openrac_mod_info*);
typedef int (*load_fn)(const openrac_api*, openrac_mod*);
static load_fn g_load;

/* Loads the mod with the current settings; the hooks it took are in g_hooks. */
static int load_mod(void) {
    static openrac_api api;
    memset(&api, 0, sizeof api);
    api.size = sizeof api;
    api.api_major = OPENRAC_MOD_API_MAJOR;
    api.api_minor = OPENRAC_MOD_API_MINOR;
    api.game = g_game;
    api.version = g_version;
    api.openrac_version = "test";
    api.platform = "test";
    api.memory = g_mem;
    api.log = fake_log;
    api.fail = fake_fail;
    api.setting_bool = fake_bool;
    api.setting_int = fake_int;
    api.setting_float = fake_float;
    api.setting_string = fake_string;
    api.hook = fake_hook;
    api.function = fake_function;
    api.relocate_data = identity;
    api.relocate_level_data = identity_level;
    api.loaded_level = fake_loaded_level;

    // No hook taken yet.
    for (int i = 0; i < HOOK_COUNT; i++) {
        g_hooks[i].replacement = NULL;
    }

    g_hooks_taken = 0;
    g_fail[0] = 0;
    g_warned = 0;
    {
        const int r = g_load(&api, (openrac_mod*)&api);

        // A refusal says why, for the test's output.
        if (r != 0) {
            printf("load refused: %s\n", g_fail);
        }

        return r;
    }
}

static void defaults(void) {
    s_ground = s_air = s_flips = s_twist = s_wrench = 1;
    s_trace = 0;
    s_strafe = "both";
    s_camera = "rac2";
    s_clank = "enhanced";
    g_no_approach = 0;
    g_game = "rac1";
    g_version = D->version;
}

/* A hook point as the game calls it: the mod's replacement, or the game's own when no mod took it. */
#define AT(i) (g_hooks[i].replacement != NULL ? g_hooks[i].replacement : g_hooks[i].original)
#define CALL_UPDATE() ((void (*)(void))AT(UPDATE))()
#define CALL_TURN(m, a, d, l) ((void (*)(int32_t, float, float, float))AT(TURN))((m), (a), (d), (l))
#define CALL_APPROACH(u, d) ((void (*)(float, float))AT(APPROACH))((u), (d))
#define CALL_PLANAR(a) ((void (*)(float))AT(PLANAR))((a))
#define CALL_PAD() ((void (*)(uint32_t, uint32_t, int32_t))AT(PAD))(D->pad, 0, 18)
#define CALL_CAMERA() ((void (*)(uint32_t))AT(CAMERA))(CAMERA_MOBY)
#define CALL_ADVANCE() ((void (*)(void))AT(ADVANCE))()

/* ---- What rac1's state machine does inside hero_update, for each test ------------------- */

static float g_in_turn[3] =
    {0.019f, 0.1f, 9.948377f / 60.0f}; /* rac1's walk-band turn at full stick */
static float g_in_approach[2] = {7.5f / 3600.0f, 8.5f / 3600.0f};

static void run_tick(void) {
    CALL_TURN(0, g_in_turn[0], g_in_turn[1], g_in_turn[2]);
    putf(H(BM_HERO_TARGET_SPEED), 5.7f / 60.0f); /* rac1's run speed */
    CALL_APPROACH(g_in_approach[0], g_in_approach[1]);
    CALL_PLANAR(99999.0f);
}

static void idle_tick(void) {
    CALL_APPROACH(0.0f, 12.6f / 3600.0f);
}

/* ---- The tests ---------------------------------------------------------------------------- */

static void refusals(void) {
    defaults();
    g_game = "rac2";
    CHECK_EQ(load_mod() != 0, 1);
    CHECK_EQ(strstr(g_fail, "rac1") != NULL, 1);
    CHECK_EQ(g_hooks_taken, 0);

    defaults();
    g_version = "jp";
    CHECK_EQ(load_mod() != 0, 1);
    CHECK_EQ(strstr(g_fail, "PAL v2.00 and NTSC-U") != NULL, 1);
    CHECK_EQ(g_hooks_taken, 0);

    // Everything off, the camera and Clank the first game's: loads, takes nothing.
    defaults();
    s_ground = s_air = s_wrench = 0;
    s_strafe = "off";
    s_camera = "rac1";
    s_clank = "rac1";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 0);
    CHECK_EQ(strstr(g_last_log, "no hooks taken") != NULL, 1);

    // Clank alone, either choice: his update, the animation's undo, his run's turn, velocity and
    // speeds, and the pad's undo; not the camera.
    s_clank = "rac2";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 6);
    CHECK_EQ(g_hooks[CAMERA].replacement == NULL, 1);
    CHECK_EQ(strstr(g_last_log, "Clank Going Commando's") != NULL, 1);
    s_clank = "enhanced";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 6);
    CHECK_EQ(strstr(g_last_log, "Clank enhanced") != NULL, 1);

    // A value the manifest does not list: the default, enhanced.
    s_clank = "faster";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 6);

    // The wrench alone: the update, the animation, the turn, the speeds and the pad.
    defaults();
    s_ground = s_air = 0;
    s_strafe = "off";
    s_camera = "rac1";
    s_clank = "rac1";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 5);
    CHECK_EQ(g_hooks[PLANAR].replacement == NULL, 1);
    CHECK_EQ(g_hooks[CAMERA].replacement == NULL, 1);

    // The camera alone: the camera's overrides and the pad (which puts its gain back before each
    // tick), not the hero's update.
    defaults();
    s_ground = s_air = s_wrench = 0;
    s_strafe = "off";
    s_clank = "rac1";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 2);
    CHECK_EQ(g_hooks[CAMERA].replacement != NULL, 1);
    CHECK_EQ(g_hooks[PAD].replacement != NULL, 1);
    CHECK_EQ(g_hooks[UPDATE].replacement == NULL, 1);

    // A camera value the manifest no longer has (0.3.0's `modern`, kept by a host that does not hand
    // the default as the port does): Going Commando's, the default, with the same hooks, so not the
    // load that takes nothing.
    s_camera = "modern";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 2);
    CHECK_EQ(strstr(g_last_log, "camera Going Commando's") != NULL, 1);
}

static void hooks(void) {
    defaults();
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_fail[0], 0);
    CHECK_EQ(g_hooks_taken, HOOK_COUNT);

    // Each with a replacement.
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK_EQ(g_hooks[i].replacement != NULL, 1);
    }

    // A port without approach_speed: loads with a warning, the rest taken.
    defaults();
    g_no_approach = 1;
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, HOOK_COUNT - 1);
    CHECK_EQ(g_warned > 0, 1);
}

/* The run with Going Commando's running: rac2's turn, speed and velocity. */
static void run(void) {
    defaults();
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 2);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_FLAT_SPEED), 0.09f);
    g_inside = run_tick;
    g_turn_calls = g_approach_calls = g_planar_calls = 0;
    CALL_UPDATE();

    // The walk band at full stick: 0.022 / 0.27 / 0.093084246.
    CHECK_EQ(g_turn_calls, 1);
    CHECK_NEAR(g_turn_args[0], 0.022, TOL);
    CHECK_NEAR(g_turn_args[1], 0.27, TOL);
    CHECK_NEAR(g_turn_args[2], 0.093084246, TOL);

    // The speed: target 5.95 / 60 during the call, rac1's own after; up 0.005, down 0.0055555557.
    CHECK_EQ(g_approach_calls, 1);
    CHECK_NEAR(g_approach_target, 0.0991666667, TOL);
    CHECK_NEAR(g_approach_args[0], 0.005, TOL);
    CHECK_NEAR(g_approach_args[1], 0.0055555557, TOL);
    CHECK_NEAR(getf(H(BM_HERO_TARGET_SPEED)), 5.7 / 60.0, TOL);

    // Fast and facing the stick: along the facing.
    CHECK_NEAR(g_planar_angle, 99999.0, 1e-2);

    // Slow and off the stick: along the move yaw.
    putf(H(BM_HERO_FLAT_SPEED), 0.01f);
    putf(H(BM_HERO_YAW_ERROR), 1.0f);
    putf(H(BM_HERO_MOVE_YAW), 0.7f);
    CALL_UPDATE();
    CHECK_NEAR(g_planar_angle, 0.7, TOL);

    // Another mod scaled rac1's speed by 2 (down 17 / 3600): rac2's numbers by 2 too.
    putf(H(BM_HERO_YAW_ERROR), 0.0f);
    g_in_approach[0] = 15.0f / 3600.0f;
    g_in_approach[1] = 17.0f / 3600.0f;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 0.198333333, TOL);
    CHECK_NEAR(g_approach_args[0], 0.01, TOL);
    CHECK_NEAR(g_approach_args[1], 0.0111111114, TOL);
    g_in_approach[0] = 7.5f / 3600.0f;
    g_in_approach[1] = 8.5f / 3600.0f;

    // Standing: idle brakes 17.7 where rac1 brakes 12.6.
    putu(H(BM_HERO_STATE), 0);
    g_inside = idle_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_args[1], 17.7 / 3600.0, 1e-7);

    // Not in play (a scene): the calls as they came.
    putu(H(BM_HERO_STATE), 2);
    putu(D->cinematic_mode, 2);
    g_inside = run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.019, TOL);
    CHECK_NEAR(g_approach_args[1], 8.5 / 3600.0, 1e-7);
    putu(D->cinematic_mode, 0);
    g_inside = NULL;
}

/* A jump's first tick: rac2's air stick speed. */
static void air(void) {
    defaults();
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 7);
    putf(H(BM_HERO_AIR_STICK_SPEED), 5.7f / 60.0f);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_AIR_STICK_SPEED)), 0.0991666687, TOL);

    // With the setting off, it stays.
    s_air = 0;
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 7);
    putf(H(BM_HERO_AIR_STICK_SPEED), 5.7f / 60.0f);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_AIR_STICK_SPEED)), 0.095, TOL);
}

/* The run's exit: steered for the transitions, put back after them when they did not change state. */
static void run_exit(void) {
    defaults();
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 2);
    putf(H(BM_HERO_STICK), 0.05f);
    putf(H(BM_HERO_SPEED_XY), 0.08f);
    putu(H(BM_HERO_STATE_TIMER), 3);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_EQ(getf(H(BM_HERO_SPEED_XY)) < 2.7f / 60.0f, 1);
    CHECK_EQ((int)getu(H(BM_HERO_STATE_TIMER)) > 25, 1);

    // The transitions kept the run: both come back.
    CALL_ADVANCE();
    CHECK_NEAR(getf(H(BM_HERO_SPEED_XY)), 0.08, TOL);
    CHECK_EQ(getu(H(BM_HERO_STATE_TIMER)), 3);

    // They went to idle: the momentum and ground speed clamped to 1.5 units a second.
    CALL_UPDATE();
    putu(H(BM_HERO_STATE), 0);
    putu(H(BM_HERO_PREV_STATE), 2);
    putf(H(BM_HERO_MOMENTUM), 0.09f);
    putf(H(BM_HERO_GROUND_SPEED), 0.09f);
    CALL_ADVANCE();
    CHECK_NEAR(getf(H(BM_HERO_MOMENTUM)), 0.025, TOL);
    CHECK_NEAR(getf(H(BM_HERO_GROUND_SPEED)), 0.025, TOL);
}

/* The strafe: the pad, the facing, the velocity, the camera, the flip and its undo. */
static void strafe(void) {
    defaults();
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 2);
    putu(H(BM_HERO_CONTROL_MODE), 1);
    putf(D->camera_yaw, 1.0f);
    putf(H(BM_HERO_FACING), 0.9f);
    putf(H(BM_HERO_MOVE_YAW), 2.5f);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_STICK_X), -1.0f);

    // The pad: L2 and X held; the game sees only X, the mod keeps L2.
    putu(P(BM_PAD_HELD), BM_PAD_L2 | BM_PAD_CROSS);
    putu(P(BM_PAD_PRESSED), BM_PAD_L2);
    putf(P(BM_PAD_ANALOG + 4 * 14), 0.8f);
    CALL_PAD();
    CHECK_EQ(getu(P(BM_PAD_HELD)), BM_PAD_CROSS);
    CHECK_EQ(getu(P(BM_PAD_PRESSED)), 0);
    CHECK_NEAR(getf(P(BM_PAD_ANALOG + 4 * 14)), 0.0, TOL);

    // The run strafing: faces the camera's yaw on rac2's spring, then the stick's yaw is put back.
    g_inside = run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_move_yaw, 1.0, TOL);
    CHECK_NEAR(g_turn_args[0], 0.037, TOL);
    CHECK_NEAR(g_turn_args[1], 0.25, TOL);
    CHECK_NEAR(g_turn_args[2], 0.20362177, TOL);
    CHECK_NEAR(getf(H(BM_HERO_MOVE_YAW)), 2.5, TOL);
    CHECK_NEAR(getf(H(BM_HERO_YAW_ERROR)), 0.0, TOL);
    CHECK_NEAR(getf(H(BM_HERO_YAW_RATE)), 0.0, TOL);
    CHECK_NEAR(g_planar_angle, 2.5, TOL);

    // The twist: record 0's yaw target 2.5 - 0.9 = 1.6, record 1's -1.6, rac2's springs.
    CHECK_NEAR(getf(D->pose_table + BM_POSE_TARGET_YAW), 1.6, 1e-5);
    CHECK_NEAR(getf(D->pose_table + BM_POSE_RECORD + BM_POSE_TARGET_YAW), -1.6, 1e-5);
    CHECK_NEAR(getf(D->pose_table + BM_POSE_ACCEL), 0.037, TOL);

    // The camera: rac1's strafe requests, the timers cleared, the recentring's weight put back.
    putf(D->blend_weight, 0.3f);
    g_orbit_calls = g_slide_calls = g_spring_calls = 0;
    CALL_CAMERA();
    CHECK_EQ(g_orbit_calls, 1);
    CHECK_EQ(g_orbit_mode, 0);
    CHECK_EQ(g_slide_calls, 1);
    CHECK_NEAR(g_spring_args[0], 0.04, TOL);
    CHECK_NEAR(g_spring_args[1], 0.2, TOL);
    CHECK_EQ(gets16(CAMERA_VARS + 0x12), 0);
    CHECK_NEAR(getf(D->blend_weight), 0.3, TOL);

    // The flip from standing: X pressed, the stick fully left of his facing.
    putu(H(BM_HERO_STATE), 0);
    putf(H(BM_HERO_MOVE_YAW), 0.9f + 1.5707963f);
    putu(P(BM_PAD_HIST_BTN + 4 * 4), BM_PAD_CROSS); /* the newest poll: slot (5 - 1) */
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD(); /* the poll: L2 kept for the strafe */
    putu(P(BM_PAD_HIST_BTN + 4 * 4), BM_PAD_CROSS);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_EQ((getu(P(BM_PAD_HELD)) & BM_PAD_R1) != 0, 1);
    CHECK_EQ((getu(P(BM_PAD_HIST_BTN + 4 * 4)) & BM_PAD_FLICK) != 0, 1);

    // The next poll takes both back.
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    CHECK_EQ(getu(P(BM_PAD_HELD)) & BM_PAD_R1, 0);
    CHECK_EQ(getu(P(BM_PAD_HIST_BTN + 4 * 4)) & BM_PAD_FLICK, 0);

    // L2 only: R2 is the game's.
    s_strafe = "l2";
    load_mod();
    putu(P(BM_PAD_HELD), BM_PAD_R2);
    CALL_PAD();
    CHECK_EQ(getu(P(BM_PAD_HELD)), BM_PAD_R2);

    // Under a level's own camera: no strafe, so no camera requests.
    s_strafe = "both";
    load_mod();
    puts16(CAM_RECORD + BM_CAMERA_KIND, 5);
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    putu(H(BM_HERO_STATE), 2);
    g_inside = NULL;
    CALL_UPDATE();
    g_orbit_calls = 0;
    CALL_CAMERA();
    CHECK_EQ(g_orbit_calls, 0);
}

/* The backpedal: the run stepped back a key, the original run with no step of its own. */
static void backpedal(void) {
    defaults();
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 2);
    putu(H(BM_HERO_CONTROL_MODE), 1);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_STICK_Y), 1.0f); /* down: back */

    // A run of four keys, each rate 1, with a footstep at place 30 (key 1, half way).
    g_mem[MOBY + BM_MOBY_SEQ] = 4;
    g_mem[MOBY + BM_MOBY_PREV_SEQ] = 4;
    g_mem[MOBY + BM_MOBY_FRAME] = 2;
    g_mem[MOBY + BM_MOBY_NEXT] = 3;
    putf(MOBY + BM_MOBY_ANIM_TIME, 0.3f);
    putu(MOBY + BM_MOBY_CLASS, CLASS);
    putu(CLASS + 0x48 + 4 * 4, SEQ_RECORD);
    g_mem[SEQ_RECORD + 0x10] = 4;
    g_mem[SEQ_RECORD + 0x12] = 1;

    // Each key's frame data, its rate first.
    for (int i = 0; i < 4; i++) {
        putu(SEQ_RECORD + 0x1C + 4 * i, FRAMES + 0x100 * i);
        putf(FRAMES + 0x100 * i, 1.0f);
    }

    putu(SEQ_RECORD + 0x1C + 4 * 4, (30u << 16) | 77u);
    putf(H(BM_HERO_ANIM_SPEED), 0.5f);

    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    g_inside = NULL;
    CALL_UPDATE();
    g_sound_calls = 0;
    CALL_ADVANCE();

    // 0.3 - 0.5 is key 1's 0.8 (place 28): the footstep at 30 is gone over.
    CHECK_EQ(g_mem[MOBY + BM_MOBY_FRAME], 1);
    CHECK_EQ(g_mem[MOBY + BM_MOBY_NEXT], 2);
    CHECK_NEAR(getf(MOBY + BM_MOBY_ANIM_TIME), 0.8, TOL);
    CHECK_EQ(getu(MOBY + BM_MOBY_FRAME_DATA), FRAMES + 0x100);
    CHECK_EQ(g_sound_calls, 1);
    CHECK_EQ(g_sound_id, 77);
    CHECK_NEAR(g_advance_speed, 0.0, TOL);
    CHECK_NEAR(getf(H(BM_HERO_ANIM_SPEED)), 0.5, TOL);
}

/* ---- The wrench ---------------------------------------------------------------------------- */

/* The history slot of a press `age` polls back (the newest poll is 1; reset_game's index is 5). */
static uint32_t slot_of(int age) {
    return P(BM_PAD_HIST_BTN + 4 * (uint32_t)((5 - age + 30) % 30));
}

/* The wrench in hand, ready, on foot. */
static void wrench_in_hand(void) {
    putu(H(BM_HERO_ITEM_MOBY), MOBY);
    putu(H(BM_HERO_ITEM_STATE), 2);
    putu(H(BM_HERO_ITEM_ID), 8);
}

static void lock_on_tick(void) {
    CALL_TURN(0, 0.05f, 0.2f, 15.009831f / 60.0f);
}

static void lock_on_tick_pal(void) {
    CALL_TURN(0, 0.05f * 1.44f, 0.2f * 1.44f, 15.009831f / 50.0f);
}

static void slam_tick(void) {
    putf(H(BM_HERO_TARGET_SPEED), 0.0f); /* rac1's slam without a target */
    CALL_APPROACH(15.0f / 3600.0f, 15.0f / 3600.0f);
}

/* Going Commando's wrench: the lock-on turn, the swings' steers and their undo, Square's buffer, no
 * Square while the wrench is out, the slam's drive, the flip giving way to a swing, the setting off. */
static void wrench(void) {
    defaults();
    load_mod();
    reset_game();
    wrench_in_hand();

    // The lock-on turn in a swing: 15.01 radians a second becomes 0.2821616 a tick; the throw's
    // accel 0.057.
    putu(H(BM_HERO_STATE), 0x13);
    putu(H(BM_HERO_CONTROL_MODE), 6);
    g_inside = lock_on_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.05, TOL);
    CHECK_NEAR(g_turn_args[1], 0.2, TOL);
    CHECK_NEAR(g_turn_args[2], 0.2821616, TOL);
    putu(H(BM_HERO_STATE), 0x15);
    putu(H(BM_HERO_ATTACK_RECORD), 3);
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.057, TOL);
    g_inside = NULL;
    CALL_ADVANCE();

    // The first swing at frame 26.5 with a full stick: rac2 runs out (past its exit, 26), so rac1
    // sees frame 31.5 (past its exit, 31) for the transitions. There rac1's windows are 15 polls for
    // Square and 24 for X, rac2's at 26.5 only 14: a Square 15 back and an X 20 back are hidden.
    putu(H(BM_HERO_STATE), 0x13);
    putu(H(BM_HERO_ATTACK_RECORD), 0);
    putf(H(BM_HERO_ANIM_FRAME), 26.5f);
    putf(H(BM_HERO_STICK), 1.0f);
    putu(slot_of(15), BM_PAD_SQUARE);
    putu(slot_of(20), BM_PAD_CROSS);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 31.5, TOL);
    CHECK_EQ(getu(slot_of(15)), 0);
    CHECK_EQ(getu(slot_of(20)), 0);

    // The transitions went to idle: the frame is put back before the animation's advance all the
    // same, and the presses before the next poll.
    putu(H(BM_HERO_STATE), 0);
    CALL_ADVANCE();
    CHECK_NEAR(g_advance_frame, 26.5, TOL);
    CALL_PAD();
    CHECK_EQ(getu(slot_of(15)), BM_PAD_SQUARE);
    CHECK_EQ(getu(slot_of(20)), BM_PAD_CROSS);
    putu(slot_of(15), 0);
    putu(slot_of(20), 0);

    // The third swing at 22, stick centred: rac1 would stand (past 21), rac2 waits: held by the blend
    // flag for the transitions, which is put back after.
    putu(H(BM_HERO_STATE), 0x13);
    putu(H(BM_HERO_ATTACK_RECORD), 2);
    putf(H(BM_HERO_ANIM_FRAME), 22.0f);
    putf(H(BM_HERO_STICK), 0.0f);
    CALL_UPDATE();
    CHECK_EQ(getu(H(BM_HERO_ANIM_CHANGING)), 1);
    CALL_ADVANCE();
    CHECK_EQ(g_advance_changing, 0);

    // The throw at 36, X 3 polls back: rac2 jumps out, so rac1 sees frame 88; a Square 5 back is
    // hidden (at 88 it would throw again), X stays.
    putu(H(BM_HERO_STATE), 0x15);
    putu(H(BM_HERO_ATTACK_RECORD), 3);
    putf(H(BM_HERO_ANIM_FRAME), 36.0f);
    putu(slot_of(3), BM_PAD_CROSS);
    putu(slot_of(5), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 88.0, TOL);
    CHECK_EQ(getu(slot_of(3)), BM_PAD_CROSS);
    CHECK_EQ(getu(slot_of(5)), 0);
    CALL_ADVANCE();
    CALL_PAD();
    CHECK_EQ(getu(slot_of(5)), BM_PAD_SQUARE);
    putu(slot_of(3), 0);
    putu(slot_of(5), 0);

    // Standing, Square 9 polls back: rac1 counts 7, rac2 10, so it is shown in the newest poll.
    putu(H(BM_HERO_STATE), 0);
    putu(H(BM_HERO_CONTROL_MODE), 0);
    putu(slot_of(9), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), BM_PAD_SQUARE);
    CALL_ADVANCE();
    CALL_PAD();
    CHECK_EQ(getu(slot_of(1)), 0);
    CHECK_EQ(getu(slot_of(9)), BM_PAD_SQUARE);

    // 11 polls back is past rac2's 10 too.
    putu(slot_of(9), 0);
    putu(slot_of(11), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), 0);
    CALL_ADVANCE();
    putu(slot_of(11), 0);

    // The wrench out, in a jump: Square 3 polls back is hidden (rac1 would slam with no wrench).
    putu(H(BM_HERO_STATE), 7);
    putu(H(BM_HERO_CONTROL_MODE), 4);
    putu(H(BM_HERO_STATE_TIMER), 20);
    g_mem[H(BM_HERO_ITEM_THROWN)] = 1;
    putu(slot_of(3), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(3)), 0);
    CALL_ADVANCE();
    CALL_PAD();
    CHECK_EQ(getu(slot_of(3)), BM_PAD_SQUARE);
    putu(slot_of(3), 0);
    g_mem[H(BM_HERO_ITEM_THROWN)] = 0;

    // The slam 5 above the ground, no target, its 10th tick: the target is 4.5 a second (0.075) for
    // the call, rac1's 0 after.
    putu(H(BM_HERO_STATE), 0x14);
    putu(H(BM_HERO_STATE_TIMER), 10);
    putf(H(BM_HERO_GROUND_DISTANCE), 5.0f);
    g_inside = slam_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 0.075, TOL);
    CHECK_NEAR(getf(H(BM_HERO_TARGET_SPEED)), 0.0, TOL);

    // 3 above: rac1's own.
    putf(H(BM_HERO_GROUND_DISTANCE), 3.0f);
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 0.0, TOL);
    g_inside = NULL;
    CALL_ADVANCE();

    // The setting off: the turn as it came, and no steer.
    s_wrench = 0;
    load_mod();
    putu(H(BM_HERO_STATE), 0x13);
    putu(H(BM_HERO_CONTROL_MODE), 6);
    putu(H(BM_HERO_ATTACK_RECORD), 0);
    putf(H(BM_HERO_ANIM_FRAME), 27.0f);
    putf(H(BM_HERO_STICK), 1.0f);
    g_inside = lock_on_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[2], 15.009831 / 60.0, TOL);
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 27.0, TOL);
    g_inside = NULL;
}

/* A strafe flip gives way to a Square the wrench takes in the same tick (rac2 swings; R1 would make
 * rac1 throw). */
static void flip_and_square(void) {
    defaults();
    load_mod();
    reset_game();
    wrench_in_hand();
    putu(H(BM_HERO_STATE), 0);
    putu(H(BM_HERO_CONTROL_MODE), 0);
    putf(D->camera_yaw, 0.9f);
    putf(H(BM_HERO_FACING), 0.9f);
    putf(H(BM_HERO_MOVE_YAW), 0.9f + 1.5707963f);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_STICK_X), -1.0f);
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    putu(slot_of(1), BM_PAD_CROSS);
    putu(slot_of(3), BM_PAD_SQUARE);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_EQ(getu(P(BM_PAD_HELD)) & BM_PAD_R1, 0);
    CHECK_EQ(getu(slot_of(1)) & BM_PAD_FLICK, 0);
    CALL_ADVANCE();

    // Without the Square, the flip.
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    putu(slot_of(1), BM_PAD_CROSS);
    putu(slot_of(3), 0);
    CALL_UPDATE();
    CHECK_EQ((getu(P(BM_PAD_HELD)) & BM_PAD_R1) != 0, 1);
    CALL_ADVANCE();
    CALL_PAD();
}

/* Settings off: the calls as they came. */
static void settings_off(void) {
    defaults();
    s_ground = 0;
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 2);
    putf(H(BM_HERO_STICK), 1.0f);
    g_inside = run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.019, TOL);
    CHECK_NEAR(g_planar_angle, 99999.0, 1e-2);

    // The speeds as rac1 asked (the hook is there for the wrench's slam).
    CHECK_NEAR(g_approach_target, 5.7 / 60.0, TOL);
    CHECK_NEAR(g_approach_args[1], 8.5 / 3600.0, 1e-7);

    // With the wrench and Clank off too, the speeds' hook is not taken.
    s_wrench = 0;
    s_clank = "rac1";
    load_mod();
    CHECK_EQ(g_hooks[APPROACH].replacement == NULL, 1);
    g_inside = NULL;
}

/* ---- The camera ---------------------------------------------------------------------------- */

/* The follow camera's update as far as the mod sees it: the gain set from the game's table
 * (follow_camera_update), then the overrides, the mod's hook. */
static void camera_update(float table_gain) {
    putf(D->cam_gain, table_gain);
    CALL_CAMERA();
}

/* Standing strafing on L2, the follow camera in use. */
static void strafing(void) {
    putu(H(BM_HERO_STATE), 2);
    putu(H(BM_HERO_CONTROL_MODE), 1);
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    g_inside = NULL;
    CALL_UPDATE();
}

/* The camera's turn (the `camera` setting): each choice's gain, and only the gain, put back before the
 * next tick; a saved `modern` (0.3.0's, removed) as Going Commando's; nothing out of play, nothing
 * written into another level's program; the strafe camera beside it. */
static void camera_turn(void) {
    const float medium = 0.02268928f;
    const float fast = 0.027925268f;

    // Going Commando's at the game's middle step: its fastest, 2.2 degrees a tick for 1.3.
    defaults();
    load_mod();
    reset_game();
    g_camera_calls = 0;
    camera_update(medium);
    CHECK_EQ(g_camera_calls, 1);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == medium, 1);

    // At the fast step, the stick turning the camera: 2.2 degrees a tick for 1.6, and no other word
    // of the game's written (one update first, so the test's stand-in for the game's own camera code
    // has already written what it writes).
    putu(D->cam_option, 2);
    putf(P(BM_PAD_ANALOG), 1.0f);
    putf(P(BM_PAD_ANALOG + 4), 0.6f);
    camera_update(fast);
    CALL_PAD();
    memcpy(g_before, g_mem, MEMORY_SIZE);
    camera_update(fast);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CHECK_EQ(changed_besides(D->cam_gain), 0);

    // The next poll puts it back (the game would set it again from its table itself).
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == fast, 1);

    // A second update without the table's write or a poll between: put back first, so 2.2 degrees
    // again, not scaled twice.
    camera_update(fast);
    CALL_CAMERA();
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == fast, 1);

    // A saved `modern`, which the manifest no longer lists: Going Commando's, as the default, on
    // every step and with the stick on the camera (0.3.0 tripled the gain and stiffened the eye).
    s_camera = "modern";
    load_mod();
    reset_game();
    putf(P(BM_PAD_ANALOG), 1.0f);
    putf(P(BM_PAD_ANALOG + 4), 0.6f);
    camera_update(medium);
    CALL_PAD();
    memcpy(g_before, g_mem, MEMORY_SIZE);
    camera_update(medium);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CHECK_EQ(changed_besides(D->cam_gain), 0);
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == medium, 1);
    putu(D->cam_option, 0);
    camera_update(0.017453292f);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();
    putu(D->cam_option, 1);
    putf(P(BM_PAD_ANALOG + 4), 0.0f);

    // Another level loaded before the poll: its program is elsewhere, so nothing is written back.
    camera_update(medium);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    g_level = 3;
    CALL_PAD();
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    g_level = 0;

    // Out of play (a scene): nothing steered.
    s_camera = "rac2";
    load_mod();
    reset_game();
    putf(P(BM_PAD_ANALOG), 1.0f);
    putu(D->cinematic_mode, 1);
    camera_update(medium);
    CHECK_EQ(getf(D->cam_gain) == medium, 1);
    putu(D->cinematic_mode, 0);

    // Strafing: rac1's strafe requests and the turn's gain, both.
    reset_game();
    strafing();
    g_orbit_calls = 0;
    camera_update(medium);
    CHECK_EQ(g_orbit_calls, 1);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_ADVANCE();
    CALL_PAD();

    // The first game's camera: the strafe's requests still, the gain the game's.
    s_camera = "rac1";
    load_mod();
    reset_game();
    strafing();
    g_orbit_calls = 0;
    camera_update(medium);
    CHECK_EQ(g_orbit_calls, 1);
    CHECK_EQ(getf(D->cam_gain) == medium, 1);
    CALL_ADVANCE();
    CALL_PAD();
}

/* ---- Clank -------------------------------------------------------------------------------------- */

/* What Clank's run does inside hero_update: rac1's fast turn at full stick (t = 1.35), its speed step
 * with the stick's 3 u/s as the target, and its velocity along his facing. */
static void clank_run_tick(void) {
    CALL_TURN(0, 0.008f * 1.35f, 0.15f, 9.948377f * 1.35f / 60.0f);
    putf(H(BM_HERO_TARGET_SPEED), 3.0f / 60.0f);
    CALL_APPROACH(7.5f * 0.9f / 3600.0f, 8.5f / 3600.0f);
    CALL_PLANAR(99999.0f);
}

/* The same at 50 Hz: rac1's spring x 1.44, its limit and speeds by 1/50. */
static void clank_run_tick_pal(void) {
    CALL_TURN(0, 0.008f * 1.35f * 1.44f, 0.15f * 1.44f, 9.948377f * 1.35f / 50.0f);
    putf(H(BM_HERO_TARGET_SPEED), 3.0f / 50.0f);
    CALL_APPROACH(7.5f / 2500.0f, 8.5f / 2500.0f);
    CALL_PLANAR(99999.0f);
}

/* His punch's lock-on turn. */
static void clank_punch_tick(void) {
    CALL_TURN(0, 0.05f, 0.2f, 15.009831f / 60.0f);
}

/* Clank alone in play (vehicle mode 1), in `state`. */
static void clank_in(int state) {
    g_mem[H(BM_HERO_VEHICLE_MODE)] = 1;
    putu(H(BM_HERO_STATE), (uint32_t)state);
}

/* Clank under each choice: his run's turn, speed and velocity; his jump's entry; Square remembered;
 * the punch's way out and lock-on turn; the steers put back; nothing for the first game's choice, for
 * Giant Clank or in a scene; and Ratchet's run as before beside him. */
static void clank(void) {
    // Enhanced, his run at full stick, running: rac2's spring, 18 / 20 and the target x 3.45 / 3 for
    // the call (rac1's put back after), along his facing.
    defaults();
    load_mod();
    reset_game();
    clank_in(0x44);
    putu(H(BM_HERO_STATE_TIMER), 10);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_SPEED_XY), 0.05f);
    putf(H(BM_HERO_FLAT_SPEED), 0.05f);
    putf(H(BM_HERO_MOVE_YAW), 0.4f);
    g_inside = clank_run_tick;
    g_turn_calls = g_approach_calls = g_planar_calls = 0;
    CALL_UPDATE();
    CHECK_EQ(g_turn_calls, 1);
    CHECK_NEAR(g_turn_args[0], 0.033, TOL);
    CHECK_NEAR(g_turn_args[1], 0.15, TOL);
    CHECK_NEAR(g_turn_args[2], 0.16580631, TOL);
    CHECK_NEAR(g_approach_target, 3.45 / 60.0, TOL);
    CHECK_NEAR(g_approach_args[0], 18.0 / 3600.0, 1e-7);
    CHECK_NEAR(g_approach_args[1], 20.0 / 3600.0, 1e-7);
    CHECK_NEAR(getf(H(BM_HERO_TARGET_SPEED)), 3.0 / 60.0, TOL);
    CHECK_NEAR(g_planar_angle, 99999.0, 1e-2);
    CALL_ADVANCE();

    // Slow (0.3 u/s), the turn leaving him 0.25 rad off: along the stick, at the move yaw.
    putf(H(BM_HERO_FLAT_SPEED), 0.005f);
    CALL_UPDATE();
    CHECK_NEAR(g_planar_angle, 0.4, TOL);
    CALL_ADVANCE();

    // Slow, the turn leaving him 2 rad off at full stick: a turnaround, whose spring the next tick's
    // turn takes (0.037 / 0.15 / 11.69).
    g_turn_error = 2.0f;
    CALL_UPDATE();
    CALL_ADVANCE();
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.037, TOL);
    CHECK_NEAR(g_turn_args[2], 0.19489513, TOL);
    CALL_ADVANCE();

    // A new run starts with the flags clear (rac2's run entry): its first turn is the run's.
    clank_in(0x43);
    g_inside = NULL;
    CALL_UPDATE();
    CALL_ADVANCE();
    clank_in(0x44);
    g_turn_error = 0.25f;
    g_inside = clank_run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.033, TOL);
    g_inside = NULL;
    CALL_ADVANCE();

    // His jump's first tick: rac1's takeoff 5, frames 18 / 27, air speed 2 become 3, 15 / 24, 2.5.
    clank_in(0x49);
    putu(H(BM_HERO_STATE_TIMER), 0);
    putu(H(BM_HERO_TAKEOFF_FRAME), 5);
    putf(H(BM_HERO_APEX_FRAME), 18.0f);
    putf(H(BM_HERO_TARGET_FRAME), 27.0f);
    putf(H(BM_HERO_AIR_STICK_SPEED), 2.0f / 60.0f);
    CALL_UPDATE();
    CHECK_EQ(getu(H(BM_HERO_TAKEOFF_FRAME)), 3);
    CHECK_NEAR(getf(H(BM_HERO_APEX_FRAME)), 15.0, TOL);
    CHECK_NEAR(getf(H(BM_HERO_TARGET_FRAME)), 24.0, TOL);
    CHECK_NEAR(getf(H(BM_HERO_AIR_STICK_SPEED)), 2.5 / 60.0, TOL);
    CALL_ADVANCE();

    // Standing, Square 11 polls back: rac1 remembers 9, enhanced 13: shown in the newest poll for the
    // transitions, taken back at the next poll.
    clank_in(0x43);
    putu(slot_of(11), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), BM_PAD_SQUARE);
    CALL_ADVANCE();
    CALL_PAD();
    CHECK_EQ(getu(slot_of(1)), 0);
    CHECK_EQ(getu(slot_of(11)), BM_PAD_SQUARE);
    putu(slot_of(11), 0);

    // The punch at frame 8.5 with the stick pushed: rac1 sees 10.5 and stands (then runs), X 20 polls
    // back hidden for the tick; the frame and X put back.
    clank_in(0x51);
    putu(H(BM_HERO_ATTACK_RECORD), 7);
    putf(H(BM_HERO_ANIM_FRAME), 8.5f);
    putu(slot_of(20), BM_PAD_CROSS);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 10.5, TOL);
    CHECK_EQ(getu(slot_of(20)), 0);
    clank_in(0x43);
    CALL_ADVANCE();
    CHECK_NEAR(g_advance_frame, 8.5, TOL);
    CALL_PAD();
    CHECK_EQ(getu(slot_of(20)), BM_PAD_CROSS);
    putu(slot_of(20), 0);

    // Its lock-on turn: 16.93 radians a second.
    clank_in(0x51);
    putf(H(BM_HERO_ANIM_FRAME), 4.0f);
    g_inside = clank_punch_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[2], 0.2821616, TOL);
    CALL_ADVANCE();

    // Going Commando's: no Square past 9, no run out at 8.5, but standing past 9.
    s_clank = "rac2";
    load_mod();
    g_inside = NULL;
    clank_in(0x43);
    putu(slot_of(11), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), 0);
    CALL_ADVANCE();
    CALL_PAD();
    putu(slot_of(11), 0);
    clank_in(0x51);
    putf(H(BM_HERO_ANIM_FRAME), 8.5f);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 8.5, TOL);
    CALL_ADVANCE();
    putf(H(BM_HERO_ANIM_FRAME), 9.5f);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 10.5, TOL);
    CALL_ADVANCE();
    CALL_PAD();

    // X 9 polls back at frame 7.5: rac2's punch, a frame ahead, jumps out; rac1 is shown 8.5, whose
    // window takes the press (X kept).
    putf(H(BM_HERO_ANIM_FRAME), 7.5f);
    putu(slot_of(9), BM_PAD_CROSS);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 8.5, TOL);
    CHECK_EQ(getu(slot_of(9)), BM_PAD_CROSS);
    CALL_ADVANCE();
    CALL_PAD();
    putu(slot_of(9), 0);

    // Its run: the target as rac1's (3 u/s).
    clank_in(0x44);
    g_inside = clank_run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 3.0 / 60.0, TOL);
    CHECK_NEAR(g_turn_args[0], 0.033, TOL);
    CALL_ADVANCE();

    // The first game's Clank: every call as it came, nothing steered.
    s_clank = "rac1";
    load_mod();
    memcpy(g_before, g_mem, MEMORY_SIZE);
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.008 * 1.35, TOL);
    CHECK_NEAR(g_approach_args[0], 7.5 * 0.9 / 3600.0, 1e-7);
    CHECK_NEAR(g_approach_target, 3.0 / 60.0, TOL);
    CHECK_EQ(changed_besides(H(BM_HERO_TARGET_SPEED)), 0);
    CALL_ADVANCE();

    // Giant Clank (vehicle mode 2), and Clank in a scene: as they came.
    s_clank = "enhanced";
    load_mod();
    clank_in(0x44);
    g_mem[H(BM_HERO_VEHICLE_MODE)] = 2;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.008 * 1.35, TOL);
    CHECK_NEAR(g_approach_args[1], 8.5 / 3600.0, 1e-7);
    CALL_ADVANCE();
    g_mem[H(BM_HERO_VEHICLE_MODE)] = 1;
    putu(D->cinematic_mode, 1);
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.008 * 1.35, TOL);
    putu(D->cinematic_mode, 0);
    CALL_ADVANCE();

    // Ratchet again (vehicle mode 0) in his run: Going Commando's run, as before.
    g_mem[H(BM_HERO_VEHICLE_MODE)] = 0;
    putu(H(BM_HERO_STATE), 2);
    g_inside = run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.022, TOL);
    CHECK_NEAR(g_approach_target, 0.0991666667, TOL);
    g_inside = NULL;
    CALL_ADVANCE();
}

/* PAL's 50 Hz tick on PAL's row: rac2's numbers scaled as rac1 scales its own, rac1's counts by 1.2. */
static void pal_tick(void) {
    D = &k_bm_discs[0];
    CHECK_EQ(strcmp(D->version, "pal"), 0);
    defaults();
    load_mod();
    reset_game();
    putf(D->frame_time, 1.0f / 50.0f);
    putf(D->frame_time_sq, 1.0f / 2500.0f);
    putf(D->frame_scale_sq, 1.44f);
    putf(D->frame_scale, 1.2f);
    putu(H(BM_HERO_STATE), 2);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_FLAT_SPEED), 0.09f);
    g_inside = run_tick;
    CALL_UPDATE();

    // The walk band at full stick: 0.022 x 1.44 = 0.03168, 0.27 x 1.44 = 0.3888, 0.093084246 x 1.2.
    CHECK_NEAR(g_turn_args[0], 0.03168, TOL);
    CHECK_NEAR(g_turn_args[1], 0.3888, TOL);
    CHECK_NEAR(g_turn_args[2], 0.111701095, TOL);

    // The exit: the state timer past rac1's 25 ticks as PAL counts them, 30.
    putf(H(BM_HERO_STICK), 0.05f);
    putu(H(BM_HERO_STATE_TIMER), 3);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_EQ(getu(H(BM_HERO_STATE_TIMER)), 31);
    CALL_ADVANCE();

    // The wrench standing: Square 9 polls back, which 60 Hz buffers (rac1 7, rac2 10). At 50 Hz rac1
    // counts 8 and rac2's PAL release 8 too: not shown.
    wrench_in_hand();
    putu(H(BM_HERO_STATE), 0);
    putu(H(BM_HERO_CONTROL_MODE), 0);
    putf(H(BM_HERO_STICK), 0.0f);
    putu(slot_of(9), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), 0);
    CALL_ADVANCE();
    CALL_PAD();
    putu(slot_of(9), 0);

    // The lock-on turn: 0.2821616 x 1.2 radians a tick.
    putu(H(BM_HERO_STATE), 0x13);
    putu(H(BM_HERO_CONTROL_MODE), 6);
    g_inside = lock_on_tick_pal;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[2], 0.33859392, TOL);
    CHECK_NEAR(g_turn_args[0], 0.072, TOL);
    g_inside = NULL;
    CALL_ADVANCE();

    // Going Commando's at 50 Hz: its fastest step per tick, as rac1 counts the gain per tick; a saved
    // `modern` the same.
    s_camera = "rac2";
    load_mod();
    putf(P(BM_PAD_ANALOG), -1.0f);
    camera_update(0.02268928f);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();
    s_camera = "modern";
    load_mod();
    camera_update(0.02268928f);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();

    // Clank at 50 Hz: rac1's takeoff scale_ticks(5) = 6 becomes rac2's PAL 2, the air speed 2.5 /
    // 50; his run 18 / 2500 and 20 / 2500 against rac1's 7.5 and 8.5 / 2500.
    s_clank = "enhanced";
    load_mod();
    clank_in(0x49);
    putu(H(BM_HERO_STATE_TIMER), 0);
    putu(H(BM_HERO_TAKEOFF_FRAME), 6);
    putf(H(BM_HERO_APEX_FRAME), 18.0f);
    putf(H(BM_HERO_TARGET_FRAME), 27.0f);
    putf(H(BM_HERO_AIR_STICK_SPEED), 2.0f / 50.0f);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_EQ(getu(H(BM_HERO_TAKEOFF_FRAME)), 2);
    CHECK_NEAR(getf(H(BM_HERO_AIR_STICK_SPEED)), 2.5 / 50.0, TOL);
    CALL_ADVANCE();
    clank_in(0x44);
    putu(H(BM_HERO_STATE_TIMER), 10);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_SPEED_XY), 0.06f);
    g_inside = clank_run_tick_pal;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.033 * 1.44, TOL);
    CHECK_NEAR(g_turn_args[2], 0.16580631 * 1.2, 1e-6);
    CHECK_NEAR(g_approach_args[0], 18.0 / 2500.0, 1e-7);
    CHECK_NEAR(g_approach_args[1], 20.0 / 2500.0, 1e-7);
    CHECK_NEAR(g_approach_target, 3.45 / 50.0, TOL);
    g_inside = NULL;
    CALL_ADVANCE();

    // Square at 50 Hz: rac1 counts 9 as 11, enhanced 13 as 16: 14 polls back is shown.
    clank_in(0x43);
    putu(slot_of(14), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), BM_PAD_SQUARE);
    CALL_ADVANCE();
    CALL_PAD();
    putu(slot_of(14), 0);
}

int main(int argc, char** argv) {
    openrac_mod_info info = {0};
    void* lib;
    query_fn query;

    // Unbuffered, so a crash still shows the checks before it.
    setvbuf(stdout, NULL, _IONBF, 0);

    // The library to load.
    if (argc < 2) {
        printf("usage: %s <mod library>\n", argv[0]);
        return 2;
    }

    lib = LIB_OPEN(argv[1]);

    // It loads, or nothing else can be tested.
    if (lib == NULL) {
        printf("cannot load %s\n", argv[1]);
        return 2;
    }

    query = (query_fn)LIB_SYM(lib, "openrac_mod_query");
    g_load = (load_fn)LIB_SYM(lib, "openrac_mod_load");
    CHECK_EQ(query != NULL && g_load != NULL, 1);

    // Without its entry points there is nothing more to test.
    if (query == NULL || g_load == NULL) {
        CHECK_DONE("load");
    }

    // The query: the manifest's id, version and api (1.0).
    info.size = sizeof info;
    query(&info);
    CHECK_EQ(strcmp(info.id, RAC1_ENHANCED_MOVEMENT_ID), 0);
    CHECK_EQ(strcmp(info.version, RAC1_ENHANCED_MOVEMENT_VERSION), 0);
    CHECK_EQ(info.api_major, 1);
    CHECK_EQ(info.api_minor, 0);

    g_mem = calloc(1, MEMORY_SIZE);
    g_before = calloc(1, MEMORY_SIZE);

    // Every disc's row, each with the game laid out at its own addresses.
    for (size_t i = 0; i < sizeof k_bm_discs / sizeof k_bm_discs[0]; i++) {
        D = &k_bm_discs[i];
        printf("disc %s\n", D->version);
        refusals();
        hooks();
        run();
        air();
        run_exit();
        strafe();
        backpedal();
        wrench();
        flip_and_square();
        settings_off();
        camera_turn();
        clank();
    }

    pal_tick();

    // The last line the mod logged at load, for the record.
    printf("last log: %s\n", g_last_log);
    free(g_mem);
    free(g_before);
    CHECK_DONE("load");
}
