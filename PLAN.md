# rac1-enhanced-movement: the plan

A native OpenRAC mod that gives Ratchet in Ratchet & Clank (rac1) the movement of Ratchet & Clank:
Going Commando (rac2), built as `openrac-mods/docs/GUIDE.md` describes. This file is the plan
for carrying it out: what was decided, what the mod changes, how it is built, and the order
of the work. Research done so far is summarised here. Everything still open is listed in
Phase 1, and the finished evidence goes to `DESIGN.md`.

## Status (2026-11-14)

| Phase | State |
|---|---|
| 0. Repository | Done: the SDK template, SDK 1.14.4, builds and packages. openrac-mods' `cpp_style.py` is copied to `tools/` (it only covers its own repository's folders) |
| 1. Research | Done: DESIGN.md; `tests/data/rac2_constants.tsv` (68 numbers read out of rac2-decomp, the run speed from the v2.00 disc's level 0 data). Two corrections to section 3.1 below are marked there. The state changes are steered, not called (section 4) |
| 2. Decisions and unit tests | Done: `ground.h`, `air.h`, `strafe.h`; `ground`, `air`, `strafe`, `rac2_constants` tests |
| 3. Game layer and load test | Done: `game.cpp`, `load`, `manifest` tests |
| 4. Build and checks | Done: 6 of 6 tests, style clean, `package_mod.py --check` ok, README |
| 5. Play-testing on NTSC-U | Done by script in OpenRAC's port on the NTSC-U disc data (PLAYTEST.md, `tools/playtest.sh`): Veldin, Novalis and Kerwan against the game without the mod, every setting off identical to no mod, jump heights and L1/R1 unchanged, the heli-pack, the hip twist's joints confirmed in pictures, a smoke run of every level. Left for a person: the places the routes do not reach (PLAYTEST.md, last section) |
| 6. Release | Done on your go-ahead (commit, push, tag; no index listing): github.com/kmomoney/rac1-better-movement (since renamed), tag `rac1-better-movement-v0.1.0`. CI built and tested every platform (Linux x86-64 and AArch64, Windows, macOS arm64 and x86-64, Android) and published the release with `SHA256SUMS`. Listing it in the openrac-mods index (`index/rac1-enhanced-movement.json`) is left for when you want it |
| 7. PAL v2.00 (0.2.0) | Done on your request: the PAL row (DESIGN.md section 6, section 5 before 0.3.0), rac1's tick windows counted as `scale_ticks` counts them, the load test on both discs and at PAL's 50 Hz, the play-test on the PAL disc (PLAYTEST.md); released as 0.2.0 (tag `rac1-better-movement-v0.2.0`) |
| 8. Renamed (0.2.1) | On your request: the repository, folder, id, name, library and tags are `rac1-enhanced-movement` (github.com/kmomoney/rac1-enhanced-movement; GitHub redirects the old address). The 0.1.0 and 0.2.0 releases keep their old names. The manifest conflicts with the old id, so an old copy must be removed before the new one runs |
| 9. Wrench (0.3.0) | Done on your request, released as 0.3.0 (tag `rac1-enhanced-movement-v0.3.0`): Going Commando's wrench timing (section 9 below, DESIGN.md section 5), the `wrench` setting, `wrench.h` and its test, the load test on both discs, 34 more rows in `rac2_constants.tsv` (the attack records read from the v2.00 disc's level 0 data), the play-test against Going Commando itself in OpenRAC's rac2 port and on both rac1 discs (PLAYTEST.md). The movement route plays tick for tick as 0.2.1 did |
| 10. Camera (0.3.0) | Done on your request, released as 0.3.0 (tag `rac1-enhanced-movement-v0.3.0`): the `camera` setting (`rac1`, `rac2`, `modern`; section 10 below, DESIGN.md section 8), `camera.h` and its test, the load test on both discs and at 50 Hz, 10 more rows in `rac2_constants.tsv` (read from the v2.00 disc's level 0 data), the play-test on both rac1 discs against Going Commando in OpenRAC's rac2 port (PLAYTEST.md). `rac2` is Going Commando's fastest turn on every step, your choice (section 10). With `rac1` every route plays tick for tick as before, and with `rac2` until the right stick turns the camera. 0.3.0 is the wrench and the camera together. (`modern` was removed in 0.3.1: row 11) |
| 11. `modern` removed (0.3.1) | Done on your request, released within 0.4.0 (tag `rac1-enhanced-movement-v0.4.0`; no 0.3.1 tag): you play-tested 0.3.0 and found `modern` far too fast, even on the game's slowest camera speed (it is three times the game's: 180 / 234 / 288 degrees a second). The choice, its code (the ×3 gain, the pitch's steps, the eye's springs and their 20-tick window), its addresses, tests and docs are gone; `rac1` and `rac2` are unchanged (section 10). A saved `modern` gives the default `rac2`: the launcher drops it on installing 0.3.1, the port hands the default for it, and `camera_choice()` maps it to `rac2` (the load test feeds it). Play-tested on both discs against the 0.3.0 release (PLAYTEST.md) |
| 12. Clank (0.4.0) | Done on your request, released as 0.4.0 (tag `rac1-enhanced-movement-v0.4.0`; top speed 3.45, your choice over 3.13): the `clank` setting (`rac1`, `rac2`, `enhanced`, the default; section 11 below, DESIGN.md section 9), `clank.h`, every `enhanced` number in `clank_tune.h`, the `clank` test, the load test on both discs and at 50 Hz, 48 more rows in `rac2_constants.tsv` (Clank's literals and his punch records), the play-test on both rac1 discs and in Going Commando's port, whose Aranos Prison starts as Clank (PLAYTEST.md). `rac1` plays Clank and every Ratchet route tick for tick as 0.3.1. Giant Clank stays the game's (section 11) |
| 13. Language audit (0.4.1) | Done on your request, released as 0.4.1 (tag `rac1-enhanced-movement-v0.4.1`): plain player text in the README, the manifest and the log messages; code comments in OpenRAC's style, with about 25 wrong ones fixed. The comment changes compile to 0.4.0's machine code, and the standard, wrench, camera and Clank routes play tick for tick as 0.4.0 on NTSC-U and PAL |

## 1. Decisions

| | Decided |
|---|---|
| Scope | Ground movement, jumps and air control, rac2's L2/R2 strafe with its flips, swimming, heli/thruster pack |
| Jump heights | rac1's, for every jump, the pack jumps included. rac1's levels are built around them. rac2's plain jump is lower (1.28 tapped / 2.3 held against 1.47 / 2.62) |
| Hook points | The SDK's existing ones only. No OpenRAC change |
| Discs | NTSC-U (`ntsc`, SCUS-97199) only, the disc that could be play-tested. **0.2.0 adds PAL v2.00 (`pal`, SCES-50916)**, once you had its disc, to match the other rac1 mods (`pal` and `ntsc`) |
| Strafe | Written for this mod from the decompilations. No code or tests are taken from `lock-strafe`. Both mods take L2/R2 and the same hook points, so the manifest declares `"conflicts": ["lock-strafe"]` |
| Repository | `~/Projects/rac1-enhanced-movement`, its own git repository, started from OpenRAC's `sdk/template`. Nothing is pushed or tagged without asking |
| Id | `rac1-enhanced-movement`, version `0.1.0` |
| Multiplayer | `"input"`: it changes how one player's Ratchet moves, not the world or its rules |
| Wrench (0.3.0) | Part of Ratchet's movement: rac2's timing, windows, way out of a swing and lock-on turn. **Damage and hit windows stay rac1's**, for the same reason as the jump heights: rac1's enemies and bosses are balanced around them. Where unclear, the choice that keeps rac1 as beatable (section 9) |
| Clank (0.4.0) | One `clank` choice: `rac1` (the game's: the mod stays out of him), `rac2` (Going Commando's Clank where it differs), `enhanced` (past it, the default). Moderate steps, each anchored to Going Commando's own step for Ratchet in the same quantity (section 11). **Jump heights, punch damage, hits and reach stay rac1's**, as for Ratchet; the Gadgebot commands are the game's; Giant Clank is left out |
| Camera (0.3.0) | One `camera` choice: `rac1` (the game's), `rac2` (Going Commando's fastest turn, 132 degrees a second on every step of the game's camera speed, the default like the mod's other Going Commando parts). Turn speed only; the follow camera's other behaviour stays rac1's (section 10). **0.3.1 removes 0.3.0's third choice, `modern`** (three times the game's speed): played, it was far too fast even on the game's slowest step. `rac2` is the fastest choice; a saved `modern` gives `rac2` |

What the research found about two of the scope items:

- **Swimming**, underwater and on the surface (rac1 states 0x33–0x37, rac2 0x32–0x36), is the same
  code with the same numbers in both games. There is nothing to port. The README says so.
- **Packs.** With rac1's heights kept, the heli and thruster jumps (0xA, 0xD, 0xF, 0x10) have the
  same numbers apart from height. Two differences remain, and both sit where no hook point reaches:
  - rac2 eases into the hover's descent (state 8, which the decomp misnames `HERO_STATE_LAND`).
  - The heli double-jump glide uses thresholds of 2.8/2.2 instead of 2.7/2.1, plus a check for room overhead (inside `hero_integrate_velocity`).

  Both go in the README's known limits. The mod still gives every pack jump rac2's default air
  control where rac2 differs.

## 2. Sources and conventions

- **rac2**: `~/Projects/rac2-decomp` (USA v1.01). The hero code is level 0's shared copy in
  `src/levels/shared/`:
  - the state machine `func_L00_002BA7D8`
  - the transitions `func_L00_002CB3A8` (`-decider.c`)
  - state entry `func_L00_002C42E0` (`src/levels/00_aranos_tutorial/`)
  - stick and turn helpers in `L00_002AFBC0*.c`, `L00_002B3D10.c`, `L00_002B3F68.c`, `L00_002B35A0.c`
  - airborne velocity `func_L00_002B6560` / `func_L00_002B6BC8`
  - the joint pose `func_L00_002B7B80`
  - the follow camera `func_L00_004224D0`

  rac2 writes its numbers for 60 Hz: a per-tick speed × 60 is units per second, and a per-tick²
  acceleration × 3600 is units per second². Turn limits are rad/tick × 60.
- **rac1**: `~/Projects/rac1-decomp` (PAL names). The state machine is `hero_update_movement`
  (`func_L00_002180F8`, `src/overlays/shared/hero_water_and_ground_motion.c`). Transitions are in
  `hero_item_input_and_hud.c` / `hero_high_jump_input.c`, state entry is `HeroEnterMoveState`
  (`func_L00_00222B80`), and the hero struct is `include/structs/hero.h`.
- **Scaling in rac1**: speeds × `g_frame_time`, accelerations × `g_frame_time_sq`, turn
  acceleration and damping × `g_frame_scale_sq`, turn limits × `g_frame_time`. The mod keeps every
  rac2 number as a per-second value with its source and scales it the same way, so PAL's 50 Hz
  would come out right if PAL is ever added.
- **NTSC-U names**:
  - `rac1-decomp/config/overlays/us_map.tsv` maps each PAL function to its US address. The
    port's NTSC build calls it `FUN_L00_<address in lower case>` (for example `HeroEnterMoveState` is
    `FUN_L00_002223f8`).
  - Data addresses come from `OpenRAC/port/game/rac1-ntsc/host/rac1_version.h` and
    `rac1-decomp/config/names_ntsc.tsv`. The hook points' NTSC rows are in
    `OpenRAC/port/game/rac1-ntsc/libraries.tsv`.
  - Struct offsets are the same on both discs.
- **SDK**: the release `sdk.lock` pins (1.14.4, as openrac-mods uses), or `-DOPENRAC_SDK=../OpenRAC/sdk`
  while developing.

## 3. What the mod changes

Values are rac1 → rac2, in units per second (u/s, u/s², rad/s) unless marked. "Accel / damping" are
the turn spring's per-tick coefficients at 60 Hz.

### 3.1 Ground (setting `ground`)

| | rac1 | rac2 (source) |
|---|---|---|
| Target speed in the run | Walk or run, from `hero_speed_steps` (run 5.7) | `5.95 × stick` (`func_L00_002B3D10`, `D_001A7DB0[4]`). Wading: × 0.8, at least 2.5 (both games) |
| Speeding up / slowing to target | 7.5·k / 8.5 | 18·k / 20 (`0.005`, `0.0055556`) |
| k: no acceleration while turning | 0 when stick < 0.8 and yaw error > 30° | 0 when (stick < 0.9, or item-locked without turnaround) and \|yaw error\| > 45° |
| Slow stick, wide angle | — | target × max(0, 1 − 0.4·\|err\|) when stick < 0.75 and \|err\| > 35° |
| Run turn, full stick | 0.019 / 0.1 / 9.95 | 0.022 / 0.27 / 5.585 (`func_L00_002B3F68`) |
| Run turn, partial stick | accel 0.019·s, limit 9.95·s, s = (stick+1)/2; below the threshold 0.005·(stick+0.35) / 0.1 / 4.71·(stick+0.35) | accel stick·0.02 in [0.01, 0.022], damping 0.27, limit stick·3.84 in [1.92, 5.585] |
| Turn in the run band (`state.step` 1; the full-stick row above is the walk band's) | 0.008 / 0.15 / 9.95 | 0.02 / 0.17 / 9.95 |
| Turnaround turn | accel ×1.1, limit ×1.2 | 0.035 / 0.27 / 11.69 |
| Turnaround trigger | pad flick event (`pressed & 0x10000`) | \|err\| > 120° at full stick while slow (< 1.7), or the stick's yaw moving > 110° (`func_L00_0032F038`) with \|err\| > 70° at full stick. Cleared under 4° |
| Turnaround speed | target × clamp(2.618 − err, 0.2, 1), slowing 17 | the same, plus a cap of 5.95 − 1.75·\|err\| |
| Moving along the stick, not the facing | from < 0.3 at stick > 0.85, until within 3° | from < 1.7, until within 5° |
| Letting go of the stick (< 0.2) in the run | target 0 only when the stick is exactly centred | target 0 below 0.2; he slows along his facing as in rac1 (`+0x3F0`/`+0x408` hold the facing; only one rac2 item keeps the stick's direction). Corrected in Phase 1 |
| Run exit, stick < 0.17 | STOP (skid) between 2.7 and 4.4, else idle after 25 ticks | idle at once unless still turning, speed and velocity clamped to 1.5 (`func_L00_002CB3A8`) |
| Braking: idle group / STOP / crouch | 12.6 / 12·t / 6.48 | 17.7 / 27·t / 27 (ice crouch 1.7 in both) |
| Running jump (9) | rac1's rule (Phase 1) | only with stick > 0.85 and no strafe, else the plain jump (7) |

### 3.2 Jumps and air (setting `air`)

Gravity, air acceleration and braking, air turn (0.015 / 0.2 / 15), the running jump's push, the
high, long and heli/thruster jumps' numbers, hover speeds and the terminal speed (50) are the same in
both games. What changes, with rac1's heights kept:

| | rac1 | rac2 |
|---|---|---|
| Default air stick speed (every jump state not overriding it) | 5.7 | 5.95 (`c4A4`, common block of `func_L00_002C42E0`) |
| Running jump (9) | 5.7 | 5.95 |
| Double jump (0xE) | 3.5 | 4.75 |

These are entry parameters (`air_stick_speed`, `+0x424`). The mod writes them on the first tick a
state is seen, before its first physics tick (section 4).

### 3.3 Strafe (settings `strafe`, `flips`, `twist`)

rac2's strafe, from its own code:

- **Held**: L2 or R2 held strafes (rac2 sets its short at `+0x1454` while `pad & 3`, and counts it
  down otherwise). On foot, the game no longer sees L2/R2: L1 still looks (state 1) and R1 still
  crouches (state 4), as in rac2.
  - The mod takes the bits out in `Pad_ProcessReport` while in play and on foot.
  - It leaves them alone in rac1's own state 0x81, on the hoverboard (control mode 0x16), in
    vehicles, menus and scenes.
- **Stick sorted into sectors** (`func_L00_002B39F0`): left, right, forward and back, 45° each.
  Back with the stick past 0.15 is a backpedal.
- **Facing** (`func_L00_002B35A0`): the move yaw is the camera's for the turn, at
  0.037 / 0.25 / 12.2. The turn keeps its own yaw rate for the strafe. The stick's yaw is saved for
  the velocity.
- **Velocity**: along the stick's yaw, at the run's target speed.
- **Side reversal**, two 5-tick timers, one per side, on the ground only:
  - Going left while the right timer runs multiplies the speed by −0.25 each tick.
  - Going right while the left timer runs multiplies it by 0, 0.25, 0.5 or 0.75.
  - Phase 1 checks that rac1's velocity builder takes a negative speed the way rac2's does.
- **Standing**: idle with the strafe held turns him to the camera, through the walk state, once he
  is more than 10° off it (`func_L00_002B3268(0.1745)`). The run's exit waits for him to face it
  within 2°.
- **Jump**: X in the run gives the plain jump while strafing (rac2 never does the running jump then).
- **Flips** (`flips`, `func_L00_002C9FC8`): X in the jump window, strafing, stick > 0.9, the stick not
  forward of his facing (rac1's `stick_direction_vs_facing`). This enters state 0xB, the same state
  in both games, with the animation `0x1C + direction`. rac2 then drifts along the strafe yaw at
  5 u/s (`+0x4DC`, `+0x52D`).
- **In the air** (`func_L00_002B6BC8`): the turn uses the camera's yaw, and the velocity steers along
  the stick.
- **Hip twist** (`twist`, `func_L00_002B7B80`): with the stick past 0.15, not in state 0xB and not
  backpedalling, rac2 does three things:
  - writes pose-table record 0's yaw target (`+0x68`) as `wrap(stick yaw − facing)`, spring 0.037 / 0.27;
  - writes record 1's (`+0x118`) as the negative of that, spring 0.025 / 0.27;
  - skips the run's lean.

  rac1's pose table `D_L00_0017A780` has the same 0xB0-byte records. Both games' swim code writes
  record 4's spring at `+0x364`/`+0x368`. So the mod writes rac1's table the way rac2 does and the
  game's own easing applies it. That needs no `hero_set_joints` and works on every port.
- **Backpedal**: rac1 has no backpedal animation (its 0x14 is the run's stop). Within 45° of
  straight back, the mod plays the walk or run backwards from `Hero_AdvanceAnimation`.
- **Camera**: while strafing, the follow camera gets rac1's own strafe requests, the ones its
  state 0x81 makes in `follow_camera_apply_overrides`:
  - orbit clamp mode 0
  - start the slide
  - lateral spring 0.04 / 0.2
  - recentre timers zeroed

  rac2's `func_L00_004224D0` makes the same requests. None of this applies under a level's own
  camera; the test for one is worked out in Phase 1.

## 4. How it works

Each tick rac1 runs `advance_hero_animation`, then `hero_update`, then ground, ledge and rail
probes, then **the state transitions**, then easing and drawing (`update_hero_frame_pipeline`).
`hero_update` reads the stick, runs the state machine (each family of levels has its own copy),
and integrates.

Every copy calls the same shared helpers. The SDK's hook points sit on those helpers, so these
hooks reach every level:

| Hook point | Role |
|---|---|
| `rac1.player.update_motion_input` (`hero_update`) | **Before**: (1) set the tick's context: state, step, stick, strafe, and a flag saying the inner hooks are inside the hero's update; (2) on a state's first tick, write rac2's entry parameters (air stick speed); (3) set rac1's own run flags (move along the stick `+0x3B8`, turnaround `+0x3BE`) by rac2's rules. **After**, before rac1's transitions run: rac2's run exit (to idle with the 1.5 clamp, in place of the skid); the plain jump instead of the running jump while strafing; strafe flips; the twist's pose-table targets; clear the context |
| `rac1.player.turn_to_heading` | rac2's turn for the run, turnaround, first tick, strafe facing (move yaw swapped to the camera's around the call, as rac2 does) and the strafe in the air |
| `rac1.player.approach_speed` | rac2's target speed, written to `+0x190` before the call, and its acceleration and braking for the run, idle group, STOP and crouch; the side-reversal factor while strafing |
| `rac1.player.set_planar_velocity` | Along the stick: starting, after letting go (the last stick direction), and strafing |
| `rac1.pad.process_report` | Takes L2/R2 out for the strafe, and keeps them for it; puts back the press history the flips and the wrench steered, and the camera's words |
| `rac1.camera.follow.apply_overrides` | The strafe camera requests; the camera turn's speed for the rest of the camera's update (0.3.0, section 10) |
| `rac1.player.advance_animation` | The backpedal (walk or run played backwards); puts back the hero's steered fields |

The wrench (0.3.0, section 9) uses the same hook points: the post-step of `update_motion_input`
for its steers, `turn_to_heading` for the lock-on turn, `approach_speed` for the slam's drive.
So does Clank (0.4.0, section 11): `update_motion_input` gives him a tick of his own (his jump's
entry before, his steers after), and the run's helpers and the punch's lock-on turn are changed as
Ratchet's are.

Rules every hook follows:

- **Early return first.** Not in play, not on foot, in a vehicle or on the hoverboard, a setting
  off, or not inside the hero's update: call `original` unchanged.
- **Recognising rac1's calls.** A hook recognises which rac1 call it is in from the tick's
  context (state, step, flags), which the mod works out from the same rules rac1 uses.
  - Each numeric argument is replaced by `incoming × (rac2 value / the value rac1 would pass)`.
    With no other mod, that is rac2's value. If another mod scaled it (difficulty's Ratchet speed),
    its scaling survives, whatever the load order.
  - Arguments that match no rac1 call the mod knows pass through untouched, with a trace line.
- **Game memory.** Relocate every time. Never cache a relocated address or a game pointer across
  ticks. Null-check the hero's moby and the camera slot.
- **State.** All the mod's state (strafe timers, side timers, twist springs, last stick direction,
  the context) lives in one struct reset in `load`.
- **Calls into the game** go by NTSC function name through `function()`/`required_function()`:
  only functions that are one function in every level (the follow camera's requests,
  `Moby_PlaySound`).
- **State changes are steered, not called (changed in Phase 1).** `HeroEnterMoveState` and the
  idle picker have a different copy in each family of levels, and `function(name)` returns one
  compiled copy. So the mod never calls them. Instead, for the one tick, it sets what rac1's own
  transitions read (the state timer, the speed, the target speed, the stick amount; for a strafe
  flip R1 held and a stick press beside X), so the loaded level's own code makes the change.
  It puts each value back after the transitions: in `Hero_AdvanceAnimation`, the next tick's
  first hook, and for the pad in `Pad_ProcessReport` before the next poll.

## 5. Repository layout

```
rac1-enhanced-movement/
├── PLAN.md                this file
├── openrac-mod.json       the manifest
├── README.md              for players: what changes, each setting, known limits
├── DESIGN.md              for maintainers: rac2 against rac1, each row with its evidence
├── CMakeLists.txt         the SDK template's top, the mod, the tests
├── sdk.lock, tools/fetch_sdk.py, .github/workflows/build.yml, .gitignore, .gitattributes
│                          from OpenRAC/sdk/template
├── LICENSE                GPL-3.0-or-later
├── .clang-format          the openrac-mods one
├── src/
│   ├── mod.cpp            entry point: rac1 only, else a refusal
│   ├── version.h          id and version, shared by mod.cpp and the tests
│   └── rac1/
│       ├── discs.h        the NTSC-U row: addresses and function names, each with its source
│       ├── hero.h         offsets in the hero, pad, pose table and camera, each "seen in" or "assumed"
│       ├── rac2.h         rac2's numbers as per-second values, each with its rac2 source
│       ├── ground.h       run, stop, turn and exit decisions: plain C, no game memory
│       ├── air.h          jump-entry adjustments
│       ├── strafe.h       sectors, side timers, flip test, twist angles, backpedal
│       ├── wrench.h       the swings' steers, Square's windows, the lock-on turn (0.3.0)
│       ├── camera.h       the camera turn's gain (0.3.0)
│       ├── clank.h        Clank's run, jump, Square and punch decisions (0.4.0)
│       ├── clank_tune.h   every number `enhanced` plays Clank with, to adjust after playing
│       ├── game.h
│       └── game.cpp       load(), the hooks, reading and writing game memory
├── tests/
│   ├── ground_test.c, air_test.c, strafe_test.c, wrench_test.c, camera_test.c, clank_test.c
│   │                      the decisions, values worked out by hand
│   ├── load_test.c        the built library loaded as the port loads it
│   ├── manifest_test.py   settings keys, types and defaults against the code
│   └── data/rac2_constants.tsv   rac2's literals, extracted by tools/
└── tools/
    └── rac2_constants.py  reads the literals the mod uses out of rac2-decomp (numbers only)
```

Every source file starts with `SPDX-License-Identifier: GPL-3.0-or-later` and a comment saying what it
is for and where its facts come from.

## 6. Manifest and settings

`format` 2, `api` the lowest that works (Phase 0 finds it: hook ids in the manifest, and
`Hero_ApproachSpeed`, which is checked at run time), `games: {"rac1": ["ntsc"]}`,
`multiplayer: "input"`, `conflicts: ["lock-strafe"]`, and the seven hook ids of section 4.

| Key | Type | Default | What the player sees |
|---|---|---|---|
| `ground` | bool | true | Going Commando's running: speed follows the stick, quicker to start and stop, its turning and turnaround, no skid when letting go |
| `air` | bool | true | Going Commando's air control in jumps; jump heights stay the first game's |
| `strafe` | choice: `both`, `l2`, `r2`, `off` | `both` | Hold L2 or R2 (or only one, leaving the other to the game) to strafe as in Going Commando |
| `flips` | bool | true | Strafing, X with the stick pushed fully sideways or back flips that way |
| `twist` | bool | true | Strafing, his hips turn toward where he goes |
| `wrench` | bool | true | Going Commando's wrench timing (0.3.0, section 9); damage stays the first game's |
| `camera` | choice: `rac1`, `rac2` | `rac2` | How fast the right stick turns the camera (0.3.0, section 10): the first game's or Going Commando's fastest. 0.3.0's `modern` was removed in 0.3.1; a saved one is `rac2` |
| `clank` | choice: `rac1`, `rac2`, `enhanced` | `enhanced` | How Clank handles alone (0.4.0, section 11): the first game's, Going Commando's, or past it. Heights and punch damage stay the first game's; Giant Clank stays the game's |
| `trace` | bool, `multiplayer: "own"` | false | A log line per tick with every input each decision used. `OPENRAC_ENHANCED_MOVEMENT_TRACE=1` does the same |

The defaults are the recommended experience. With everything off and `camera` and `clank` at `rac1`,
the mod takes no hooks. One log line
at load says the disc and each setting.

## 7. Phases

### Phase 0: repository

1. `git init` in `~/Projects/rac1-enhanced-movement` and copy `OpenRAC/sdk/template`.
   - Rename the target to `rac1_enhanced_movement` and the id to `rac1-enhanced-movement`.
   - Add LICENSE, `.clang-format`, `.gitignore`.
2. Fetch the SDK (`python3 tools/fetch_sdk.py`), or point at `../OpenRAC/sdk`. Configure and build the
   empty mod.
3. Find the `api` floor from `OpenRAC/sdk/docs/manifest.md` and `c-api.md`.
4. Check that `openrac-mods/tools/cpp_style.py` runs on this folder from outside its repository.
   If it doesn't, call it with the folder's absolute path or copy its rules.

Done when the empty mod builds, packages, and `package_mod.py --check` passes.

### Phase 1: finish the research, write DESIGN.md

Every row of DESIGN.md gets rac2's code and value, rac1's code and value, and the hook the mod uses.
The open items:

1. **rac1's level copies** of the state machine (`src/overlays/l01_novalis`, `l02`, `l03`, `l05`,
   `l06`, `l08`, `l11`, `l12`, `l14`, `l16`, …). Check that the run, idle group, STOP, crouch, jump
   group and hover cases call `turn_to`, `speed_step` and `set_planar_vel` with the shared copy's
   arguments, and record any copy that differs.
2. **Transitions.** Compare rac1's run exits, idle-to-run, crouch, which jump X gives
   (`hero_handle_jump_press`, `try_start_hero_jump`) and the running jump rule with rac2's
   decider. Identify:
   - `func_L00_002B3268` (the "still turning" test);
   - `func_L00_0032F038` (the stick's yaw history behind the turnaround);
   - `func_L00_0032EF10` (the press window) against rac1's `pad_pressed_within`.
3. **Run exit details.** Find what rac2 clamps at `+0x160` and its rac1 field. Compare
   `func_L00_002B9F00`'s third argument with rac1's `stop_hero_motion_without_forward_support`.
4. **Items.** rac2's run turn special-cases items 16, 28, 29, 30, 39 and 46, and rac1's items 15,
   16 and 21. Map the ids between the games, and keep rac1's item turns where rac2 has none.
5. **Strafe and the flip state 0xB.** Read rac2's entry with the strafe (`+0x4C0`, `+0x4DC`,
   `+0x52D`), its per-tick drift in `func_L00_002B6BC8`, and rac1's high-jump drift. Find what each
   `stick_direction_vs_facing` value means, which flip animation each direction gives, and what
   gates the side timers (`+0x22B4`).
6. **Negative ground speed.** rac2's ×−0.25 makes the speed negative. Check that rac1's
   `set_planar_vel` / `BuildSphericalOffset` and `speed_step` treat it as rac2's do.
7. **Air strafe.** Check rac2's jump group before takeoff (turn toward the stick's yaw) and after it
   (the camera's yaw swapped in), and match rac1's jump group.
8. **Pose table.** Find:
   - which joints records 0 and 1 drive in rac1's Ratchet;
   - how rac1 eases the records (wrapping);
   - where rac1 writes those records each tick, so the mod's write lands after the run's lean and
     before the easing runs;
   - `D_L00_0017A780`'s NTSC address.
9. **Backpedal.** How `advance_hero_animation` moves the walk and run (`anim_frame`,
   `anim_speed`, the blend), so they can play backwards cleanly. Only a looping sequence with no
   blend pending.
10. **Camera.** How to tell the follow camera from a level's own camera (the camera slot in use).
    The NTSC names of the strafe-camera calls.
11. **Pad.** The `PadState` layout (held, pressed, raw) for L2/R2. When the game's own state 0x81
    must keep them.
12. **NTSC-U addresses.** The hero, pad, camera yaw and camera slot, `g_frame_time`,
    `g_frame_time_sq`, `g_frame_scale_sq`, the game mode, and the pose table. Each function's name.
    Every address comes from a source and the source is written next to it in `discs.h`.
13. **Other mods.** How `difficulty-mod` scales `approach_speed`, to check the ratio rule
    composes. The README names the mods that share hook points.
14. **Packs.** Confirm the hover easing and the glide thresholds really are out of reach, and that
    every pack jump's air speed is covered by 3.2.

Done when DESIGN.md has a row for everything in section 3, every open item is answered (or moved
to the README's limits with the reason), and `tools/rac2_constants.py` has extracted
`tests/data/rac2_constants.tsv`.

### Phase 2: the decisions, with unit tests

1. `rac2.h`: every rac2 number as a per-second value. `tests` checks each one against
   `rac2_constants.tsv` with the conversion written out.
2. `ground.h`, `air.h`, `strafe.h`: plain C taking plain structs (stick, state, step, yaw error,
   flat speed, timers, settings) and returning the arguments, flags and actions. No `openrac_api`,
   no addresses.
3. Tests with values worked out by hand in comments, at 60 Hz and at 50 Hz:
   - each turn branch;
   - target speeds at several stick amounts;
   - the turnaround cap;
   - the run exit;
   - the side-reversal factor sequence;
   - the sector edges;
   - the flip test;
   - twist angles across the ±π wrap;
   - backpedal entry and exit;
   - the ratio rule with and without another mod's scaling.

   Built with `-ffp-contract=off`.

Done when `ctest -R rac1_enhanced_movement` passes.

### Phase 3: the game layer, with the load test

1. `discs.h` and `hero.h` from Phase 1. `game.cpp` follows the guide's load order:
   - find the disc, or refuse;
   - read the settings once and clamp them;
   - reset all state;
   - check every required function exists, or refuse naming it;
   - take the hooks, skipping those the settings make useless;
   - log one line.
2. The hooks of section 4: each one reads game memory into the decision's struct, calls the
   decision, writes the result, and calls `original` on every path.
3. `load_test.c` builds a fake `openrac_api`:
   - `calloc` memory;
   - `hook` records the id, signature and replacement, and hands back a counting original;
   - `function` answers only NTSC names;
   - identity relocation;
   - settings from test variables;
   - `fail` saves the reason.

   It checks:
   - rac2 and PAL are refused with a reason and no hooks taken;
   - NTSC loads, and every hook is taken with the SDK's signature;
   - with hero memory laid out at `discs.h`'s addresses, each recorded replacement writes what
     rac2 would, and `original` ran;
   - each setting turned off removes its behaviour;
   - another mod's scaled arguments keep their scale;
   - reloading resets the state.
4. `manifest_test.py`.

Done when all tests pass.

### Phase 4: build and checks

`cmake --build`, `ctest`, `cpp_style.py --fix` and `--comments`, and `package_mod.py --check`.
README written: every setting, the controls, what stays rac1's, the known limits, and the conflict
with lock-strafe.

### Phase 5: play-testing on NTSC-U (you)

Install `build/package/rac1_enhanced_movement` from the launcher's Mods tab with `trace` on. The list:

- **Veldin** (family A), **Novalis** (B), **Kerwan** (C):
  - starting from standstill in every direction;
  - walking with a half stick;
  - full run;
  - 180° turnaround;
  - letting go at full speed (no skid);
  - slopes;
  - wading;
  - ice (Orxon / Hoven).
- **Jumps:**
  - plain, running, double, crouch high jump, long jump;
  - heli and thruster jumps, hover, glide;
  - each reaches the same heights as unmodded.
- **Strafe:**
  - L2, R2 and each `strafe` choice;
  - L1 look and R1 crouch still work;
  - side reversals;
  - backpedal;
  - flips each way;
  - jump while strafing;
  - twist;
  - camera;
  - a level's own camera (Kalebo III's arena);
  - hoverboard (Rilgar) untouched;
  - state 0x81 untouched.
- **Untouched:**
  - swimming;
  - Clank and Giant Clank;
  - ship;
  - grind rails;
  - Swingshot;
  - vendors, cutscenes and menus.
- **Every setting off** plays exactly as unmodded.

Each problem comes back as a trace excerpt, which becomes a test before it is fixed.

### Phase 6: release (only when asked)

Bump `version` in the manifest and `version.h` together, write the README notes, and commit as
`feat(rac1-enhanced-movement): ...`. Tagging `rac1-enhanced-movement-v0.1.0` and listing the mod in the
openrac-mods index (`index/rac1-enhanced-movement.json`, `aiAssisted: true`) wait for your go-ahead.

## 8. Risks and limits

- **Indirect control.** Without a hook on the state machine itself, rac2's behaviour is reached
  through rac1's helpers and flags. A level copy that differs (Phase 1, item 1) needs its own case.
  An unrecognised call passes through and is traced, so the worst case is rac1's behaviour at that
  spot, not a crash.
- **One tick of difference.** Transitions the mod makes in the post-update step happen where rac1's
  own transition would, just before the decider. A state rac1 enters from inside the state machine
  gets its entry parameters one tick later.
- **Animations are rac1's.** There is no backpedal (the walk plays backwards) and no strafe-jump
  animations beyond the flips rac1 has.
- **Out of reach**, listed in the README:
  - the hover's eased descent;
  - the heli glide's thresholds and overhead check;
  - rac2 dropping rac1's per-tick limit on falling speed (`resolved − 0.1`, inside
    `hero_integrate_velocity`).
- **Other mods** on the same hook points (`difficulty-mod` on `approach_speed`) are expected to
  compose by the ratio rule. lock-strafe is declared a conflict.

## 9. The wrench (setting `wrench`, 0.3.0)

rac2's wrench, researched from both decompilations (DESIGN.md section 5), turned out to be rac1's
code carried on: the same three states (combo 0x13, slam 0x14, throw 0x15), the same movement in
them, the same hits. What changes is timing. The mod ports what changes, and keeps what decides how
hard rac1's levels are.

| | rac1 → rac2 | The mod |
|---|---|---|
| Square's window, standing or running | 7 → 10 polls | ports it: a Square rac2 would still take is shown to rac1 as pressed this poll |
| Square's window in a jump (the slam, or a swing near the ground) | 10 → 14 polls | ports it, the same way |
| The first swing chains the second | from frame 26 → 25 | ports it: rac1 sees frame 26 for the tick |
| Leaving a swing with the stick pushed (past 0.8) | idle at frame 31 / 23 / 21 → the run at 26 / 15 / 21 | ports it: rac1 sees its exit frame for the tick, stands, and its idle runs on the next tick |
| Leaving a swing without the stick | idle at 31 / 23 / 21 → 31 / 20 / 26 (or the animation's end) | ports it: an earlier exit as above; a later one by holding rac1's exit for the tick |
| The throw (crouch + Square): moving on while the wrench flies | stuck until the catch (frame 91) → the run past frame 34 with the stick past 0.9, X jumps out from frame 35 | ports both |
| No Square while the wrench is out | rac1 still slams, or throws again from the crouch → nothing | ports it: Square is hidden from rac1 while the wrench is out |
| The lock-on turn (swings, slam, throw) | 15.01 → 16.93 rad/s; the throw's accel 0.05 → 0.057 | ports it in `turn_to_heading` |
| The slam from more than 4 above the ground | slows to a stop → keeps driving at 4.5 u/s | ports it in `approach_speed` |
| Damage, hit frames, the wrench's flight and reach | the same in both games | rac1's (nothing to port) |

Decided on the line between feel and balance:
- **Damage stays rac1's**, and the mod changes no hit: rac1's enemies and bosses are balanced around
  rac1's hits, so rac2's hit code (in the wrench's moby update) is not ported.
- **The faster exits and the throw's freedom** are rac2's timing. They let the player move and
  jump sooner, but never make a hit land sooner, harder or farther. rac2's slower exits (the third
  swing standing until 26) are ported too, so the combo is not quicker overall than rac2's (the
  play-test's three swings: 96 ticks in rac2 and with the mod, 88 in rac1).
- **The throw stays R1 + Square.** rac2 dropped R2 for it because R2 strafes. With the strafe on R2
  (the default) that is already so; with `strafe` set to `l2` or `off`, R2 stays rac1's crouch, so
  its throw stays too.

Left out, with the reason (README, known limits):
- **The combo continuing through a short run** (rac2 keeps the next swing if the combo was the state
  before the last and both lasted under 27 ticks). rac1 picks the next swing inside
  `HeroEnterMoveState` from its previous control mode, and no hook point sits in the state entry.
  Faking the mode for a tick would also turn rac1's item code down its attack branch.
- **The thrown wrench's flight details** (it carries 0.98 of his momentum where rac1 carries 0.7 of
  his velocity, and drops back to his hand past 32 units). Both are inside the wrench's own update.
  The only hook point there is `rac1.moby.wrench.update`, which the SDK marks experimental, and
  standing still the flights are the same.
- **The slam's ground flags** (rac2 also drives on three ground types). rac1's matching bytes are
  unnamed, so the mapping would be a guess.
- **The swings' ledge test** (rac2 probes 0.35 down, rac1 0.2). It is inside
  `stop_hero_motion_without_forward_support`, which no hook point reaches.

How: no new hook point and no game function called. The post-step of `update_motion_input`
works out what rac2's input code and decider would do this tick and what rac1's will do
(`src/rac1/wrench.h`). Where they differ it sets, for the transitions only:
- the animation's frame (`+0xAA8`), or the blend flag (`+0xA9C`) that makes rac1 hold the swing;
- Square in, or Square and X out of, the pad's press history.

It puts the hero's fields back in `advance_animation` and the pad's in `process_report`.
`turn_to_heading` gives the lock-on turn, `approach_speed` the slam's drive.

Other mods: **wrench-throw** (openrac-mods index) also takes R2 on foot and throws from
`rac1.player.update`. It runs beside this mod when the strafe is set to L2 only, so the manifest
declares no conflict, and the README says to set L2 (DESIGN.md section 7).

Risks:
- The swings are steered through `anim_frame`. Between the transitions and the next tick, the
  wrench's hit test reads it, and so does the timed-events code, which only reads it for the walk
  and run. The steered frames are past every hit window, so nothing hits early or twice.
- The run out of a swing is one tick later than rac2's (rac1's idle, then its run).
- Running or jumping while the wrench flies is new to rac1's throw, not to its code: the
  first-person throw already does it. The play-test ran and jumped with the wrench out on both discs.
- At 50 Hz both games' Square windows are the same length, so nothing is buffered there. OpenRAC
  runs PAL at 60 Hz, so only the tests run that path.

## 10. The camera (setting `camera`, 0.3.0; `modern` removed in 0.3.1)

Research (DESIGN.md section 8, with the evidence):
- **rac1** turns its follow camera with the right stick only: rate control for yaw, gain × stick x a
  tick (`follow_camera_turn_and_pitch`), the gain from a three-step table by the game's own camera
  speed option (1.0, 1.3, 1.6 degrees a tick; the middle by default: 78 degrees a second). No input
  ramp; the eye follows the orbit through a spring (0.015 / 0.2): half the rate after 11 ticks, 90%
  after 24, 15 degrees coasted after letting go. The pitch is position control, ±40 degrees, back to
  level on letting go. The pad's dead zone is 37.8% per axis. First person is another camera, with a
  squared curve to 87 degrees a second.
- **rac2** has the same functions and the same numbers, the dead zone and first person included, but
  for its table's fastest step: 1.0, 1.3, **2.2**. Its default is the middle step too. Measured in
  OpenRAC's rac2 port, its turn is rac1's to the decimal (PLAYTEST.md). So at the default camera speed
  the two games turn the camera alike; Going Commando is quicker only on its fastest step (132 degrees
  a second against 96).
- Around the turn rac2 differs a little (its camera dragged round behind him a little more, its swing
  toward a point of interest waiting 4 seconds after the stick moves). Those are not the turn's speed:
  reported, not ported.

**Decision (the user, 2026-11-14): `rac2` is Going Commando's top speed.** Faithful to rac2's
default step, `rac2` would change nothing until the player raised the game's camera speed. The user
chose to have it always take rac2's fastest step (2.2 degrees a tick, 132 a second), whichever step
the game's option is on, so the two choices rise one above the other: 78 and 132 degrees a second at
the game's default. Only rac2's fastest number is used; the pitch, the springs and the dead zone are
the same in both games anyway.

The choices:

| | Yaw at full stick, by the game's camera speed step | Pitch | The eye's ramp and stop |
|---|---|---|---|
| `rac1` | 60 / **78** / 96 degrees a second | the game's | the game's: 90% after 24 ticks, 15 degrees coasted |
| `rac2` (default) | 132 / **132** / 132 | the game's (rac2's is the same) | the game's (rac2's is the same): 90% after 24 ticks, 27 degrees coasted |

At 50 Hz `rac2` keeps rac1's per tick (rac1's PAL disc does not scale its camera).

**Decision (the user, 2026-11-14): `modern` removed in 0.3.1.** 0.3.0 had a third choice, `modern`:
three times the game's own speed (180 / 234 / 288 degrees a second by the game's step), picked as the
middle of the 180 to 300 degrees a second current third-person games turn at by default, with the
pitch's steps three times as quick and the eye's spring stiffened to 0.2 / 0.7 while the stick turned
the camera and for 20 ticks after (90% of the rate within 6 ticks, stopped within 10). The user
play-tested 0.3.0 and found it far too fast, even on the game's slowest camera speed (180 degrees a
second). It is removed, not retuned: `rac2` (132) is the fastest choice, and nothing replaces it.
Players who saved `modern` get the default, `rac2`: the launcher drops a value the new manifest no
longer takes when it installs the new version (`plugins::install`), the port hands the default for one
anyway (`resolve_settings`, with a warning in the log), and `camera_choice()` gives `rac2` for any
value it does not know (the load test feeds it `modern`). PLAYTEST.md keeps 0.3.0's measurements of
`modern`, marked as removed.

How (no new hook point, no game function called): `rac1.camera.follow.apply_overrides` runs after
`follow_camera_update` sets the gain from the table and before the turn reads it. For `rac2` the hook
writes the gain × rac2's entry over rac1's. rac1 sets the gain again at its next update; the pad hook
puts it back before the next tick, as the mod's other steers.

Left out (README, known limits): first person (the same in both games; no hook point brackets its
camera), the dead zone (it would change the stick for the whole game), and the differences around the
turn. Other mods: **modern-controls** drives the right-stick camera itself, so with it `camera` should be
`rac1`; **steam-deck-pack**'s gyro and trackpad feed the right stick and take this setting's speed
(DESIGN.md section 7).

Risk: if a tick ran the camera's update twice without a pad poll between, the hook's own start puts
the gain back first, so it never compounds (the load test runs that).

## 11. Clank (setting `clank`, 0.4.0)

Your request: Clank is "notoriously slow" in the first game and not loved in Going Commando either,
so improve his controls past Going Commando's, on the principles the mod used for Ratchet, his
attack, jump and movement included. Research and evidence: DESIGN.md section 9.

**What the research found.** Clank alone is the hero in vehicle mode 1, with states of his own run
through the same helpers as Ratchet's, so the existing hook points reach him (DESIGN.md 9.4).
- **rac1's Clank** already moves at a speed that follows the stick (3 u/s at full stick), has no skid
  and stops dead when you let go. What makes him slow to handle is the rest:
  - he gets going at 7.5 u/s², cut further while he turns (90% of his speed after 24 ticks);
  - at a run he turns on a soft spring (90 degrees in 29 ticks);
  - a turnaround is a loop along his facing (19 ticks before he moves the new way);
  - his jump leaves the ground 6 ticks after X;
  - his punch takes 24 ticks and can only be left by jumping;
  - Square counts for 9 polls before he can punch.
- **rac2's Clank** is the same code with a few changed numbers:
  - Going Commando's run acceleration and braking (18, 20);
  - a firm turn (0.033 / 0.15 / 9.95: 90 degrees in 11 ticks) and its turnaround spring;
  - moving along the stick when slow;
  - a 3-tick takeoff;
  - the wrench's quicker lock-on turn (16.93);
  - a punch whose frames come 2 ticks sooner (22 ticks; out by a jump after 18).

  His speed, heights, air speed, glide, windows, records and hits are the first game's. That is
  enough difference for a choice of its own.
- **Giant Clank** (rac1's Quartu boss fight, a walking mech) has no Going Commando counterpart to
  port or to anchor a step to: rac2's Giant Clank is a flying fight in orbit. Left out (below).

### 11.1 The choices

| Choice | Clank |
|---|---|
| `rac1` | the game's; the mod takes nothing of him (with everything else off, no hooks) |
| `rac2` | Going Commando's where it differs (above), with the first game's hit, lunge and reach (rac2's punch hits 2 ticks sooner and lunges 0.69 units, not 0.81: only its way out is ported) |
| `enhanced` (default) | `rac2`, and past it by the steps of 11.2 |

### 11.2 `enhanced`: each number and its anchor

All in `src/rac1/clank_tune.h`, to adjust after playing. The rule: start from rac2's Clank where it
improved on rac1. Go past it only by the size of the step Going Commando itself gave Ratchet in the
matching quantity, and never past that step. Your rejection of the camera's `modern` (three times the
game's speed) is the reason for this rule.

| Quantity | rac1 | rac2 | `enhanced` | Anchor |
|---|---|---|---|---|
| Top speed | 3.0 u/s | 3.0 | **3.45** | Your choice (2026-11-14), 15% over both games. First built at 3.13, Ratchet's step 5.7 to 5.95 (× 1.044) applied to Clank, which left a long walk as slow as the first game's; still well under Ratchet's 5.95 |
| Speed by stick | 3 × stick | the same | 3.45 × stick | both games already follow the stick |
| Getting going, slowing | 7.5 × k, 8.5 | 18, 20 | **18, 20** | rac2's (Ratchet's own rac1-to-rac2 step: 7.5 to 18). At 18 he reaches 90% of 3.45 in 12 ticks; quicker would be twitchy |
| Turn, turnaround | by stick and speed; 90° in 29 ticks at a run | 0.033 / 0.15 / 9.95; 0.037 / 11.69 | **rac2's** | already a bigger step than Ratchet's (his run band 40 to 18 ticks for 90°; Clank 29 to 11) |
| Along the stick when slow | never | under 0.7 u/s | **rac2's** | |
| Letting go | stops in a tick | the same | the same | no skid in either game |
| Jump takeoff | 5 ticks | 3 | **3** | rac2's |
| Air control in his jump | 2.0 u/s (67% of his run) | 2.0 | **2.5** (80% of 3.13, Ratchet's step applied to his top speed; kept at 2.5 with the top speed at 3.45, so his jumps reach no further than they were played) | Going Commando raised Ratchet's double jump from 61% to 80% of his run speed (3.5 to 4.75) |
| Jump heights, gravity | 1.3 / 1.35, 25 | the same | **the same** | the levels' |
| Glide, fall | 2.0, 3.0 | the same | **the same** | their match, Ratchet's heli hover, is the same in both games |
| Lock-on turn | 15.01 rad/s | 16.93 | **16.93** | rac2's, the wrench's step |
| Square remembered standing, running | 9 polls | 9 | **13** | Ratchet's 7 to 10 (× 10/7) |
| Square remembered landing | 5 | 5 | **7** | Ratchet's 10 to 14 in a jump (× 1.4) |
| Leaving a punch | stands after frame 10 (24 ticks); jumps out after 8 (21) | a frame sooner: 22, 18 | **rac2's**, and with the stick past 0.8 it runs out past frame 7 (19 ticks to the run) | rac2's run out of a swing (stick 0.8). Going Commando let Ratchet run out of his swings 10 and 16 ticks sooner than the first game; this is 6 sooner than the first game's Clank and 4 sooner than rac2's, 1 frame after the hit |
| Hits, damage, the lunge | frames 3 to 6, 1.0, 0.81 units | (sooner, shorter) | **rac1's** | the levels' |
| X windows, Gadgebot commands | | | the game's | |

Measured (PLAYTEST.md, Clank) on the same route in the first game and in Going Commando's port:

| | rac1 | `enhanced` |
|---|---|---|
| 90% of the run speed | 24 ticks | 11 |
| The turnaround: moving the new way | 19 ticks | 1 |
| 90 degrees | 19 ticks | 7 |
| Off the ground | 6 ticks after X | 4 |
| A punch | 24 ticks | 22; with the stick pushed out after 18, running after 19 |
| A running jump | 1.10 units | 1.37 |

`rac2` matches Going Commando's port on every one of those. The one exception is rac1's own pipeline,
which starts and stops the run a tick after rac2's, with or without the mod.

### 11.3 The levels as built

- **Heights, hits, damage, reach** stay the first game's in every choice, for the reason Ratchet's
  do: Orxon's and Blarg's Clank parts and their enemies are built around them.
- **Timed and triggered things in his sections** (rac1-decomp `l10_orxon`, `l06_blarg`; what was
  checked):
  - Orxon's gates: `gate_1` opens for Clank by itself; `gate_2` by flags.
  - Its platforms: `cycling_platform` (1.5 units up and down over 45 ticks, then 120 ticks of wait);
    `sinking_platform` (sinks while stood on, at most 2 u/s).
  - Its doors and buttons: `lab_door` (opens over 20 ticks), the pressure plate `button_1031`.
  - The Gadgebots' telepot pad (a 90-tick timer), the bot elevators and door locks.
  - Blarg's Clank part: the timed and oscillating toxic gas vents, the beam barriers, the drawbridge.

  Each waits for him or punishes him for being there. None needs him slow, none counts his time, and
  each clip volume is far wider than a tick of his run (0.05 units). A 4% faster, quicker-starting
  Clank only arrives sooner and crosses a vent's or a sinking platform's danger sooner.

  On Orxon the play-test walked the start through both gates to the first toxic crab and punched the
  crates by the start: the same events in every choice, only sooner (PLAYTEST.md).
- **The Gadgebot commands** are the game's: the mod changes no input but Square's memory and X's for
  the steered tick, and only in his idle, run, landing and punch.
- **Giant Clank** is left out (above; DESIGN.md 9.3): the mod acts only in vehicle mode 1.

### 11.4 How

No new hook point and no game function called (DESIGN.md 9.5):
- `update_motion_input` gives Clank a tick of his own (`clank_update`), with Ratchet's paths off.
- The jump's entry values are written on its first tick, before its first physics tick.
- `turn_to_heading`, `approach_speed` and `set_planar_velocity` change his run's and his punch's
  arguments by the ratio rule.
- The post-step steers his transitions for one tick: a Square shown as pressed, and the punch's frame
  set to where rac1's own code leaves it as the choice would. `advance_animation` and `process_report`
  put both back.

With `clank` `rac1` the mod takes the hooks it took before for the other settings and passes every
call of Clank's through. With everything else off, it takes none.

### 11.5 Left out, and risks

- Giant Clank (11.3).
- rac2's punch hitting sooner and lunging less: the levels' reach.
- The glide's and the fall's speeds: no anchor.
- rac2's L1-only look.
- rac2's landing animation frame.
- **Risk:** the punch's frame is steered for a tick, as the wrench's is. Its hit test runs inside his
  update before the steer, and the steered frames (8.5 to 11.5, or 10.5 to stand) are past its hit
  frames, so nothing hits early or twice.
- **Risk:** the run turn is recognised by rac1's own two springs (within 1%). A level copy calling it
  with other numbers would keep rac1's turn and say so in the trace. Blarg's copy was diffed: the same
  numbers.
- **Not covered by script:**
  - Blarg's Clank part, which needs Ratchet at its split door with Triangle;
  - Orxon past its first crab: the Gadgebot puzzles, whose containers break on his punch. The way
    there is not a straight line from the start, and the crab out-fights a walk-in-and-punch script in
    the game without the mod too.

  Play them once by hand.
