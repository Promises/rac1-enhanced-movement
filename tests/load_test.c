/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests the built library as the port loads it (GUIDE.md section 12), against a host table made
 * here: its query, its refusals, the hooks it takes and their signatures, and what each hook does
 * to game memory laid out at the addresses of each row of src/rac1/discs.h. Stubs that count their
 * calls stand in for the game's code. The expected values are worked out from rac1's and rac2's
 * numbers (src/rac1/rac1.h, rac2.h) in the comment above each check.
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

/* ---- Game memory and host state ---- */

#define MEMORY_SIZE 0x00400000u
static uint8_t* g_mem;
static uint8_t* g_before; /* a copy, to find what a hook wrote */

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

/* The disc row under test. */
static const bm_disc* D;

/* Where the test lays out the rest of the game. */
#define CAM_RECORD 0x00300000u
#define MOBY 0x00310000u
#define CLASS 0x00320000u
#define SEQ_RECORD 0x00330000u
#define FRAMES 0x00340000u
#define CAMERA_MOBY 0x00350000u
#define CAMERA_VARS 0x00360000u

/* Reads and writes of game memory, by guest address. */
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

/* A field of the hero, and of the first pad, on the disc under test. */
#define H(off) (D->hero + (off))
#define P(off) (D->pad + (off))

/* Lays out a hero in play at NTSC's tick: a level, his moby, no scene, on foot, under the follow
 * camera. */
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

    // The camera speed option and gain as the game sets them for its middle step.
    putu(D->cam_option, 1);
    putf(D->cam_gain, 0.02268928f);
}

/* Returns how many bytes of game memory differ from g_before, not counting the word at `skip`. */
static int changed_besides(uint32_t skip) {
    int n = 0;

    // The whole memory is compared byte by byte.
    for (uint32_t i = 0; i < MEMORY_SIZE; i++) {
        // A changed byte outside the skipped word counts.
        if ((i < skip || i >= skip + 4) && g_mem[i] != g_before[i]) {
            n++;
        }
    }

    return n;
}

/* ---- Hook points ---- */

/* A hook point the test's host offers: its id and alias, its signature, the mod's replacement and
 * the stub behind it. */
typedef struct taken {
    const char* id;
    const char* alias;
    const char* signature;
    void* replacement;
    void* original;
} taken;

/* What the stubs saw: their calls, counted, and the last arguments and values each read. */
static int g_turn_calls, g_approach_calls, g_planar_calls, g_update_calls, g_advance_calls,
    g_pad_calls, g_camera_calls;
static float g_turn_args[3], g_turn_move_yaw, g_approach_args[2], g_approach_target, g_planar_angle,
    g_advance_speed, g_advance_frame;
static int32_t g_advance_changing;
static float g_turn_error = 0.25f; /* the yaw error the game's turn leaves */
static void (*g_inside)(void);     /* what the state machine does inside hero_update */

/* The game's code behind each hook point: stubs that count their calls and record what they see. */
static void orig_pad(uint32_t p, uint32_t r, int32_t n) {
    (void)p;
    (void)r;
    (void)n;
    g_pad_calls++;
}

static void orig_update(void) {
    g_update_calls++;

    // The test's stand-in for the state machine makes its calls here.
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

/* The hook points the host offers, in the order of the indexes below. */
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

/* Indexes into g_hooks. */
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

/* The host's hook: takes a hook point by id or alias and checks the signature, as the port does. */
static int fake_hook(
    openrac_mod* mod, const char* name, const char* signature, void* replacement, void** original
) {
    (void)mod;

    // The port's loader takes a hook point by its id or its alias.
    for (int i = 0; i < HOOK_COUNT; i++) {
        // The name matches this hook point.
        if (strcmp(name, g_hooks[i].id) == 0 || strcmp(name, g_hooks[i].alias) == 0) {
            // A port older than approach_speed, when a test asks for one.
            if (i == APPROACH && g_no_approach) {
                return OPENRAC_HOOK_UNKNOWN;
            }

            // A signature other than the SDK's is refused, as the port refuses it.
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

/* ---- Game functions ---- */

/* Stubs for the game functions the mod calls, which count their calls and record the arguments. */
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

/* Returns the stub for a game function by this disc's name, or NULL. */
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

    // The four stubs are looked up by name.
    for (int i = 0; i < 4; i++) {
        // The name matches as this disc's build of the port gives it.
        if (strcmp(name, fns[i].name) == 0) {
            return fns[i].fn;
        }
    }

    return NULL;
}

/* ---- Host table ---- */

/* The host's log: keeps the last line and counts warnings and errors. */
static void fake_log(openrac_mod* mod, int level, const char* text) {
    (void)mod;
    snprintf(g_last_log, sizeof g_last_log, "%s", text);

    // Warnings and errors are counted.
    if (level >= OPENRAC_LOG_WARN) {
        g_warned++;
    }
}

/* The host's refusal: keeps the reason for the test's output. */
static void fake_fail(openrac_mod* mod, const char* reason) {
    (void)mod;
    snprintf(g_fail, sizeof g_fail, "%s", reason);
}

/* The host's bool settings, from the test's settings above. */
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

    // A key the test does not set reads as 0.
    for (int i = 0; i < 6; i++) {
        // The key matches this setting.
        if (strcmp(key, settings[i].key) == 0) {
            return *settings[i].value;
        }
    }

    return 0;
}

/* The manifest has no int or float setting, so these read 0. */
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

/* The host's string settings: camera, clank and strafe. */
static const char* fake_string(openrac_mod* mod, const char* key) {
    (void)mod;

    // The camera setting.
    if (strcmp(key, "camera") == 0) {
        return s_camera;
    }

    // The clank setting.
    if (strcmp(key, "clank") == 0) {
        return s_clank;
    }

    // The strafe setting is the only other string.
    return strcmp(key, "strafe") == 0 ? s_strafe : "";
}

/* The test's memory is not relocated: each address is where discs.h puts it. */
static openrac_gaddr identity(openrac_gaddr a) {
    return a;
}

static openrac_gaddr identity_level(int level, openrac_gaddr a) {
    (void)level;
    return a;
}

/* The level the test has loaded (g_level). */
static int fake_loaded_level(void) {
    return g_level;
}

/* The library's entry points. */
typedef void (*query_fn)(openrac_mod_info*);
typedef int (*load_fn)(const openrac_api*, openrac_mod*);
static load_fn g_load;

/* Loads the mod with the current settings. The hooks it took are in g_hooks. */
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

    // The last load's hooks are cleared first.
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

/* Sets every setting to the manifest's default, on rac1 and the disc under test, with
 * approach_speed in the port. */
static void defaults(void) {
    s_ground = s_air = s_flips = s_twist = s_wrench = 1;
    s_trace = 0;
    s_strafe = "both";
    s_camera = "enhanced";
    s_clank = "enhanced";
    g_no_approach = 0;
    g_game = "rac1";
    g_version = D->version;
}

/* Calls a hook point as the game does: the mod's replacement, or the stub if it took none. */
#define AT(i) (g_hooks[i].replacement != NULL ? g_hooks[i].replacement : g_hooks[i].original)
#define CALL_UPDATE() ((void (*)(void))AT(UPDATE))()
#define CALL_TURN(m, a, d, l) ((void (*)(int32_t, float, float, float))AT(TURN))((m), (a), (d), (l))
#define CALL_APPROACH(u, d) ((void (*)(float, float))AT(APPROACH))((u), (d))
#define CALL_PLANAR(a) ((void (*)(float))AT(PLANAR))((a))
#define CALL_PAD() ((void (*)(uint32_t, uint32_t, int32_t))AT(PAD))(D->pad, 0, 18)
#define CALL_CAMERA() ((void (*)(uint32_t))AT(CAMERA))(CAMERA_MOBY)
#define CALL_ADVANCE() ((void (*)(void))AT(ADVANCE))()

/* ---- Inside hero_update ---- */

static float g_in_turn[3] =
    {0.019f, 0.1f, 9.948377f / 60.0f}; /* rac1's walk-band turn at full stick */
static float g_in_approach[2] = {7.5f / 3600.0f, 8.5f / 3600.0f}; /* rac1's run speed step */

/* A run tick of the state machine: rac1's turn, speed step and velocity along the facing. */
static void run_tick(void) {
    CALL_TURN(0, g_in_turn[0], g_in_turn[1], g_in_turn[2]);
    putf(H(BM_HERO_TARGET_SPEED), 5.7f / 60.0f); /* rac1's run speed */
    CALL_APPROACH(g_in_approach[0], g_in_approach[1]);
    CALL_PLANAR(99999.0f);
}

/* An idle tick: rac1's idle braking. */
static void idle_tick(void) {
    CALL_APPROACH(0.0f, 12.6f / 3600.0f);
}

/* ---- Tests ---- */

/* The refusals of another game and an unknown disc, and the hooks each mix of settings takes. */
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

    // With everything off, and the camera and Clank as the first game has them, the mod loads and
    // takes nothing.
    defaults();
    s_ground = s_air = s_wrench = 0;
    s_strafe = "off";
    s_camera = "rac1";
    s_clank = "rac1";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 0);
    CHECK_EQ(strstr(g_last_log, "changes nothing") != NULL, 1);

    // Clank alone, either choice, takes his update, the animation's undo, his run's turn, velocity
    // and speeds, and the pad's undo, but not the camera.
    s_clank = "rac2";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 6);
    CHECK_EQ(g_hooks[CAMERA].replacement == NULL, 1);
    CHECK_EQ(strstr(g_last_log, "Clank: Going Commando") != NULL, 1);
    s_clank = "enhanced";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 6);
    CHECK_EQ(strstr(g_last_log, "Clank: Enhanced") != NULL, 1);

    // A value the manifest does not list gives the default, enhanced.
    s_clank = "faster";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 6);

    // The wrench alone takes the update, the animation, the turn, the speeds and the pad.
    defaults();
    s_ground = s_air = 0;
    s_strafe = "off";
    s_camera = "rac1";
    s_clank = "rac1";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 5);
    CHECK_EQ(g_hooks[PLANAR].replacement == NULL, 1);
    CHECK_EQ(g_hooks[CAMERA].replacement == NULL, 1);

    // The camera alone takes the camera's overrides and the pad, which puts the gain back before
    // each tick, but not the hero's update.
    defaults();
    s_ground = s_air = s_wrench = 0;
    s_strafe = "off";
    s_clank = "rac1";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 2);
    CHECK_EQ(g_hooks[CAMERA].replacement != NULL, 1);
    CHECK_EQ(g_hooks[PAD].replacement != NULL, 1);
    CHECK_EQ(g_hooks[UPDATE].replacement == NULL, 1);

    // A camera value the manifest no longer has (0.3.0's `modern`, from a host that does not swap
    // in the default as the port does) gives the default, Enhanced, with the same hooks
    // rather than none.
    s_camera = "modern";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 2);
    CHECK_EQ(strstr(g_last_log, "Camera: Enhanced") != NULL, 1);
}

/* Every hook taken with the defaults, and all but one, with a warning, on a port without
 * approach_speed. */
static void hooks(void) {
    defaults();
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_fail[0], 0);
    CHECK_EQ(g_hooks_taken, HOOK_COUNT);

    // Every hook point has the mod's replacement.
    for (int i = 0; i < HOOK_COUNT; i++) {
        CHECK_EQ(g_hooks[i].replacement != NULL, 1);
    }

    // A port without approach_speed loads the mod with a warning and the other hooks taken.
    defaults();
    g_no_approach = 1;
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, HOOK_COUNT - 1);
    CHECK_EQ(g_warned > 0, 1);
}

/* The run under the `ground` setting: rac2's turn, speed and velocity. */
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

    // The walk band at full stick turns at 0.022 / 0.27 / 0.093084246.
    CHECK_EQ(g_turn_calls, 1);
    CHECK_NEAR(g_turn_args[0], 0.022, TOL);
    CHECK_NEAR(g_turn_args[1], 0.27, TOL);
    CHECK_NEAR(g_turn_args[2], 0.093084246, TOL);

    // The target speed is 5.95 / 60 during the call and rac1's after; up 0.005, down 0.0055555557.
    CHECK_EQ(g_approach_calls, 1);
    CHECK_NEAR(g_approach_target, 0.0991666667, TOL);
    CHECK_NEAR(g_approach_args[0], 0.005, TOL);
    CHECK_NEAR(g_approach_args[1], 0.0055555557, TOL);
    CHECK_NEAR(getf(H(BM_HERO_TARGET_SPEED)), 5.7 / 60.0, TOL);

    // Fast and facing the stick, he moves along his facing.
    CHECK_NEAR(g_planar_angle, 99999.0, 1e-2);

    // Slow and turned away from the stick, he moves along the move yaw.
    putf(H(BM_HERO_FLAT_SPEED), 0.01f);
    putf(H(BM_HERO_YAW_ERROR), 1.0f);
    putf(H(BM_HERO_MOVE_YAW), 0.7f);
    CALL_UPDATE();
    CHECK_NEAR(g_planar_angle, 0.7, TOL);

    // When another mod has doubled rac1's speeds (down 17 / 3600), rac2's are doubled too.
    putf(H(BM_HERO_YAW_ERROR), 0.0f);
    g_in_approach[0] = 15.0f / 3600.0f;
    g_in_approach[1] = 17.0f / 3600.0f;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 0.198333333, TOL);
    CHECK_NEAR(g_approach_args[0], 0.01, TOL);
    CHECK_NEAR(g_approach_args[1], 0.0111111114, TOL);
    g_in_approach[0] = 7.5f / 3600.0f;
    g_in_approach[1] = 8.5f / 3600.0f;

    // Standing, idle brakes at 17.7 where rac1 brakes at 12.6.
    putu(H(BM_HERO_STATE), 0);
    g_inside = idle_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_args[1], 17.7 / 3600.0, 1e-7);

    // Out of play (a scene), the calls go through as they came.
    putu(H(BM_HERO_STATE), 2);
    putu(D->cinematic_mode, 2);
    g_inside = run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.019, TOL);
    CHECK_NEAR(g_approach_args[1], 8.5 / 3600.0, 1e-7);
    putu(D->cinematic_mode, 0);
    g_inside = NULL;
}

/* rac2's air stick speed, set on a jump's first tick. */
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

/* The run's exit: steered for the transitions, and put back after them if the state stays. */
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

    // The transitions kept the run, so both values come back.
    CALL_ADVANCE();
    CHECK_NEAR(getf(H(BM_HERO_SPEED_XY)), 0.08, TOL);
    CHECK_EQ(getu(H(BM_HERO_STATE_TIMER)), 3);

    // When they went to idle, the momentum and ground speed are clamped to 1.5 units a second.
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

    // With L2 and X held, the game sees only X and the mod keeps L2.
    putu(P(BM_PAD_HELD), BM_PAD_L2 | BM_PAD_CROSS);
    putu(P(BM_PAD_PRESSED), BM_PAD_L2);
    putf(P(BM_PAD_ANALOG + 4 * 14), 0.8f);
    CALL_PAD();
    CHECK_EQ(getu(P(BM_PAD_HELD)), BM_PAD_CROSS);
    CHECK_EQ(getu(P(BM_PAD_PRESSED)), 0);
    CHECK_NEAR(getf(P(BM_PAD_ANALOG + 4 * 14)), 0.0, TOL);

    // Running while strafing, he turns to the camera's yaw on rac2's spring, then the stick's yaw
    // is put back.
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

    // The twist sets record 0's yaw target to 2.5 - 0.9 = 1.6 and record 1's to -1.6, on rac2's
    // springs.
    CHECK_NEAR(getf(D->pose_table + BM_POSE_TARGET_YAW), 1.6, 1e-5);
    CHECK_NEAR(getf(D->pose_table + BM_POSE_RECORD + BM_POSE_TARGET_YAW), -1.6, 1e-5);
    CHECK_NEAR(getf(D->pose_table + BM_POSE_ACCEL), 0.037, TOL);

    // The camera gets rac1's strafe requests, its timers cleared and the recentring's weight put
    // back.
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

    // The flip from standing: X pressed with the stick fully left of his facing.
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

    // With `l2`, R2 stays the game's.
    s_strafe = "l2";
    load_mod();
    putu(P(BM_PAD_HELD), BM_PAD_R2);
    CALL_PAD();
    CHECK_EQ(getu(P(BM_PAD_HELD)), BM_PAD_R2);

    // Under a level's own camera there is no strafe, so no camera requests.
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

/* The backpedal: the run animation stepped back, and the original run with no step of its own. */
static void backpedal(void) {
    defaults();
    load_mod();
    reset_game();
    putu(H(BM_HERO_STATE), 2);
    putu(H(BM_HERO_CONTROL_MODE), 1);
    putf(H(BM_HERO_STICK), 1.0f);
    putf(H(BM_HERO_STICK_Y), 1.0f); /* down: back */

    // A run of four keys, each at rate 1, with a footstep at place 30 (halfway through key 1).
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

    // 0.3 - 0.5 lands at key 1's 0.8 (place 28), so the step back crosses the footstep at 30.
    CHECK_EQ(g_mem[MOBY + BM_MOBY_FRAME], 1);
    CHECK_EQ(g_mem[MOBY + BM_MOBY_NEXT], 2);
    CHECK_NEAR(getf(MOBY + BM_MOBY_ANIM_TIME), 0.8, TOL);
    CHECK_EQ(getu(MOBY + BM_MOBY_FRAME_DATA), FRAMES + 0x100);
    CHECK_EQ(g_sound_calls, 1);
    CHECK_EQ(g_sound_id, 77);
    CHECK_NEAR(g_advance_speed, 0.0, TOL);
    CHECK_NEAR(getf(H(BM_HERO_ANIM_SPEED)), 0.5, TOL);
}

/* ---- The wrench ---- */

/* Returns the address of the history slot `age` polls back, 1 being the newest (reset_game puts
 * the history's index at 5). */
static uint32_t slot_of(int age) {
    return P(BM_PAD_HIST_BTN + 4 * (uint32_t)((5 - age + 30) % 30));
}

/* Puts the wrench in hand, ready. */
static void wrench_in_hand(void) {
    putu(H(BM_HERO_ITEM_MOBY), MOBY);
    putu(H(BM_HERO_ITEM_STATE), 2);
    putu(H(BM_HERO_ITEM_ID), 8);
}

/* rac1's lock-on turn at 60 Hz. */
static void lock_on_tick(void) {
    CALL_TURN(0, 0.05f, 0.2f, 15.009831f / 60.0f);
}

/* The same at 50 Hz: the spring x 1.44, the limit by 1/50. */
static void lock_on_tick_pal(void) {
    CALL_TURN(0, 0.05f * 1.44f, 0.2f * 1.44f, 15.009831f / 50.0f);
}

/* rac1's slam with no target: its speed step toward a target speed of 0. */
static void slam_tick(void) {
    putf(H(BM_HERO_TARGET_SPEED), 0.0f); /* rac1's slam without a target */
    CALL_APPROACH(15.0f / 3600.0f, 15.0f / 3600.0f);
}

/* Going Commando's wrench: the lock-on turn, the swings' steers and their undo, Square's buffer, no
 * Square while the wrench is out, the slam's drive, and the setting off. */
static void wrench(void) {
    defaults();
    load_mod();
    reset_game();
    wrench_in_hand();

    // In a swing, the lock-on turn's 15.01 radians a second becomes 0.2821616 a tick; the throw's
    // accel is 0.057.
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
    // is shown frame 31.5 (past its exit, 31). There rac1's windows are 15 polls for Square and 24
    // for X, but rac2's at 26.5 are only 14, so a Square 15 back and an X 20 back are hidden.
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

    // The transitions went to idle. The frame is still put back before the animation advance, and
    // the presses before the next poll.
    putu(H(BM_HERO_STATE), 0);
    CALL_ADVANCE();
    CHECK_NEAR(g_advance_frame, 26.5, TOL);
    CALL_PAD();
    CHECK_EQ(getu(slot_of(15)), BM_PAD_SQUARE);
    CHECK_EQ(getu(slot_of(20)), BM_PAD_CROSS);
    putu(slot_of(15), 0);
    putu(slot_of(20), 0);

    // The third swing at 22 with the stick centred: rac1 would stand (past 21) but rac2 waits, so
    // the blend flag holds it for the transitions and is put back after.
    putu(H(BM_HERO_STATE), 0x13);
    putu(H(BM_HERO_ATTACK_RECORD), 2);
    putf(H(BM_HERO_ANIM_FRAME), 22.0f);
    putf(H(BM_HERO_STICK), 0.0f);
    CALL_UPDATE();
    CHECK_EQ(getu(H(BM_HERO_ANIM_CHANGING)), 1);
    CALL_ADVANCE();
    CHECK_EQ(g_advance_changing, 0);

    // The throw at 36 with X 3 polls back: rac2 jumps out, so rac1 is shown frame 88. A Square 5
    // back is hidden (at 88 it would throw again); X stays.
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

    // Standing with Square 9 polls back: rac1 counts 7, rac2 10, so it is shown in the newest poll.
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

    // The wrench out in a jump: Square 3 polls back is hidden, or rac1 would slam with no wrench.
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

    // The slam 5 above the ground, with no target, on its 10th tick: the target speed is 4.5 a
    // second (0.075) for the call and rac1's 0 after.
    putu(H(BM_HERO_STATE), 0x14);
    putu(H(BM_HERO_STATE_TIMER), 10);
    putf(H(BM_HERO_GROUND_DISTANCE), 5.0f);
    g_inside = slam_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 0.075, TOL);
    CHECK_NEAR(getf(H(BM_HERO_TARGET_SPEED)), 0.0, TOL);

    // 3 above the ground, rac1's target stands.
    putf(H(BM_HERO_GROUND_DISTANCE), 3.0f);
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 0.0, TOL);
    g_inside = NULL;
    CALL_ADVANCE();

    // With the setting off, the turn goes through as it came and nothing is steered.
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

    // Without the Square, he flips.
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    putu(slot_of(1), BM_PAD_CROSS);
    putu(slot_of(3), 0);
    CALL_UPDATE();
    CHECK_EQ((getu(P(BM_PAD_HELD)) & BM_PAD_R1) != 0, 1);
    CALL_ADVANCE();
    CALL_PAD();
}

/* With the ground setting off, the calls go through as they came. */
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

    // The speeds are rac1's; the hook is taken for the wrench's slam.
    CHECK_NEAR(g_approach_target, 5.7 / 60.0, TOL);
    CHECK_NEAR(g_approach_args[1], 8.5 / 3600.0, 1e-7);

    // With the wrench and Clank off too, the speeds' hook is not taken.
    s_wrench = 0;
    s_clank = "rac1";
    load_mod();
    CHECK_EQ(g_hooks[APPROACH].replacement == NULL, 1);
    g_inside = NULL;
}

/* ---- The camera ---- */

/* Runs the follow camera's update as far as the mod sees it: the gain from the game's table
 * (follow_camera_update), then the overrides, which the mod hooks. */
static void camera_update(float table_gain) {
    putf(D->cam_gain, table_gain);
    CALL_CAMERA();
}

/* Puts him in the run, strafing on L2, under the follow camera. */
static void strafing(void) {
    putu(H(BM_HERO_STATE), 2);
    putu(H(BM_HERO_CONTROL_MODE), 1);
    putu(P(BM_PAD_HELD), BM_PAD_L2);
    CALL_PAD();
    g_inside = NULL;
    CALL_UPDATE();
}

/* The `camera` setting's turn: each choice's gain, the only word written, put back before the next
 * tick; a saved `modern` (removed after 0.3.0) read as the default, Enhanced; nothing steered out
 * of play or written into another level's program; and the strafe camera alongside. */
static void camera_turn(void) {
    const float slow = 0.017453292f;
    const float medium = 0.02268928f;
    const float fast = 0.027925268f;
    const float enhanced = 0.03839725f / 0.02268928f;

    // Going Commando's keeps the game's slow and middle steps and turns its fastest at 2.2 degrees
    // a tick for 1.6.
    defaults();
    s_camera = "rac2";
    load_mod();
    reset_game();
    putf(P(BM_PAD_ANALOG), 1.0f);
    camera_update(medium);
    CHECK_EQ(getf(D->cam_gain) == medium, 1);
    CALL_PAD();
    putu(D->cam_option, 0);
    camera_update(slow);
    CHECK_EQ(getf(D->cam_gain) == slow, 1);
    CALL_PAD();
    putu(D->cam_option, 2);
    camera_update(fast);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == fast, 1);
    putu(D->cam_option, 1);
    putf(P(BM_PAD_ANALOG), 0.0f);

    // Enhanced, the default, at the game's middle step gives Going Commando's fastest, 2.2 degrees
    // a tick for 1.3.
    defaults();
    load_mod();
    reset_game();
    g_camera_calls = 0;
    camera_update(medium);
    CHECK_EQ(g_camera_calls, 1);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == medium, 1);

    // At the fast step with the stick turning the camera: 1.6 degrees a tick times 2.2 / 1.3, and no
    // other game word written. One update runs first, so the stub for the game's camera code has already
    // made its writes.
    putu(D->cam_option, 2);
    putf(P(BM_PAD_ANALOG), 1.0f);
    putf(P(BM_PAD_ANALOG + 4), 0.6f);
    camera_update(fast);
    CALL_PAD();
    memcpy(g_before, g_mem, MEMORY_SIZE);
    camera_update(fast);
    CHECK_NEAR(getf(D->cam_gain), fast * enhanced, TOL);
    CHECK_EQ(changed_besides(D->cam_gain), 0);

    // The next poll puts it back; the game would reset it from its table anyway.
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == fast, 1);

    // A second update with no table write or poll in between puts the gain back first, so it is
    // scaled once, not twice.
    camera_update(fast);
    CALL_CAMERA();
    CHECK_NEAR(getf(D->cam_gain), fast * enhanced, TOL);
    CALL_PAD();
    CHECK_EQ(getf(D->cam_gain) == fast, 1);

    // A saved `modern`, which the manifest no longer lists, gives the default, Enhanced, on every
    // step and with the stick on the camera (0.3.0 tripled the gain and stiffened the eye).
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
    camera_update(slow);
    CHECK_NEAR(getf(D->cam_gain), slow * enhanced, TOL);
    CALL_PAD();
    putu(D->cam_option, 1);
    putf(P(BM_PAD_ANALOG + 4), 0.0f);

    // With another level loaded before the poll, its program is elsewhere, so nothing goes back.
    camera_update(medium);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    g_level = 3;
    CALL_PAD();
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    g_level = 0;

    // Out of play (a scene), nothing is steered.
    s_camera = "enhanced";
    load_mod();
    reset_game();
    putf(P(BM_PAD_ANALOG), 1.0f);
    putu(D->cinematic_mode, 1);
    camera_update(medium);
    CHECK_EQ(getf(D->cam_gain) == medium, 1);
    putu(D->cinematic_mode, 0);

    // While strafing, the camera gets both rac1's strafe requests and the turn's gain.
    reset_game();
    strafing();
    g_orbit_calls = 0;
    camera_update(medium);
    CHECK_EQ(g_orbit_calls, 1);
    CHECK_NEAR(getf(D->cam_gain), 0.03839725, TOL);
    CALL_ADVANCE();
    CALL_PAD();

    // With the first game's camera, the strafe's requests still go out; the gain stays the game's.
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

/* ---- Clank ---- */

/* Clank's run inside hero_update: rac1's fast turn at full stick (t = 1.35), its speed step toward
 * the stick's 3 u/s, and its velocity along his facing. */
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

/* Clank's punch: rac1's lock-on turn. */
static void clank_punch_tick(void) {
    CALL_TURN(0, 0.05f, 0.2f, 15.009831f / 60.0f);
}

/* Puts Clank alone in play (vehicle mode 1), in `state`. */
static void clank_in(int state) {
    g_mem[H(BM_HERO_VEHICLE_MODE)] = 1;
    putu(H(BM_HERO_STATE), (uint32_t)state);
}

/* Clank under each choice: his run's turn, speed and velocity, his jump's entry, a remembered
 * Square, the punch's way out and lock-on turn, and the steers put back. Nothing changes for the
 * first game's choice, for Giant Clank or in a scene, and Ratchet's run still works after him. */
static void clank(void) {
    // Enhanced, in his run at full stick: rac2's spring, up 18 and down 20, and the target times
    // 3.45 / 3 for the call (rac1's put back after), along his facing.
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

    // Slow (0.3 u/s), with the turn leaving him 0.25 rad off, he moves along the stick, at the move
    // yaw.
    putf(H(BM_HERO_FLAT_SPEED), 0.005f);
    CALL_UPDATE();
    CHECK_NEAR(g_planar_angle, 0.4, TOL);
    CALL_ADVANCE();

    // Slow, with the turn leaving him 2 rad off at full stick, he turns around; the next tick's
    // turn takes the turnaround spring (0.037 / 0.15 / 11.69).
    g_turn_error = 2.0f;
    CALL_UPDATE();
    CALL_ADVANCE();
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.037, TOL);
    CHECK_NEAR(g_turn_args[2], 0.19489513, TOL);
    CALL_ADVANCE();

    // A new run starts with the flags clear, as rac2's run entry does, so its first turn is the
    // plain run's.
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

    // On his jump's first tick, rac1's takeoff 5, frames 18 / 27 and air speed 2 become 3, 15 / 24
    // and 2.5.
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

    // Standing with Square 11 polls back: rac1 remembers 9 polls and enhanced 13, so it is shown in
    // the newest poll for the transitions and taken back at the next poll.
    clank_in(0x43);
    putu(slot_of(11), BM_PAD_SQUARE);
    CALL_UPDATE();
    CHECK_EQ(getu(slot_of(1)), BM_PAD_SQUARE);
    CALL_ADVANCE();
    CALL_PAD();
    CHECK_EQ(getu(slot_of(1)), 0);
    CHECK_EQ(getu(slot_of(11)), BM_PAD_SQUARE);
    putu(slot_of(11), 0);

    // The punch at frame 8.5 with the stick pushed: rac1 is shown 10.5 and stands (then runs), and
    // X 20 polls back is hidden for the tick. The frame and X are put back.
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

    // The punch's lock-on turn is 16.93 radians a second.
    clank_in(0x51);
    putf(H(BM_HERO_ANIM_FRAME), 4.0f);
    g_inside = clank_punch_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[2], 0.2821616, TOL);
    CALL_ADVANCE();

    // Going Commando's keeps no Square past 9 polls and has no run out at 8.5, but stands past 9.
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

    // X 9 polls back at frame 7.5: rac2's punch, a frame ahead, jumps out, so rac1 is shown 8.5,
    // whose window takes the press (X stays).
    putf(H(BM_HERO_ANIM_FRAME), 7.5f);
    putu(slot_of(9), BM_PAD_CROSS);
    CALL_UPDATE();
    CHECK_NEAR(getf(H(BM_HERO_ANIM_FRAME)), 8.5, TOL);
    CHECK_EQ(getu(slot_of(9)), BM_PAD_CROSS);
    CALL_ADVANCE();
    CALL_PAD();
    putu(slot_of(9), 0);

    // In his run the target is rac1's, 3 u/s.
    clank_in(0x44);
    g_inside = clank_run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_approach_target, 3.0 / 60.0, TOL);
    CHECK_NEAR(g_turn_args[0], 0.033, TOL);
    CALL_ADVANCE();

    // With the first game's Clank, every call goes through as it came and nothing is steered.
    s_clank = "rac1";
    load_mod();
    memcpy(g_before, g_mem, MEMORY_SIZE);
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.008 * 1.35, TOL);
    CHECK_NEAR(g_approach_args[0], 7.5 * 0.9 / 3600.0, 1e-7);
    CHECK_NEAR(g_approach_target, 3.0 / 60.0, TOL);
    CHECK_EQ(changed_besides(H(BM_HERO_TARGET_SPEED)), 0);
    CALL_ADVANCE();

    // Giant Clank (vehicle mode 2) and Clank in a scene are left as they came.
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

    // Back to Ratchet (vehicle mode 0) in his run, Going Commando's run applies as before.
    g_mem[H(BM_HERO_VEHICLE_MODE)] = 0;
    putu(H(BM_HERO_STATE), 2);
    g_inside = run_tick;
    CALL_UPDATE();
    CHECK_NEAR(g_turn_args[0], 0.022, TOL);
    CHECK_NEAR(g_approach_target, 0.0991666667, TOL);
    g_inside = NULL;
    CALL_ADVANCE();
}

/* The 50 Hz tick on the PAL row: rac2's numbers scaled as rac1 scales its own, and rac1's tick
 * counts by 1.2. */
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

    // The exit sets the state timer past rac1's 25 ticks as PAL counts them, 30.
    putf(H(BM_HERO_STICK), 0.05f);
    putu(H(BM_HERO_STATE_TIMER), 3);
    g_inside = NULL;
    CALL_UPDATE();
    CHECK_EQ(getu(H(BM_HERO_STATE_TIMER)), 31);
    CALL_ADVANCE();

    // The wrench, standing, with Square 9 polls back, which 60 Hz buffers (rac1 7, rac2 10). At
    // 50 Hz rac1 counts 8 and rac2's PAL release 8 too, so it is not shown.
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

    // Enhanced at 50 Hz gives Going Commando's fastest step per tick at the middle step, since rac1
    // counts the gain per tick; a saved `modern` does the same.
    s_camera = "enhanced";
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

    // Clank at 50 Hz: rac1's takeoff scale_ticks(5) = 6 becomes rac2's PAL 2 and the air speed
    // 2.5 / 50; his run gets 18 / 2500 and 20 / 2500 against rac1's 7.5 and 8.5 / 2500.
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

    // For Square at 50 Hz, rac1 counts 9 as 11 and enhanced 13 as 16, so 14 polls back is shown.
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

    // The library's path is the one argument.
    if (argc < 2) {
        printf("usage: %s <mod library>\n", argv[0]);
        return 2;
    }

    lib = LIB_OPEN(argv[1]);

    // Nothing else can be tested if the library does not load.
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

    // The query reports the manifest's id, version and API (1.0).
    info.size = sizeof info;
    query(&info);
    CHECK_EQ(strcmp(info.id, RAC1_ENHANCED_MOVEMENT_ID), 0);
    CHECK_EQ(strcmp(info.version, RAC1_ENHANCED_MOVEMENT_VERSION), 0);
    CHECK_EQ(info.api_major, 1);
    CHECK_EQ(info.api_minor, 0);

    g_mem = calloc(1, MEMORY_SIZE);
    g_before = calloc(1, MEMORY_SIZE);

    // Each disc row runs every test, with the game laid out at its addresses.
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
