// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ratchet & Clank's part of the mod: Going Commando's movement through rac1's own hook points
// (DESIGN.md). The decisions are src/rac1/ground.h, air.h, strafe.h, wrench.h and camera.h, apart
// from game memory; this file reads the hero into them and applies what they return. Addresses:
// discs.h; offsets: hero.h.
//
// A tick, and where each hook acts (DESIGN.md section 1):
//   pad poll          process_report: undo last tick's pad and camera steers; take L2/R2 for the
//                     strafe
//   animation         advance_animation: undo last tick's hero steers; the idle clamp after the
//                     run's exit; the backpedal (the walk or run played backwards)
//   hero_update       update_motion_input: before, the tick's context, the jump's air speed, the
//                     run's flags, the flip's drift; inside, turn_to_heading, approach_speed and
//                     set_planar_velocity give rac2's numbers (the wrench's lock-on turn and the
//                     slam's drive too); after, the twist, and the steers rac1's transitions will
//                     read (the exit, the jump, the flip, the start, the wrench's swings and Square)
//   transitions       rac1's own, the loaded level's copy
//   follow camera     apply_overrides: the strafe camera; the turn's speed for the update that
//                     follows (the gain, and for modern the pitch's steps and the eye's springs)
#include "rac1/game.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "openrac/games/rac1.hpp"
#include "rac1/air.h"
#include "rac1/camera.h"
#include "rac1/discs.h"
#include "rac1/ground.h"
#include "rac1/hero.h"
#include "rac1/strafe.h"
#include "rac1/wrench.h"

namespace enhanced_movement::rac1 {

namespace {

using openrac::gaddr;
namespace hp = openrac::rac1;

// The settings, read once at load.
struct Settings {
    bool ground = true;
    bool air = true;
    std::uint32_t strafe = BM_PAD_L2 | BM_PAD_R2;  // the strafe's buttons; 0 off
    bool flips = true;
    bool twist = true;
    bool wrench = true;
    int camera = BM_CAMERA_RAC2;  // camera.h's BM_CAMERA_*
    bool trace = false;
};

// One value rac1's transitions read, changed for one tick (DESIGN.md section 1). `always`: put back
// even when the state changed (the animation's frame and blend flag, which the next tick's animation
// advance works out again from its own last values).
struct Steered {
    gaddr address = 0;
    std::uint32_t old_bits = 0;
    std::uint32_t wrote_bits = 0;
    bool always = false;
};

// One slot of the pad's press history changed for one tick: bits put in, bits taken out.
struct PadEdit {
    int slot = 0;
    std::uint32_t added = 0;
    std::uint32_t removed = 0;
};

// What the hooks keep between calls. Reset in load (the load test loads the mod several times).
struct State {
    bool in_update = false;    // inside hero_update: the inner hooks act
    bool active = false;       // this tick is in play, on foot
    bool strafing = false;     // the strafe this tick
    bool strafe_held = false;  // the strafe's button, from the pad
    int sector = BM_FORWARD;
    float strafe_rate = 0.0f;  // rac2's +0x145C: the strafe's own yaw rate
    float strafe_yaw = 0.0f;   // rac2's +0x1458: the stick's yaw while strafing
    bm_sides sides{};
    bm_run_flags flags{};
    float k = 1.0f;             // the run's acceleration share, from the turn
    bool flip_steered = false;  // a flip was steered at the last transitions
    bool flipping = false;      // in a flip the strafe started
    int last_state = -1;
    bm_tick tick{};
    // Steers to put back after the transitions.
    Steered hero_steers[4];
    int hero_steer_count = 0;
    int steered_state = -1;
    bool idle_clamp = false;
    std::uint32_t pad_r1 = 0;  // R1 put in pad_held
    PadEdit pad_edits[64];     // the press history's slots changed for the transitions
    int pad_edit_count = 0;
    // The follow camera's words changed for its update, put back before the next tick, and the level
    // whose program they are in.
    Steered cam_steers[8];
    int cam_steer_count = 0;
    int cam_steer_level = -1;
    int cam_settle = 0;   // ticks left of modern's spring after the stick lets go
    std::string decided;  // the trace's words for this tick
};

openrac::Mod* s_mod = nullptr;
const bm_disc* s_disc = nullptr;
Settings s_set;
State s;

using OrbitClampFn = void (*)(int32_t);
using SlideFn = void (*)();
using SpringFn = void (*)(float, float);
using PlaySoundFn = int32_t (*)(int32_t, int32_t, std::uint32_t);
OrbitClampFn s_orbit_clamp = nullptr;
SlideFn s_start_slide = nullptr;
SpringFn s_lateral_spring = nullptr;
PlaySoundFn s_play_sound = nullptr;

// ---- Game memory ---------------------------------------------------------------------------

template <class T>
T get(gaddr a) {
    return s_mod->memory().read<T>(a);
}

template <class T>
void put(gaddr a, T v) {
    s_mod->memory().write<T>(a, v);
}

// Whether a pointer read from game memory can be followed: in the EE's 32 MB, not null.
bool valid(gaddr a) {
    return a != 0 && a < 0x02000000u;
}

// A field of the hero.
gaddr hero(std::uint32_t offset) {
    return s_disc->hero + offset;
}

// A field of the first pad.
gaddr pad(std::uint32_t offset) {
    return s_disc->pad + offset;
}

// A level 0 global where the loaded level keeps it: ask each time, a level moves its globals.
gaddr level0(std::uint32_t address) {
    return s_mod->relocate_level(0, address);
}

// The tick's units: g_frame_time, its square, g_frame_scale_sq, frame_scale. NTSC's when a read is
// not sane.
bm_tick read_tick() {
    bm_tick t;
    t.ft = get<float>(s_mod->relocate(s_disc->frame_time));
    t.ft2 = get<float>(s_mod->relocate(s_disc->frame_time_sq));
    t.fs2 = get<float>(s_mod->relocate(s_disc->frame_scale_sq));
    t.fs = get<float>(s_mod->relocate(s_disc->frame_scale));

    // A tick of 1/100 to 1/25 of a second, as the game runs; else the game's numbers are not set.
    if (!(t.ft > 0.01f && t.ft < 0.04f) || !(t.ft2 > 0.0f) || !(t.fs2 > 0.0f)
        || !(t.fs > 0.5f && t.fs < 2.0f)) {
        return bm_tick_ntsc();
    }

    return t;
}

// The game's state numbers.
int state() {
    return get<int32_t>(hero(BM_HERO_STATE));
}

float camera_yaw() {
    return get<float>(level0(s_disc->camera_yaw));
}

// Whether the game is in play with Ratchet on foot: a level loaded, no scene or movie, his moby
// made, not Clank or a vehicle.
bool playing() {
    // Before the first level, and between levels, the hero is not there.
    if (s_mod->loaded_level() < 0 || get<std::uint32_t>(hero(BM_HERO_MOBY)) == 0) {
        return false;
    }

    return get<int32_t>(level0(s_disc->cinematic_mode)) == 0
           && get<std::uint8_t>(hero(BM_HERO_VEHICLE_MODE)) == 0;
}

// The control modes on foot where L2/R2 belong to the strafe (rac1.h).
bool foot_mode(int mode) {
    // Standing, running, falling, jumping, hovering, attacking, an item's wait, crouching.
    switch (mode) {
        case RAC1_MODE_IDLE:
        case RAC1_MODE_RUN:
        case RAC1_MODE_FALL:
        case RAC1_MODE_JUMP:
        case RAC1_MODE_LAND:
        case RAC1_MODE_ATTACK:
        case RAC1_MODE_ITEM_RELEASE_WAIT:
        case RAC1_MODE_CROUCH:
            return true;
        default:
            return false;
    }
}

// Whether the pad's L2/R2 go to the strafe now: the setting, play, a mode on foot, not rac1's own
// strafe-like state 0x81.
bool strafe_takes_pad() {
    return s_set.strafe != 0 && playing() && foot_mode(get<int32_t>(hero(BM_HERO_CONTROL_MODE)))
           && state() != RAC1_STATE_OWN_STRAFE;
}

// Whether the strafe applies this tick: held, the pad's, the follow camera in use, the world's
// gravity, not the long jump (rac2 clears its strafe there).
bool strafe_now() {
    // Nothing held, or the pad's buttons are the game's now.
    if (!s.strafe_held || !strafe_takes_pad()) {
        return false;
    }

    const gaddr cam = get<std::uint32_t>(level0(s_disc->follow_cam));
    return valid(cam) && get<int16_t>(cam + BM_CAMERA_KIND) == 0
           && get<std::uint8_t>(hero(BM_HERO_GRAVITY_MODE)) == 0
           && state() != RAC1_STATE_PACK_LONG_JUMP;
}

bool is_run(int st) {
    return st == RAC1_STATE_RUN || st == RAC1_STATE_RUN_WADING;
}

// The press history: how many polls back `mask` was pressed within `n` (pad_pressed_within), and
// its slot; -1 when it was not.
int pressed_within(std::uint32_t mask, int n, int* slot) {
    const int count = get<int32_t>(pad(BM_PAD_HIST_COUNT));
    const int idx = get<int16_t>(pad(BM_PAD_HIST_IDX));
    const int lim = count < n ? count : n;

    // From the newest poll back.
    for (int i = 1; i <= lim; ++i) {
        const int k = ((idx - i) % BM_PAD_HIST_LEN + BM_PAD_HIST_LEN) % BM_PAD_HIST_LEN;

        // The first poll with the press.
        if (get<std::uint32_t>(pad(BM_PAD_HIST_BTN + 4 * k)) & mask) {
            // The slot, for a caller that marks it.
            if (slot != nullptr) {
                *slot = k;
            }

            return i - 1;
        }
    }

    return -1;
}

// The stick's angle `back` polls ago (the pad's hist_ang).
float stick_angle_back(int back) {
    const int idx = get<int16_t>(pad(BM_PAD_HIST_IDX));
    const int k = ((idx - back) % BM_PAD_HIST_LEN + BM_PAD_HIST_LEN) % BM_PAD_HIST_LEN;
    return get<float>(pad(BM_PAD_HIST_ANG + 4 * k));
}

// Whether a hold-to-fire weapon (items 15, 16, 21: the Blaster, Pyrocitor, Morph-o-Ray) is in hand
// with its animation on: rac1's slowed turn (turn_hero_toward_move_direction_with_stick_scale).
bool firing() {
    // Only an item in hand counts.
    if (get<int32_t>(hero(BM_HERO_ITEM_STATE)) != 2
        || get<std::uint8_t>(hero(BM_HERO_ITEM_ANIM)) == 0) {
        return false;
    }

    const int id = get<int32_t>(hero(BM_HERO_ITEM_ID));
    return id == 15 || id == 16 || id == 21;
}

// The run's inputs, with the yaw error as it is now.
bm_run_in run_in() {
    bm_run_in in{};
    in.stick = get<float>(hero(BM_HERO_STICK));
    in.yaw_error = get<float>(hero(BM_HERO_YAW_ERROR));
    in.flat_speed = get<float>(hero(BM_HERO_FLAT_SPEED));
    in.step = get<int32_t>(hero(BM_HERO_STEP));
    in.wading = state() == RAC1_STATE_RUN_WADING;
    in.item_flag = get<std::uint8_t>(hero(BM_HERO_ITEM_FLAG)) != 0;
    in.firing = firing();
    in.stick_swing = bm_diff(stick_angle_back(1), stick_angle_back(7));
    in.strafing = s.strafing;
    return in;
}

// ---- Steering rac1's transitions -----------------------------------------------------------

// Changes a hero value for the transitions about to run, to be put back after them.
template <class T>
void steer(gaddr a, T v, bool always = false) {
    static_assert(sizeof(T) == 4, "a steered value is a word");

    // The few a tick needs; never more.
    if (s.hero_steer_count >= 4) {
        return;
    }

    Steered& st = s.hero_steers[s.hero_steer_count++];
    st.address = a;
    st.old_bits = get<std::uint32_t>(a);
    put<T>(a, v);
    st.wrote_bits = get<std::uint32_t>(a);
    st.always = always;
}

// Puts back the hero values steered for the last transitions: only where the state did not change
// (or the value is one to put back always) and the value is still what the mod wrote (else the game
// has moved on from it).
void undo_hero_steers() {
    const bool same = state() == s.steered_state;

    // Newest first, so a value steered twice gets its first old value back.
    for (int i = s.hero_steer_count - 1; i >= 0; --i) {
        const Steered& st = s.hero_steers[i];

        // The game has not changed it since.
        if ((same || st.always) && get<std::uint32_t>(st.address) == st.wrote_bits) {
            put<std::uint32_t>(st.address, st.old_bits);
        }
    }

    s.hero_steer_count = 0;
}

// Takes back the pad steers (R1 in pad_held, the flick and the wrench's presses in the press
// history) before the next poll.
void undo_pad_steers() {
    // R1, only if the mod put it there.
    if (s.pad_r1 != 0) {
        put<std::uint32_t>(pad(BM_PAD_HELD), get<std::uint32_t>(pad(BM_PAD_HELD)) & ~s.pad_r1);
        put<std::uint32_t>(
            pad(BM_PAD_PREV_HELD), get<std::uint32_t>(pad(BM_PAD_PREV_HELD)) & ~s.pad_r1
        );
        s.pad_r1 = 0;
    }

    // The history's slots, newest edit first.
    for (int i = s.pad_edit_count - 1; i >= 0; --i) {
        const PadEdit& e = s.pad_edits[i];
        const gaddr slot = pad(BM_PAD_HIST_BTN + 4 * e.slot);
        put<std::uint32_t>(slot, (get<std::uint32_t>(slot) & ~e.added) | e.removed);
    }

    s.pad_edit_count = 0;
}

// Puts `bits` in a slot of the press history for the transitions about to run.
void pad_add(int slot, std::uint32_t bits) {
    const gaddr a = pad(BM_PAD_HIST_BTN + 4 * slot);
    const std::uint32_t added = bits & ~get<std::uint32_t>(a);

    // Only bits not there already, and room to put them back.
    if (added == 0 || s.pad_edit_count >= 64) {
        return;
    }

    put<std::uint32_t>(a, get<std::uint32_t>(a) | added);
    s.pad_edits[s.pad_edit_count++] = PadEdit{slot, added, 0};
}

// Takes `mask`'s presses out of the newest `n` polls of the history for the transitions about to
// run.
void pad_hide(std::uint32_t mask, int n) {
    const int count = get<int32_t>(pad(BM_PAD_HIST_COUNT));
    const int idx = get<int16_t>(pad(BM_PAD_HIST_IDX));
    const int lim = count < n ? count : n;

    // From the newest poll back.
    for (int i = 1; i <= lim && s.pad_edit_count < 64; ++i) {
        const int k = ((idx - i) % BM_PAD_HIST_LEN + BM_PAD_HIST_LEN) % BM_PAD_HIST_LEN;
        const gaddr a = pad(BM_PAD_HIST_BTN + 4 * k);
        const std::uint32_t removed = get<std::uint32_t>(a) & mask;

        // A press to hide.
        if (removed != 0) {
            put<std::uint32_t>(a, get<std::uint32_t>(a) & ~removed);
            s.pad_edits[s.pad_edit_count++] = PadEdit{k, 0, removed};
        }
    }
}

// How many polls back `mask` was last pressed (1 the newest), 0 when the history has none.
int press_age(std::uint32_t mask) {
    return pressed_within(mask, BM_PAD_HIST_LEN, nullptr) + 1;
}

// ---- The wrench -----------------------------------------------------------------------------

// Whether the wrench is the item in hand, ready (rac1's item code takes Square for it then).
bool wrench_in_hand() {
    return get<int32_t>(hero(BM_HERO_ITEM_ID)) == RAC1_ITEM_WRENCH
           && get<int32_t>(hero(BM_HERO_ITEM_STATE)) == 2
           && get<std::uint32_t>(hero(BM_HERO_ITEM_MOBY)) != 0;
}

// What the transitions read of a swing.
bm_swing_in swing_in(int st) {
    bm_swing_in in{};
    in.state = st;
    in.record = get<int32_t>(hero(BM_HERO_ATTACK_RECORD));
    in.frame = get<float>(hero(BM_HERO_ANIM_FRAME));
    in.changing = get<int32_t>(hero(BM_HERO_ANIM_CHANGING)) != 0;
    in.done = (get<int32_t>(hero(BM_HERO_ANIM_FLAGS)) & 2) != 0;
    in.stick = get<float>(hero(BM_HERO_STICK));
    in.timer = get<int32_t>(hero(BM_HERO_STATE_TIMER));
    in.square = press_age(BM_PAD_SQUARE);
    in.cross = press_age(BM_PAD_CROSS);
    in.crouch = (get<std::uint32_t>(pad(BM_PAD_HELD)) & (BM_PAD_R1 | BM_PAD_R2)) != 0;
    in.unk308 = get<int16_t>(hero(BM_HERO_UNK308));
    in.slip = get<std::uint8_t>(hero(BM_HERO_ON_SLIP)) != 0;
    in.out = get<std::uint8_t>(hero(BM_HERO_ITEM_THROWN)) != 0;
    return in;
}

// Going Commando's wrench for this tick's transitions (DESIGN.md section 5), after hero_update:
// the swings and the throw end, chain and jump as rac2's do; Square counts as long as rac2 counts
// it; while the wrench is out, Square starts nothing. Returns whether rac1's item code takes a
// Square this tick on foot (the flip then must not show it R1, which would make it the throw).
bool steer_wrench(int st) {
    // The wrench in hand, ready.
    if (!wrench_in_hand()) {
        return false;
    }

    const int age = press_age(BM_PAD_SQUARE);
    bm_reach_in reach{};
    reach.state = st;
    reach.control_mode = get<int32_t>(hero(BM_HERO_CONTROL_MODE));
    reach.timer = get<int32_t>(hero(BM_HERO_STATE_TIMER));
    reach.rail_land = get<int32_t>(hero(BM_HERO_RAIL_LAND));
    reach.unk450 = get<int32_t>(hero(BM_HERO_UNK450));
    reach.frame = get<float>(hero(BM_HERO_ANIM_FRAME));
    reach.slip = get<std::uint8_t>(hero(BM_HERO_ON_SLIP)) != 0;
    const bm_windows w = bm_wrench_windows(&reach, &s.tick);
    const bool on_foot = reach.control_mode < 2;

    // The setting off: rac1's own windows decide.
    if (!s_set.wrench) {
        return on_foot && bm_within(age, w.rac1);
    }

    const bool out = get<std::uint8_t>(hero(BM_HERO_ITEM_THROWN)) != 0;

    // Thrown and not back: rac2 takes no Square at all (rac1 would slam, or throw from the crouch).
    if (out && bm_within(age, bm_scale_ticks(RAC1_WRENCH_AIR_LATE_WINDOW, &s.tick))) {
        pad_hide(BM_PAD_SQUARE, bm_scale_ticks(RAC1_WRENCH_AIR_LATE_WINDOW, &s.tick));
        s.decided += " wrench-out";
    }

    // A swing or the throw: rac2's way out of it, or into the next.
    if (st == RAC1_STATE_WRENCH_COMBO || st == RAC1_STATE_CROUCH_SWING) {
        const bm_swing_in in = swing_in(st);
        const bm_swing_steer sw = bm_swing_steer_for(&in, &s.tick);
        static const char* const k_act[] = {"stay", "chain", "jump", "run", "idle"};

        // What rac1's own code needs to see to do it.
        if (sw.kind == BM_STEER_FRAME) {
            steer<float>(hero(BM_HERO_ANIM_FRAME), sw.frame, true);
        } else if (sw.kind == BM_STEER_HOLD) {
            steer<int32_t>(hero(BM_HERO_ANIM_CHANGING), 1, true);
        }

        // Presses that would otherwise count at the steered frame.
        if (sw.kind == BM_STEER_FRAME && sw.hide_square > 0) {
            pad_hide(BM_PAD_SQUARE, sw.hide_square);
        }

        // X too.
        if (sw.kind == BM_STEER_FRAME && sw.hide_cross > 0) {
            pad_hide(BM_PAD_CROSS, sw.hide_cross);
        }

        // The trace says it only when it differs.
        if (sw.rac2 != sw.rac1) {
            s.decided += std::string(" swing-") + k_act[sw.rac2] + "-for-" + k_act[sw.rac1]
                         + (sw.kind == BM_STEER_FRAME  ? " steer-frame"
                            : sw.kind == BM_STEER_HOLD ? " steer-hold"
                                                       : "");
        }

        return false;
    }

    // Nothing more while the wrench is out.
    if (out) {
        return false;
    }

    // Square pressed earlier than rac1 counts but within rac2's window: shown as pressed this poll.
    if (bm_wrench_buffer(w, age)) {
        const int newest =
            (get<int16_t>(pad(BM_PAD_HIST_IDX)) + BM_PAD_HIST_LEN - 1) % BM_PAD_HIST_LEN;
        pad_add(newest, BM_PAD_SQUARE);
        s.decided += " square-buffer";
        return on_foot;
    }

    return on_foot && bm_within(age, w.rac1);
}

// The steers for this tick's transitions (DESIGN.md 2.3, 2.4, 4.2), after hero_update. `square`:
// rac1's item code takes a Square this tick.
void steer_transitions(int st, bool square) {
    const float stick = get<float>(hero(BM_HERO_STICK));
    const float facing = get<float>(hero(BM_HERO_FACING));
    const float off_camera = bm_diff(facing, camera_yaw());
    const int window =
        bm_scale_ticks(is_run(st) ? RAC1_RUN_JUMP_WINDOW : RAC1_IDLE_JUMP_WINDOW, &s.tick);
    int x_slot = -1;
    const bool x = pressed_within(BM_PAD_CROSS, window, &x_slot) >= 0;

    s.steered_state = st;

    // The flip: rac1's own crouch high jump, shown R1 held and a stick press beside X. Not with a
    // Square the wrench takes this tick: rac2 swings then, and R1 would make rac1 throw.
    if (s_set.flips && s.strafing && x && !square
        && (st == RAC1_STATE_IDLE || st == RAC1_STATE_RUN)) {
        const int dir = bm_stick_vs_facing(get<float>(hero(BM_HERO_MOVE_YAW)), facing);
        const bool idle_ok = get<int16_t>(hero(BM_HERO_UNK308)) != 1;
        const bool run_ok = get<int16_t>(hero(BM_HERO_AIR_FRAMES))
                            <= bm_scale_ticks(RAC1_RUN_JUMP_AIR_TICKS, &s.tick);

        // rac2's test, and what rac1's own jump code needs to reach its crouch jump this tick.
        if (bm_flip(stick, dir) && (st == RAC1_STATE_IDLE ? idle_ok : run_ok)) {
            const std::uint32_t held = get<std::uint32_t>(pad(BM_PAD_HELD));

            // R1 held, unless it already is.
            if (!(held & BM_PAD_R1)) {
                put<std::uint32_t>(pad(BM_PAD_HELD), held | BM_PAD_R1);
                s.pad_r1 = BM_PAD_R1;
            }

            pad_add(x_slot, BM_PAD_FLICK);
            s.flip_steered = true;
            s.decided += " flip";
            return;
        }
    }

    // The jump X gives in the run: the plain jump where rac2 gives it (no running jump while
    // strafing, nor under 0.85 of stick). A target of 0 is under rac1's walk band.
    if ((s_set.ground || s.strafing) && st == RAC1_STATE_RUN && x
        && !bm_rac2_running_jump(stick, s.strafing)) {
        steer<float>(hero(BM_HERO_TARGET_SPEED), 0.0f);
        s.decided += " plain-jump";
        return;
    }

    // Leaving the run: idle at once, without the skid (rac1's own code picks idle when the speed is
    // under its STOP test and the state has run 25 ticks).
    if (s_set.ground && st == RAC1_STATE_RUN && !x && bm_run_exit(stick, s.strafing, off_camera)) {
        const float stop = RAC1_EXIT_STOP_SPEED * s.tick.ft;

        // Under rac1's STOP speed.
        if (get<float>(hero(BM_HERO_SPEED_XY)) >= stop) {
            steer<float>(hero(BM_HERO_SPEED_XY), stop * 0.99f);
        }

        const int idle_after = bm_scale_ticks(RAC1_EXIT_IDLE_TICKS, &s.tick);

        // Past rac1's 25 ticks (scaled as rac1 scales them: 30 on PAL).
        if (get<int32_t>(hero(BM_HERO_STATE_TIMER)) <= idle_after) {
            steer<int32_t>(hero(BM_HERO_STATE_TIMER), idle_after + 1);
        }

        s.idle_clamp = true;
        s.decided += " exit";
        return;
    }

    // Standing, strafing, off the camera: the walk's start (rac1's own, at a stick past 0.22).
    if (s.strafing && st == RAC1_STATE_IDLE && bm_strafe_start(facing, camera_yaw(), stick)) {
        steer<float>(hero(BM_HERO_STICK), RAC2_STRAFE_START_STICK + 0.01f);
        s.decided += " strafe-start";
    }
}

// ---- The twist ----------------------------------------------------------------------------

// rac2's twist (func_L00_002B7B80) on rac1's pose records 0 and 1, before their easing runs.
void twist() {
    const gaddr table = level0(s_disc->pose_table);
    const gaddr hips = table + BM_POSE_HIPS * BM_POSE_RECORD;
    const gaddr body = table + BM_POSE_BODY * BM_POSE_RECORD;
    const float yaw = bm_twist(s.strafe_yaw, get<float>(hero(BM_HERO_FACING)));

    put<float>(hips + BM_POSE_TARGET_YAW, yaw);
    put<float>(hips + BM_POSE_ACCEL, RAC2_TWIST_HIP_ACCEL);
    put<float>(hips + BM_POSE_DAMP, RAC2_TWIST_HIP_DAMP);
    put<float>(body + BM_POSE_TARGET_YAW, -yaw);
    put<float>(body + BM_POSE_ACCEL, RAC2_TWIST_BODY_ACCEL);
    put<float>(body + BM_POSE_DAMP, RAC2_TWIST_BODY_DAMP);
}

// ---- The follow camera's turn ---------------------------------------------------------------

// Changes a word of the follow camera's for the update about to read it, to be put back before the
// next tick (undo_camera_steers).
void cam_steer(gaddr a, float v) {
    // The few an update needs; never more.
    if (s.cam_steer_count >= 8) {
        return;
    }

    Steered& st = s.cam_steers[s.cam_steer_count++];
    st.address = a;
    st.old_bits = get<std::uint32_t>(a);
    put<float>(a, v);
    st.wrote_bits = get<std::uint32_t>(a);
    s.cam_steer_level = s_mod->loaded_level();
}

// Puts back the follow camera's words steered for its last update: each that still holds what the
// mod wrote (rac1 sets the gain again from its table itself), and only in the level whose program they
// are in (another level's program is somewhere else now).
void undo_camera_steers() {
    // Newest first, so a word steered twice gets its first old value back.
    for (int i = s.cam_steer_count - 1; i >= 0 && s_mod->loaded_level() == s.cam_steer_level; --i) {
        const Steered& st = s.cam_steers[i];

        // The game has not changed it since.
        if (get<std::uint32_t>(st.address) == st.wrote_bits) {
            put<std::uint32_t>(st.address, st.old_bits);
        }
    }

    s.cam_steer_count = 0;
}

// The turn's speed for the follow camera's update that follows its overrides (DESIGN.md section 8):
// the gain the stick turns it by, and for modern the pitch's steps and, while the stick turns it, the
// eye's springs. `cam` is the camera's moby.
void camera_speed(gaddr cam) {
    // In play: a level, his moby, no scene. On foot or not: Clank's follow camera turns as Ratchet's.
    if (s_mod->loaded_level() < 0 || get<std::uint32_t>(hero(BM_HERO_MOBY)) == 0
        || get<int32_t>(level0(s_disc->cinematic_mode)) != 0) {
        s.cam_settle = 0;
        return;
    }

    const bm_tick t = read_tick();
    const gaddr gain_at = level0(s_disc->cam_gain);
    const float gain = get<float>(gain_at);
    const int option = get<int32_t>(s_mod->relocate(s_disc->cam_option));
    const float want = bm_camera_gain(s_set.camera, gain, option, &t);

    // Only a change: the same gain is no steer.
    if (want != gain) {
        cam_steer(gain_at, want);
    }

    const float x = get<float>(pad(BM_PAD_ANALOG));
    const float y = get<float>(pad(BM_PAD_ANALOG + 4));
    const gaddr vars = get<std::uint32_t>(cam + BM_CAMERA_VARS);
    const int flags = valid(vars) ? get<int32_t>(vars + BM_CAMERA_TURN_FLAGS) : 3;
    const bool turning = bm_camera_turning(x, y, flags) != 0;
    const float aim = valid(vars) ? get<float>(vars + BM_CAMERA_PITCH_AIM) : 0.0f;

    // Modern: the pitch three times as quick, and the eye held close while the stick turns it.
    if (s_set.camera == BM_CAMERA_MODERN) {
        // The pitch's steps, while the pitch is the player's: scaled from what is there.
        for (std::uint32_t at :
             {s_disc->cam_pitch_out, s_disc->cam_pitch_back, s_disc->cam_pitch_step}) {
            const gaddr a = level0(at);

            // Only the player's pitch: the game levels the orbit with the same steps.
            if (bm_camera_pitching(y, flags, aim)) {
                cam_steer(a, bm_camera_pitch_step(s_set.camera, get<float>(a), &t));
            }
        }

        const bool stiff = turning || s.cam_settle > 0;
        s.cam_settle = bm_camera_settle(s.cam_settle, turning ? 1 : 0, &t);

        // The springs, until the eye has caught up after the stick lets go.
        if (stiff) {
            cam_steer(level0(s_disc->cam_yaw_accel), bm_camera_spring_accel(&t));
            cam_steer(level0(s_disc->cam_yaw_damp), bm_camera_spring_damp(&t));
            cam_steer(level0(s_disc->cam_pitch_accel), bm_camera_spring_accel(&t));
            cam_steer(level0(s_disc->cam_pitch_damp), bm_camera_spring_damp(&t));
        }
    }

    // The trace's line for the camera, which updates after the hero's.
    if (s_set.trace && (turning || s.cam_settle > 0)) {
        char line[200];
        std::snprintf(
            line,
            sizeof line,
            "camera option %d gain %.5f -> %.5f stick %.2f %.2f aim %.2f flags %d settle %d",
            option,
            gain,
            want,
            x,
            y,
            aim,
            flags,
            s.cam_settle
        );
        s_mod->info(line);
    }
}

// ---- The hooks ----------------------------------------------------------------------------

// The trace's line for the tick, with every input the decisions used.
void trace_tick(int st) {
    // Only when asked for.
    if (!s_set.trace) {
        return;
    }

    char line[512];
    std::snprintf(
        line,
        sizeof line,
        "state %#x step %d stick %.3f (%.2f, %.2f) sector %d strafe %d "
        "along %d turnaround %d err %.3f flat %.4f target %.4f ground %.4f "
        "facing %.3f camera %.3f k %.1f flip %d anim %.2f rec %d square %d cross %d%s",
        st,
        get<int32_t>(hero(BM_HERO_STEP)),
        get<float>(hero(BM_HERO_STICK)),
        get<float>(hero(BM_HERO_STICK_X)),
        get<float>(hero(BM_HERO_STICK_Y)),
        s.sector,
        s.strafing ? 1 : 0,
        s.flags.along,
        s.flags.turnaround,
        get<float>(hero(BM_HERO_YAW_ERROR)),
        get<float>(hero(BM_HERO_FLAT_SPEED)),
        get<float>(hero(BM_HERO_TARGET_SPEED)),
        get<float>(hero(BM_HERO_GROUND_SPEED)),
        get<float>(hero(BM_HERO_FACING)),
        camera_yaw(),
        s.k,
        s.flipping ? 1 : 0,
        get<float>(hero(BM_HERO_ANIM_FRAME)),
        get<int32_t>(hero(BM_HERO_ATTACK_RECORD)),
        press_age(BM_PAD_SQUARE),
        press_age(BM_PAD_CROSS),
        s.decided.c_str()
    );
    s_mod->info(line);
}

// rac1.pad.process_report: takes back last tick's pad steers, then takes the strafe's buttons out of
// what the game sees on foot and keeps them for the strafe.
template <class Original>
void process_report(Original original, gaddr p, gaddr report, int32_t size) {
    // Only the first pad's state is the hero's.
    if (p != s_disc->pad) {
        original(p, report, size);
        return;
    }

    undo_pad_steers();
    undo_camera_steers();
    original(p, report, size);
    s.strafe_held = false;

    // The strafe is off, or the buttons are the game's now.
    if (!strafe_takes_pad()) {
        return;
    }

    const std::uint32_t mask = s_set.strafe;
    s.strafe_held = (get<std::uint32_t>(pad(BM_PAD_HELD)) & mask) != 0;

    // Gone from every copy the hero code reads; the menus' _unmasked copies are left alone.
    for (std::uint32_t field :
         {BM_PAD_HELD,
          BM_PAD_PRESSED,
          BM_PAD_RELEASED,
          BM_PAD_RAW_HELD,
          BM_PAD_RAW_PRESSED,
          BM_PAD_RAW_RELEASED,
          BM_PAD_LATCHED}) {
        put<std::uint32_t>(pad(field), get<std::uint32_t>(pad(field)) & ~mask);
    }

    const int newest = (get<int16_t>(pad(BM_PAD_HIST_IDX)) + BM_PAD_HIST_LEN - 1) % BM_PAD_HIST_LEN;
    const gaddr slot = pad(BM_PAD_HIST_BTN + 4 * newest);
    put<std::uint32_t>(slot, get<std::uint32_t>(slot) & ~mask);

    // Their pressure, too.
    if (mask & BM_PAD_L2) {
        put<float>(pad(BM_PAD_ANALOG + 4 * 14), 0.0f);
    }

    // R2's.
    if (mask & BM_PAD_R2) {
        put<float>(pad(BM_PAD_ANALOG + 4 * 15), 0.0f);
    }
}

// rac1.player.advance_animation: the first hook of a tick, after the last one's transitions.
template <class Original>
void advance_animation(Original original) {
    undo_hero_steers();

    // rac2's clamp on idle after the run's exit, now that idle has copied its momentum.
    if (s.idle_clamp) {
        s.idle_clamp = false;

        // Only if the exit happened.
        if (state() == RAC1_STATE_IDLE
            && get<int32_t>(hero(BM_HERO_PREV_STATE)) == RAC1_STATE_RUN) {
            const float clamp = bm_exit_clamp(&s.tick);
            float m[3];

            // The momentum's length, at most 1.5 units a second.
            for (int i = 0; i < 3; ++i) {
                m[i] = get<float>(hero(BM_HERO_MOMENTUM + 4 * i));
            }

            const float len = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);

            // Longer than the clamp: scaled down to it.
            if (len > clamp && std::isfinite(len)) {
                // Each axis by the same share.
                for (int i = 0; i < 3; ++i) {
                    put<float>(hero(BM_HERO_MOMENTUM + 4 * i), m[i] * clamp / len);
                }
            }

            // The ground speed too.
            if (get<float>(hero(BM_HERO_GROUND_SPEED)) > clamp) {
                put<float>(hero(BM_HERO_GROUND_SPEED), clamp);
            }
        }
    }

    // The backpedal: the walk or run stepped back, then the original with no step of its own.
    if (s.strafing && is_run(state()) && bm_backpedal(s.sector, get<float>(hero(BM_HERO_STICK)))) {
        const gaddr moby = get<std::uint32_t>(hero(BM_HERO_MOBY));
        const int seq = valid(moby) ? get<std::uint8_t>(moby + BM_MOBY_SEQ) : -1;

        // A looping walk or run, nothing pending.
        if ((seq == BM_SEQ_WALK || seq == BM_SEQ_RUN)
            && get<std::uint8_t>(moby + BM_MOBY_PREV_SEQ) == seq
            && get<int32_t>(hero(BM_HERO_ANIM_BLEND_CURVE)) < 0
            && get<int32_t>(hero(BM_HERO_ANIM_QUEUED)) == 0
            && get<int32_t>(hero(BM_HERO_ANIM_LOOP_FRAME)) == -1) {
            const gaddr klass = get<std::uint32_t>(moby + BM_MOBY_CLASS);
            const gaddr record =
                valid(klass) ? get<std::uint32_t>(klass + BM_CLASS_SEQS + 4 * seq) : 0;
            const int count = valid(record) ? get<std::uint8_t>(record + BM_SEQ_FRAME_COUNT) : 0;
            bool keys = true;
            const int frame = get<std::uint8_t>(moby + BM_MOBY_FRAME);

            // A sequence of keys, the hero within it.
            if (count > 0 && frame < count) {
                float rates[256];

                // Each key's rate: the first float of its frame data (a bad key: no backpedal).
                for (int i = 0; i < count; ++i) {
                    const gaddr key = get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * i);
                    keys = keys && valid(key);
                    rates[i] = keys ? get<float>(key) : 0.0f;
                }

                // Every key's data where it should be.
                if (!keys) {
                    original();
                    return;
                }

                bm_anim a{
                    get<float>(moby + BM_MOBY_ANIM_TIME),
                    frame,
                    get<std::uint8_t>(moby + BM_MOBY_NEXT)
                };
                const int from = bm_anim_place(&a);
                const int moved =
                    bm_anim_step_back(&a, get<float>(hero(BM_HERO_ANIM_SPEED)), rates, count);
                const bool wrapped = moved >= count || (moved > 0 && a.frame > frame);

                // The original snaps a time over 0.99 to the next key: stay under it.
                if (a.time > 0.99f) {
                    a.time = 0.99f;
                }

                const int to = bm_anim_place(&a);
                put<std::uint8_t>(moby + BM_MOBY_FRAME, static_cast<std::uint8_t>(a.frame));
                put<std::uint8_t>(moby + BM_MOBY_NEXT, static_cast<std::uint8_t>(a.next));
                put<float>(moby + BM_MOBY_ANIM_TIME, a.time);
                put<std::uint32_t>(
                    moby + BM_MOBY_FRAME_DATA,
                    get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * a.frame)
                );
                put<std::uint32_t>(
                    moby + BM_MOBY_NEXT_DATA,
                    get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * a.next)
                );
                put<float>(hero(BM_HERO_ANIM_RATE), rates[a.frame]);

                const int triggers = get<std::uint8_t>(record + BM_SEQ_TRIGGER_COUNT);

                // The first sound trigger gone over, as the original fires the first one crossed.
                for (int i = 0; i < triggers && s_play_sound != nullptr; ++i) {
                    const std::uint32_t trig =
                        get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * (count + i));

                    // Crossed going back.
                    if (bm_anim_crossed_back(from, to, static_cast<int32_t>(trig) >> 16, wrapped)) {
                        s_play_sound(static_cast<int32_t>(trig & 0xFFFF), 0, moby);
                        break;
                    }
                }

                const float speed = get<float>(hero(BM_HERO_ANIM_SPEED));
                put<float>(hero(BM_HERO_ANIM_SPEED), 0.0f);
                original();
                put<float>(hero(BM_HERO_ANIM_SPEED), speed);
                return;
            }
        }
    }

    original();
}

// rac1.player.update_motion_input (hero_update): the tick's context before, the steers after.
template <class Original>
void update(Original original) {
    undo_hero_steers();
    s.decided.clear();

    // Out of play or off his feet: the game's own, nothing kept.
    if (!playing()) {
        s.active = false;
        s.strafing = false;
        s.flipping = false;
        s.flip_steered = false;
        s.last_state = -1;
        original();
        return;
    }

    s.active = true;
    s.tick = read_tick();

    const int st = state();
    const bool new_state = st != s.last_state || get<int32_t>(hero(BM_HERO_STATE_TIMER)) == 0;
    s.last_state = st;
    s.strafing = strafe_now();
    s.sector =
        s.strafing
            ? bm_stick_sector(get<float>(hero(BM_HERO_STICK_X)), get<float>(hero(BM_HERO_STICK_Y)))
            : BM_FORWARD;

    // A flip the strafe started: rac2's drift from its first tick.
    if (s.flip_steered) {
        s.flip_steered = false;
        s.flipping = st == RAC1_STATE_HIGH_JUMP && new_state;

        // Its start: 5 units a second.
        if (s.flipping) {
            put<float>(hero(BM_HERO_DRIFT_SPEED), bm_flip_drift_start(&s.tick));
        }
    } else if (st != RAC1_STATE_HIGH_JUMP) {
        s.flipping = false;
    }

    // Each tick of it: the drift left so rac1's easing lands on rac2's; no push along the takeoff.
    if (s.flipping) {
        const bool to_zero = get<int16_t>(hero(BM_HERO_PAST_APEX)) != 0
                             && get<int32_t>(hero(BM_HERO_FRAMES_TO_LAND))
                                    < bm_scale_ticks(RAC1_FLIP_DRIFT_LANDING_TICKS, &s.tick);
        const float drift = get<float>(hero(BM_HERO_DRIFT_SPEED));
        put<float>(
            hero(BM_HERO_DRIFT_SPEED),
            bm_flip_drift_preset(drift, get<float>(hero(BM_HERO_STICK)), to_zero ? 1 : 0, &s.tick)
        );
        put<float>(hero(BM_HERO_AIR_TARGET_SPEED), 0.0f);
    }

    // A jump's first tick: rac2's air control.
    if (s_set.air && new_state && bm_is_jump_state(st)) {
        const gaddr speed = hero(BM_HERO_AIR_STICK_SPEED);
        put<float>(speed, bm_air_stick_speed(st, get<float>(speed), &s.tick));
    }

    // The run: rac2's flags from last tick's yaw error; the turnaround is rac1's unk3BE too.
    if (s_set.ground && is_run(st)) {
        bm_run_in in = run_in();
        bm_run_flags_update(&s.flags, &in, &s.tick);
        put<int16_t>(hero(BM_HERO_TURNAROUND), static_cast<int16_t>(s.flags.turnaround));
    } else if (!is_run(st)) {
        s.flags = bm_run_flags{0, 0};
    }

    s.k = 1.0f;
    s.in_update = true;
    original();
    s.in_update = false;

    const int after = state();

    // Still in play after the update (a fall out of the world ends it).
    if (!playing()) {
        return;
    }

    // The twist, for the easing that runs after the transitions.
    if (s_set.twist && s.strafing
        && (is_run(after) || bm_is_jump_state(after) || after == RAC1_STATE_FALL)
        && bm_twist_on(
            get<float>(hero(BM_HERO_STICK)),
            after,
            bm_backpedal(s.sector, get<float>(hero(BM_HERO_STICK)))
        )) {
        twist();
    }

    // rac2's flip moves at his flat speed.
    if (s.flipping && after == RAC1_STATE_HIGH_JUMP) {
        put<float>(hero(BM_HERO_GROUND_SPEED), get<float>(hero(BM_HERO_FLAT_SPEED)));
    }

    steer_transitions(after, steer_wrench(after));
    trace_tick(after);
}

// rac1.player.turn_to_heading: rac2's turns in the run and the strafe's facing.
template <class Original>
void turn(Original original, int32_t mode, float accel, float damp, float limit) {
    // Outside the hero's update, out of play: the call as it came.
    if (!s.in_update || !s.active) {
        original(mode, accel, damp, limit);
        return;
    }

    const int st = state();

    // The wrench's lock-on turn (the swings, the slam, the throw): rac2's spring.
    if (s_set.wrench
        && (st == RAC1_STATE_WRENCH_COMBO || st == RAC1_STATE_WRENCH_SLAM
            || st == RAC1_STATE_CROUCH_SWING)) {
        bm_spring out{};

        // rac1's own lock-on turn, recognised by its numbers.
        if (bm_wrench_turn(st, bm_spring{accel, damp, limit}, &s.tick, &out)) {
            original(mode, out.accel, out.damp, out.limit);
            return;
        }

        original(mode, accel, damp, limit);
        return;
    }

    // The run.
    if (is_run(st)) {
        // Strafing: rac2's facing to the camera on its own yaw rate, then the side reversal.
        if (s.strafing) {
            const float move = get<float>(hero(BM_HERO_MOVE_YAW));
            s.strafe_yaw = move;
            put<float>(hero(BM_HERO_YAW_RATE), s.strafe_rate);
            put<float>(hero(BM_HERO_MOVE_YAW), camera_yaw());
            original(
                mode,
                rac2_spring(RAC2_STRAFE_TURN_ACCEL, &s.tick),
                rac2_spring(RAC2_STRAFE_TURN_DAMP, &s.tick),
                rac2_turn_limit(RAC2_STRAFE_TURN_LIMIT, &s.tick)
            );
            s.strafe_rate = get<float>(hero(BM_HERO_YAW_RATE));
            put<float>(hero(BM_HERO_YAW_RATE), 0.0f);
            put<float>(hero(BM_HERO_YAW_ERROR), 0.0f);
            put<float>(hero(BM_HERO_MOVE_YAW), move);

            const float f = bm_side_factor(
                &s.sides, s.sector, get<std::uint8_t>(hero(BM_HERO_VEHICLE_MODE)) == 0
            );
            put<float>(hero(BM_HERO_GROUND_SPEED), get<float>(hero(BM_HERO_GROUND_SPEED)) * f);
            return;
        }

        // Not strafing: rac2's turn, and k from the yaw error before it.
        if (s_set.ground) {
            const bm_run_in in = run_in();
            const bm_turn t = bm_run_turn(&in, &s.flags, &s.tick);
            s.k = bm_run_k(&in, &s.flags);
            original(mode, t.accel, t.damp, t.limit);
            return;
        }
    }

    // In the air after takeoff (the jump group's air turn, the fall), strafing: face the camera, the
    // steering still along the stick. The flips turn on their own (takeoff yaw), as in rac2.
    if (s.strafing
        && (st == RAC1_STATE_FALL
            || (bm_is_jump_state(st) && st != RAC1_STATE_HIGH_JUMP && st != 0x0C
                && st != RAC1_STATE_PACK_LONG_JUMP && st != 0x11 && st != 0x1C && st != 0x4C
                && get<int32_t>(hero(BM_HERO_STATE_TIMER))
                       >= get<int32_t>(hero(BM_HERO_TAKEOFF_FRAME))))) {
        const float move = get<float>(hero(BM_HERO_MOVE_YAW));
        s.strafe_yaw = move;
        put<float>(hero(BM_HERO_MOVE_YAW), camera_yaw());
        original(mode, accel, damp, limit);
        put<float>(hero(BM_HERO_MOVE_YAW), move);
        return;
    }

    original(mode, accel, damp, limit);
}

// rac1.player.approach_speed: rac2's target, acceleration and braking; the slam's drive.
template <class Original>
void approach_speed(Original original, float up, float down) {
    // Outside the hero's update or out of play: the call as it came.
    if (!s.in_update || !s.active) {
        original(up, down);
        return;
    }

    const int st = state();

    // The slam off ice, high up: rac2 drives on at 4.5 units a second (rac1 only toward a target).
    if (s_set.wrench && st == RAC1_STATE_WRENCH_SLAM && get<std::uint8_t>(hero(BM_HERO_ON_ICE)) == 0
        && bm_slam_drive(
            get<float>(hero(BM_HERO_GROUND_DISTANCE)),
            get<std::uint32_t>(hero(BM_HERO_MELEE_TARGET)) != 0,
            get<int32_t>(hero(BM_HERO_STATE_TIMER)),
            &s.tick
        )) {
        const gaddr target = hero(BM_HERO_TARGET_SPEED);
        const float came = get<float>(target);
        const float want = bm_slam_drive_speed(&s.tick);

        // Only ever more than rac1 asked for.
        if (came < want) {
            put<float>(target, want);
            s.decided += " slam-drive";
        }

        original(up, down);

        // Give rac1 its own target back.
        if (came < want && get<float>(target) == want) {
            put<float>(target, came);
        }

        return;
    }

    // The setting off: the call as it came.
    if (!s_set.ground) {
        original(up, down);
        return;
    }

    // The run: rac2's speed, scaled as another mod scaled rac1's (read from `down`).
    if (is_run(st)) {
        // A push (a conveyor) adds to rac1's own target: rac1's numbers then.
        if (get<float>(hero(BM_HERO_PUSH)) > 0.0f) {
            original(up, down);
            return;
        }

        const float own = bm_rac1_run_down(get<int16_t>(hero(BM_HERO_TURNAROUND)) != 0, &s.tick);
        float scale = own > 0.0f ? down / own : 1.0f;

        // A scale that is no number, or negative, is none.
        if (!std::isfinite(scale) || scale < 0.0f) {
            scale = 1.0f;
        }

        bm_run_in in = run_in();

        // Strafing, rac2's yaw error is 0 here and k is 1.
        if (s.strafing) {
            in.yaw_error = 0.0f;
        }

        const bm_speed sp = bm_run_speed(&in, &s.flags, s.strafing ? 1.0f : s.k, &s.tick);
        const gaddr target = hero(BM_HERO_TARGET_SPEED);
        const float came = get<float>(target);
        const float want = sp.target * scale;
        put<float>(target, want);
        original(sp.up * scale, sp.down * scale);

        // Give rac1 its own target back, so nothing else reads rac2's (as the difficulty mod does).
        if (get<float>(target) == want) {
            put<float>(target, came);
        }

        return;
    }

    // The idle group, STOP and the crouch: rac2's braking.
    original(up, down * bm_brake_scale(st, get<std::uint8_t>(hero(BM_HERO_ON_ICE)) != 0));
}

// rac1.player.set_planar_velocity: along the stick where rac2 moves him along it.
template <class Original>
void planar_velocity(Original original, float angle) {
    // Outside the hero's update or out of play: the call as it came.
    if (!s.in_update || !s.active) {
        original(angle);
        return;
    }

    const int st = state();

    // The run.
    if (is_run(st)) {
        // Strafing: along the stick.
        if (s.strafing) {
            original(s.strafe_yaw);
            return;
        }

        // rac2's flag: along the move yaw while slow and off; along the facing otherwise.
        if (s_set.ground && get<int16_t>(hero(BM_HERO_UNK308)) == 0) {
            original(s.flags.along ? get<float>(hero(BM_HERO_MOVE_YAW)) : 99999.0f);
            return;
        }
    }

    // A jump before takeoff while strafing: along the last strafe yaw, not the facing.
    if (s.strafing && bm_is_jump_state(st) && angle > BM_PI
        && get<int32_t>(hero(BM_HERO_STATE_TIMER)) < get<int32_t>(hero(BM_HERO_TAKEOFF_FRAME))) {
        original(s.strafe_yaw);
        return;
    }

    original(angle);
}

// rac1's own strafe camera while strafing, after the overrides; the overrides alone otherwise.
template <class Original>
void strafe_camera(Original original, gaddr cam) {
    // Not strafing, or the calls are not there: the game's camera.
    if (!s.strafing || !s.active || s_orbit_clamp == nullptr || s_start_slide == nullptr
        || s_lateral_spring == nullptr) {
        original(cam);
        return;
    }

    const gaddr weight = level0(s_disc->blend_weight);
    const float before = get<float>(weight);
    original(cam);
    s_orbit_clamp(0);
    s_start_slide();
    s_lateral_spring(0.04f, 0.2f);

    const gaddr vars = get<std::uint32_t>(cam + BM_CAMERA_VARS);

    // The recentre timers, as state 0x81 clears them, and the blend weight the recentring wrote.
    if (valid(vars)) {
        put<int16_t>(vars + 0x12, 0);
        put<int16_t>(vars + 0x14, 0);
        put<int16_t>(vars + 0x16, 0);
    }

    put<float>(weight, before);
}

// rac1.camera.follow.apply_overrides: while strafing, rac1's own strafe camera (its state 0x81's);
// then the turn's speed for the rest of the camera's update.
template <class Original>
void camera(Original original, gaddr cam) {
    undo_camera_steers();
    strafe_camera(original, cam);

    // The camera setting: the first game's takes nothing.
    if (s_set.camera != BM_CAMERA_RAC1) {
        camera_speed(cam);
    }
}

// A setting's value as the manifest allows it; the default for anything else.
std::uint32_t strafe_buttons(const std::string& v) {
    // L2 only.
    if (v == "l2") {
        return BM_PAD_L2;
    }

    // R2 only.
    if (v == "r2") {
        return BM_PAD_R2;
    }

    // None.
    if (v == "off") {
        return 0;
    }

    return BM_PAD_L2 | BM_PAD_R2;
}

// The camera setting's value as the manifest allows it (camera.h's BM_CAMERA_*); Going Commando's,
// the default, for anything else.
int camera_choice(const std::string& c) {
    // The first game's own.
    if (c == "rac1") {
        return BM_CAMERA_RAC1;
    }

    // Modern.
    if (c == "modern") {
        return BM_CAMERA_MODERN;
    }

    return BM_CAMERA_RAC2;
}

}  // namespace

void load(openrac::Mod& mod) {
    s_mod = &mod;

    // 1. The disc: its row, or a refusal. Never a guessed address.
    s_disc = nullptr;

    // Each disc the mod has addresses for.
    for (const bm_disc& d : k_bm_discs) {
        // The disc the port runs.
        if (mod.version() == d.version) {
            s_disc = &d;
        }
    }

    // A disc without a row.
    if (s_disc == nullptr) {
        throw openrac::Error(
            "rac1-enhanced-movement knows the PAL v2.00 and NTSC-U v1.00 discs, not "
            + std::string(mod.version())
        );
    }

    // 2. The settings, once.
    s_set = Settings{};
    s_set.ground = mod.setting<bool>("ground");
    s_set.air = mod.setting<bool>("air");
    s_set.strafe = strafe_buttons(mod.setting<std::string>("strafe"));
    s_set.flips = mod.setting<bool>("flips");
    s_set.twist = mod.setting<bool>("twist");
    s_set.wrench = mod.setting<bool>("wrench");
    s_set.camera = camera_choice(mod.setting<std::string>("camera"));
    const char* env = std::getenv("OPENRAC_ENHANCED_MOVEMENT_TRACE");
    s_set.trace = mod.setting<bool>("trace") || (env != nullptr && std::strcmp(env, "1") == 0);

    // 3. Every piece of state the hooks keep.
    s = State{};

    const bool strafe = s_set.strafe != 0;
    const bool movement = s_set.ground || s_set.air || strafe || s_set.wrench;
    const bool camera_turn = s_set.camera != BM_CAMERA_RAC1;

    // Everything off, the camera the first game's: no hooks at all, the game as it is.
    if (!movement && !camera_turn) {
        mod.info("everything off: no hooks taken");
        return;
    }

    // 4. The game functions: the strafe camera's calls, and the sound the backpedal plays.
    if (strafe) {
        s_orbit_clamp = mod.required_function<OrbitClampFn>(s_disc->orbit_clamp);
        s_start_slide = mod.required_function<SlideFn>(s_disc->start_slide);
        s_lateral_spring = mod.required_function<SpringFn>(s_disc->lateral_spring);
        s_play_sound = mod.function<PlaySoundFn>(s_disc->play_sound);
    }

    // 5. The hook points. Every one runs the game's code (or the next mod's) when it has nothing to do.
    // The hero's update and animation, for every part but the camera.
    if (movement) {
        mod.hook<hp::Hero_UpdateMotionInput>([](auto original) { update(original); });
        mod.hook<hp::Hero_AdvanceAnimation>([](auto original) { advance_animation(original); });
    }

    // The run's turns and velocity, and the strafe's; the wrench's lock-on turn.
    if (s_set.ground || strafe || s_set.wrench) {
        mod.hook<hp::Hero_TurnToHeading>([](auto original, int32_t m, float a, float d, float l) {
            turn(original, m, a, d, l);
        });
    }

    // The velocity along the stick.
    if (s_set.ground || strafe) {
        mod.hook<hp::Hero_SetPlanarVelocity>([](auto original, float angle) {
            planar_velocity(original, angle);
        });
    }

    std::string missing;

    // The speeds and the slam's drive: a hook point of the 1.x line, so a port may lack it.
    if (s_set.ground || s_set.wrench) {
        // Taken, or refused by a port without it: the rest of the mod still loads.
        try {
            mod.hook<hp::Hero_ApproachSpeed>([](auto original, float up, float down) {
                approach_speed(original, up, down);
            });
        } catch (const openrac::Error& e) {
            missing = std::string(" (without Going Commando's speeds: ") + e.what() + ")";
        }
    }

    // The pad: the strafe's buttons, the presses the flips and the wrench steer, and the camera's
    // words put back before each tick.
    if (strafe || s_set.wrench || camera_turn) {
        mod.hook<hp::Pad_ProcessReport>([](auto original, gaddr p, gaddr report, int32_t size) {
            process_report(original, p, report, size);
        });
    }

    // The strafe's camera, and the camera's turn.
    if (strafe || camera_turn) {
        mod.hook<hp::FollowCamera_ApplyOverrides>([](auto original, gaddr cam) {
            camera(original, cam);
        });
    }

    // 6. One line saying how it will play.
    const char* buttons = s_set.strafe == (BM_PAD_L2 | BM_PAD_R2) ? "L2 or R2"
                          : s_set.strafe == BM_PAD_L2             ? "L2"
                                                                  : "R2";
    mod.info(
        std::string("on the ") + std::string(mod.version()) + " disc: running "
        + (s_set.ground ? "Going Commando's" : "the game's") + ", air control "
        + (s_set.air ? "Going Commando's" : "the game's") + ", strafe "
        + (strafe ? std::string("on ") + buttons + (s_set.flips ? ", flips" : "")
                        + (s_set.twist ? ", twist" : "")
                  : "off")
        + ", wrench " + (s_set.wrench ? "Going Commando's" : "the game's") + ", camera "
        + (s_set.camera == BM_CAMERA_RAC2     ? "Going Commando's"
           : s_set.camera == BM_CAMERA_MODERN ? "modern"
                                              : "the game's")
        + (s_set.trace ? ", trace on" : "") + missing
    );

    // The missing speeds are a warning too.
    if (!missing.empty()) {
        mod.warn(
            "this OpenRAC has no rac1.player.approach_speed: Ratchet keeps the first game's speeds "
            "(and the slam's drive)"
        );
    }
}

}  // namespace enhanced_movement::rac1
