# rac1-enhanced-movement: the design

How Ratchet & Clank: Going Commando (rac2) moves Ratchet, how Ratchet & Clank (rac1) does it, and
how this mod makes rac1 do what rac2 does. Every row names its evidence:
- `rac2-decomp` (USA v1.01, level 0's shared hero code);
- `rac1-decomp` (PAL names);
- the NTSC decomp OpenRAC carries (`OpenRAC/games/rac1/ntsc-decomp`).

PLAN.md is the plan; this is the result of its Phase 1.

Units: rac2 writes its numbers for 60 Hz, so a per-tick speed × 60 is units per second (u/s), a
per-tick² acceleration × 3600 is u/s², and a turn limit × 60 is rad/s. rac1 writes per-second numbers
times `g_frame_time` (speeds, turn limits), `g_frame_time_sq` (accelerations) and `g_frame_scale_sq`
(turn spring coefficients). The mod keeps rac2's numbers per second (src/rac1/rac2.h) and scales them
the way rac1 does. `tests/data/rac2_constants.tsv` holds each literal as rac2-decomp writes it, and
`rac2_test` checks the conversion.

## 1. How a tick runs (rac1)

`update_hero_frame_pipeline` (`src/overlays/shared/hero_frame_update.c`), on foot:

1. `advance_hero_animation`: hook **`rac1.player.advance_animation`**.
2. `hero_update` (`func_L00_00211D28`): hook **`rac1.player.update_motion_input`**. It reads the stick, then runs
   `hero_update_movement` (the state machine; each family of levels has its own copy and the
   call resolves to the loaded level's), then integrates and collides.
   - The state machine calls the shared helpers `turn_to`, `speed_step` and `set_planar_vel`:
     hooks **`turn_to_heading`**, **`approach_speed`**, **`set_planar_velocity`**.
3. Ground, ledge and rail probes.
4. **The state transitions** (each family's own copy: `hero_update_transitions`, …).
5. The pose records' easing (`update_hero_easing_records`), the moby, items, drawing.

The pad is polled before the tick (`process_pad_input`: hook **`rac1.pad.process_report`**). The
follow camera's moby asks for its per-tick settings in `follow_camera_apply_overrides` (hook
**`rac1.camera.follow.apply_overrides`**).

All eleven level copies of the state machine (`src/overlays/l01…l16/hero_movement.c`) contain the
shared copy's run, idle, STOP, crouch, air-turn and turnaround constants (counted with grep:
`7.5f * k`, `8.5f`, `turn_hero_toward_move_direction_with_stick_scale`, `0.008f/0.15f`, `12.6f`,
`12.0f`, `6.48f`, `0.015f/0.2f/15.0`, `17.0f`). So a hook that recognises the call by the hero's state
behaves the same on every planet.

### Why the mod steers rac1's transitions instead of calling them

`HeroEnterMoveState` (`func_L00_00222B80`) and `hero_idle_sequence` are one function only in levels
0, 4, 7, 9, 10 and 13 (`config/overlays/us_map.tsv`). The other levels have copies of their own,
and `openrac_api.function(name)` resolves a name to one compiled function
(`OpenRAC/port/runtime/guest.cpp`, `openrac_guest_function_named`), not to the loaded level's
copy. Calling it by name on Novalis would run another level's code.

So the mod changes state the way a player would. Just before rac1's transitions run (the post-step
of `update_motion_input`), it sets the few values the transition reads so that rac1's own code (the
loaded level's copy) takes the branch rac2 would take. Then it puts them back:

| Steered value | Put back in | When |
|---|---|---|
| Hero fields (state timer, speeds, target speed, stick amount) | `advance_animation`, the next tick's first hook | only if the state did not change and the field still holds what the mod wrote |
| Pad fields (R1 in `held`, a stick press in the press history) | `process_report`, before the next poll processes its report | always |

## 2. Ground (setting `ground`)

### 2.1 The run's numbers (states 2 and 0x73)

rac1: the `HERO_STATE_RUN` / `0x73` case of `hero_update_movement` and
`update_hero_target_speed_from_stick`, `turn_hero_toward_move_direction_with_stick_scale`
(`hero_animation_slots.c`). rac2: `func_L00_002BA7D8` case 2/0x70, `func_L00_002B3D10`,
`func_L00_002B3F68`.

| | rac1 | rac2 | Mod |
|---|---|---|---|
| Target speed | walk or run from `hero_speed_steps` by stick band | `D_001A7DB0[4]` (5.95) × stick; wading × 0.8, at least 2.5 (`0.041666668`) | `approach_speed`: rac2's, written to `+0x190` for the call and put back after |
| Stick under 0.2 | target as from the stick | target 0 | the same |
| Accelerate / brake toward target | 7.5·k / 8.5 | 18·k (`0.005`) / 20 (`0.0055555557`) | `approach_speed` |
| k | 1 − err·0.55 firing; 0 when stick < 0.8 and err > 30° | 0 when (stick < 0.9, or `unk20AA` without turnaround) and \|err\| > 45° (`0.78539824`) | rac2's |
| Slow stick, wide angle | — | target × max(0, 1 − 0.4·\|err\|) when stick < 0.75 and \|err\| > 35° (`0.6108653`) | rac2's |
| Turnaround target | × clamp(2.618 − err, 0.2, 1), brake 17 | the same, after capping it at 5.95 − 1.75·\|err\| (`0.09916667 − err·0.029178405`), brake 17 (`0.004722222`) | rac2's |
| Run-band turn (`state.step` 1, no turnaround, no item flag) | 0.008 / 0.15 / 9.95 | 0.02 / 0.17 / 9.95 (`0.16580631`) | `turn_to_heading` |
| Walk-band turn | stick > threshold: 0.019·s / 0.1 / 9.95·s, s = (stick+1)/2; else 0.005·t / 0.1 / 4.71·t, t = stick + 0.35 | full stick 0.022 / 0.27 / 5.585 (`0.093084246`); else accel stick·0.02 in [0.01, 0.022], limit stick·3.84 in [1.92, 5.585] (`0.063995413`, `0.031997707`) | rac2's |
| Turnaround turn | ×1.1 accel, ×1.2 limit | 0.035 / 0.27 / 11.69 (`0.19489513`) | rac2's |
| Turn while firing a hold-to-fire weapon | items 15, 16, 21 (Blaster, Pyrocitor, Morph-o-Ray) with `item_anim_active`: 0.003 / 0.04 / 4.36 | its own weapons with the item flag and no turnaround: stick > 0.95: 0.017 / 0.18 / 8.73; else stick·0.009 / 0.15 / stick·4.71 | rac2's values for rac1's weapons (the role, not the ids: the item tables differ) |
| Move along the stick, not the facing (`unk3B8` / rac2 `+0x40C`) | set when flat speed < 0.3 at stick > 0.85; cleared under 3°, item, run band | set when flat speed < 1.7 (`0.028333336`); cleared under 5° (`0.087266475`) | the mod keeps rac2's flag and gives `set_planar_velocity` the move yaw while it is set |
| Turnaround flag (`unk3BE` / rac2 `+0x40F`) | set by rac1's transitions on the pad's stick-flick bit (`pressed & 0x10000`) | set while slow (< 1.7) with \|err\| > 120° (`2.0943954`) at full stick; or the stick's angle 1 poll back against 7 back (`ReadPadFloatHistory`, `hist_ang` at pad `+0x258`) more than 110° (`1.9198624`) apart with \|err\| > 70° (`1.2217306`) at full stick. Cleared under 4° (`0.06981318`) | the mod keeps rac2's flag and writes it to `unk3BE` before each run tick, which replaces the flick |
| Letting go | velocity along the facing | the same; only one rac2 item keeps the stick's direction (`+0x410`) | nothing to do |

`err` is the hero's yaw error (`+0x188`), the move yaw less the facing, from the previous tick's
turn. As rac2 does, the flags are worked out before the turn from that value.

### 2.2 Braking in the idle group, STOP and the crouch

`speed_step(0, brake)` in rac1's idle group. rac2's group is `func_L00_002BA7D8` case 0/1/3/4/….

| | rac1 | rac2 | Mod |
|---|---|---|---|
| Idle, look, item waits (0, 1, 0x17, 0x1D, 0x1E, 0x1F, 0x70–0x72) | 12.6 | 17.7 (`0.004916667`) | brake × 17.7/12.6 |
| STOP (3) | 12·t | 27·t (`0.0075` × t) | × 27/12 |
| Crouch (4) | 6.48, ice 1.7 | 27, ice 1.7 (`0.00047222225`) | × 27/6.48, ice unchanged |

### 2.3 Leaving the run

rac1 (`hero_update_transitions`, run case): with the stick under 0.17 it goes to STOP (the skid)
when `speed_xy` (`+0x160`) is over 2.7 u/s and `unk20AA` is 0. Otherwise it goes to idle once the
state timer passes 25 ticks.

rac2 (`func_L00_002CB3A8`, case 2): with the stick under 0.17, and not still turning to the camera
while strafing (`func_L00_002B3268(0.0349)`), it goes to idle at once. Then it clamps the momentum
(`+0x160`, rac1's `+0x150`) to 1.5 u/s (`0.025`) and the ground speed to 1.5.

The mod steers this for the tick:
- `speed_xy` just under 2.7 u/s and the state timer past 25 ticks, so rac1's own code picks idle;
- on the next tick, if he entered idle from the run, clamp the momentum and the ground speed.

The clamp waits a tick because rac1's idle entry copies the momentum from `resolved_velocity`
(`HeroEnterMoveState`, idle arm).

### 2.4 Which jump X gives in the run

rac1 picks the running jump (9) when the target speed is over the walk band's top, the stick is
within 60° of the facing, and the flat speed is over 0.85 × run. rac2 (`func_L00_002CB3A8` case 2)
picks it when the stick is past 0.85 and no strafe is held.

The mod steers the 9-to-7 direction: when rac2 would give the plain jump and X is in the jump
window (9 polls, from the pad history as `pad_pressed_within` reads it), it sets the target speed
to 0 for the transition. It does not steer the other way: with the stick past 0.85 but rac1's speed
test failing (just after starting), rac1 still gives the plain jump (README, limits).

## 3. Air (setting `air`)

rac1's `HeroEnterMoveState` jump arm against rac2's `func_L00_002C42E0` jump arm, the 7-group:

| | rac1 | rac2 |
|---|---|---|
| Gravity, air accel / brake | 29.7, 20 / 11 | 29.7 (`0.00825`), 20 / 11 |
| Air turn | 15.01 | 15.01 (`0.2501639`) |
| Default air stick speed (`air_stick_speed`, `+0x424`) | 5.7 | 5.95 (`c4A4` = `0.09916667`) |
| Running jump (9) | 5.7 | 5.95 |
| Double jump (0xE) | 3.5 | 4.75 (`0.07916667`) |
| Plain jump heights | 1.47 / 2.62 | 1.28 / 2.3: **kept rac1's** |
| Heli 0xA, long 0x10, high 0xB, running 9 heights | | the same |
| Pack high jumps 0xD, 0xF | keyed lift / 1.9 | 4.0 plain / 1.65: **kept rac1's** (heights) |

The mod: on a jump state's first tick, which `update_motion_input`'s pre-step sees before the
state's first physics tick (transitions run after `hero_update`), it raises `air_stick_speed`:
5.7 → 5.95 wherever rac1 left its default, and 3.5 → 4.75 in the double jump. Other values (2.0,
1.9, 3.3, 4.0) are the same in both games and are left alone.

**Unchanged and out of reach.** rac2 drops rac1's per-tick limit on the fall
(`max(vz, resolved_vz − 0.1)` in `hero_integrate_velocity`; rac2's `func_L00_002B6560` keeps only
the 50 u/s terminal speed). The hover (state 8, misnamed `HERO_STATE_LAND`) eases into its descent
in rac2 (`func_002AA3A8`, 0.025 / 0.3) where rac1 sets it. The heli glide's height thresholds are
2.8/2.2 (rac1 2.7/2.1), with a check for room overhead. All three sit inside functions no hook
point reaches.

**Swimming** (rac1 0x33–0x37, rac2 0x32–0x36) is the same code with the same numbers in both games:
the pitch spring, `turn_to(0.007, 0.08, 5.24)`, targets 3 / 6 / 7 / 2.8, speed steps 7/4 and 13/8,
the surface 3.0 / ×5.5 / 4.2. Nothing to port.

## 4. Strafe (settings `strafe`, `flips`, `twist`)

### 4.1 rac2

- **Held**: `func_L00_002B36E0` sets the short at `+0x1454` to 1 while `pad & 3` (L2 or R2), else
  counts it down. In state 0x10 it is 0.
- **Sectors**: `func_L00_002B39F0` (the stick handler), while strafing. The stick's angle
  `atan2(x, y)` within 45° of π/2 is back (3), of 0 right (1), of π left (0), else forward (2)
  (`+0x1457`). Back with the stick past 0.15 is the backpedal (`+0x1456`).
- **Facing**: `func_L00_002B35A0`, each run tick while strafing.
  1. It saves the move yaw (the stick's) at `+0x1458` and ticks two side timers (`+0x146C`, `+0x146E`).
  2. It puts the camera's yaw in the move yaw.
  3. It turns with a yaw rate kept for the strafe (`+0x145C`): `func_L00_002B3EF8(0, 0.037, 0.25,
     0.20362177)`, 12.2 rad/s.
  4. Then it zeroes the yaw rate and yaw error.
- **Side reversal**, on foot only (vehicle mode `+0x22B4` 0):
  - going left sets the right timer to 5 and, if the left timer runs, multiplies the ground speed by −0.25;
  - going right sets the left timer to 5 and, if the right one runs, by (4 − t)/4.
- **Velocity**: `func_L00_002B4208(+0x1458)`, along the stick.
- **Standing**: idle goes to the walk when the stick passes 0.22, or when L2/R2 is held and the
  facing is 10° or more off the camera's yaw (`func_L00_002B3268(0.17453295)`). The walk then turns
  him, and its exit waits until he is within 2°.
- **Jump**: X in the run gives the plain jump while strafing (`func_L00_002CB3A8` case 2).
- **Flip** (`func_L00_002C9FC8`, from idle and the run): X in the window, strafing, stick past 0.9,
  and the stick not forward of the facing (`func_L00_002C3A78() != 2`). This enters state 0xB.
  - The entry with the strafe: drift yaw from the strafe (`+0x4C0`), drift speed 5 (`+0x4DC` =
    `0.083333336`), flag `+0x52D`.
  - Each tick (`func_L00_002B6BC8`): the drift eases to max(stick·5, 2) at 37 u/s² (`0.010277778`),
    the ground speed is the flat speed, and there is no part along the takeoff yaw.
- **Air**: before takeoff the jump group's velocity follows the strafe yaw. After it, the air turn
  (`0.04 / 0.2 / air_turn_rate`) faces the camera (move yaw swapped round the call), and the steering
  follows the stick (`func_L00_002B6BC8`).
- **Twist**: `func_L00_002B7B80`. With the stick past 0.15, not in 0xB and not backpedalling:
  - pose record 0's yaw target `+0x68` = `wrap(strafe yaw − facing)`, spring `+0xA4` 0.037 / `+0xA8` 0.27;
  - record 1's `+0x118` = minus that, spring 0.025 / 0.27;
  - no lean that tick.
- **Camera**: `func_L00_004224D0`. While strafing:
  - orbit clamp 0;
  - the slide;
  - lateral spring 0.04 / 0.2;
  - the recentre timers zeroed;
  - no recentring.

### 4.2 On rac1

| rac2 | rac1, and the mod |
|---|---|
| L2/R2 strafe; L1 looks, R1 crouches | rac1 looks on L1 or L2 (`held & 5`) and crouches on R1 or R2 (`held & 0xA`). `process_report` takes the strafe button(s) out of `held`, `pressed`, `released`, the `raw_` copies, `unk348`, the newest press-history entry and their pressure (`analog[14]` L2, `[15]` R2), and keeps them for the strafe. That happens in play (`cinematic_mode` 0), on foot (vehicle mode 0), in control modes 0, 1, 2, 4, 5, 6, 8 and 0xC, and outside state 0x81, rac1's own strafe-like state. The menus read the `_unmasked` copies, which are left alone |
| Strafing allowed | follow camera in use (`g_current_follow_cam`, `D_L00_00166F00`: its s16 at `+0x86` is 0; rac1's volume cameras test the same), gravity mode 0, not state 0x10 |
| Sectors | the mod's own, from the stick axes (`+0x1D20`), as rac2 |
| Facing | `turn_to_heading` in the run: the camera's yaw (`camera_yaw`, `D_L00_00166ED8`) for the call, rac2's spring values, the strafe's own yaw rate kept by the mod, yaw rate and error zeroed after; then the side factor on `ground_speed` (`+0x194`), as rac2 does before its speed step |
| Velocity | `set_planar_velocity` in the run: the stick's yaw instead of the facing |
| Standing | steered: stick amount (`+0x229C`) set to 0.23 for the transition when idle, strafing and 10° off. The run's exit (2.3) waits for 2° |
| Plain jump | steered (2.4) |
| Flip | steered: rac1 enters 0xB only from its crouch high jump (`try_start_hero_jump`): `(pad_held & 0xA)` and X and a stick press (`0x1F000`) within the window (`check_pad_press_separation`, 12 ticks unless forward, `get_hero_high_jump_input_window_frames`). For the tick, the mod puts R1 in `pad_held` and the stick-flick bit in X's press-history entry. rac1's own entry then picks `0x1C + stick_direction_vs_facing()`, the same animations as rac2. Each tick after, `update_motion_input`'s pre-step presets the drift so rac1's easing (toward 4.2 at 8 u/s², or 0 near landing) lands on rac2's value; it clears the takeoff part (`air_target_speed` 0), and the post-step sets the ground speed to the flat speed |
| Air | `set_planar_velocity` before takeoff uses the last strafe yaw. `turn_to_heading` after takeoff (jump group, fall) uses the camera's yaw |
| Twist | rac1's pose table `hero_eases` (`D_L00_0017A780`, NTSC `0x0017A680`) has rac2's record layout, 0xB0 bytes (`update_hero_easing_record`): targets `+0x60`, joint `+0xA0`, spring `+0xA4`/`+0xA8`, eased with `SpringAngle(…, mode 2)` and cleared each tick. The post-step writes records 0 and 1 as rac2 does, before the easing runs. Records 0 and 1 drive the same joints as in rac2: rac1's run lean writes the same records with rac2's coefficients (`adjust_hero_pose_targets_from_motion` record 0 x −0.14·err; rac2 −0.11·err), and **the play-test confirmed it** (PLAYTEST.md): strafing right, his legs and tail turn right and his head and upper body stay ahead |
| Backpedal | rac1 has no backpedal animation (0x14 is the run's stop). While backpedalling in the run, the mod plays the walk (sequence 3) or run (4) backwards (2.5) |
| Camera | `apply_overrides`: after the original, rac1's own strafe requests, as its state 0x81 makes them (`set_follow_camera_orbit_clamp_mode(0)`, `follow_camera_start_slide()`, `follow_camera_set_lateral_spring(0.04, 0.2)`, the camera vars' `+0x12/+0x14/+0x16` zeroed). Then the turn blend weight (`D_L00_0015F040`) that the recentring wrote is put back. This is the 0x81 branch's effect without its early return: a level's camera hook in between may read the state, so the mod does not fake state 0x81 |

### 4.3 The backpedal in the animation advance (2.5)

`advance_hero_animation` (`hero_mapped_animations.c`) moves a looping sequence:
- from key `frame` (moby `+0x50`) to `prev_frame` (`+0x51`), at `anim_time` (`+0x54`, 0..1);
- by `anim_speed` (hero `+0xA90`) × `anim_frame_rate` (`+0xA94`, the key's rate, the first float of
  its frame data);
- moving the keys on when it passes 1;
- and fires the sequence's sound triggers it crosses.

Sequences: the class `+0x48`, each record's `frame_count` at `+0x10`, `frames[]` at `+0x1C`, the
triggers after them.

Only a looping walk or run with nothing pending is stepped back: `seq == prev_seq` (3 or 4), no blend
curve, no queued frame, no loop frame. The mod:
1. moves `anim_time` back by that step, moving the keys back past 0;
2. fires the triggers it crossed (`Moby_PlaySound`, an executable function);
3. calls the original with `anim_speed` 0 for the call, so it moves nothing and does the rest.

## 5. Addresses (PAL v2.00 SCES-50916, NTSC-U v1.00 SCUS-97199)

PAL's are rac1-decomp's own (`include/names/*.h`; OpenRAC's PAL build names functions as rac1-decomp
does). NTSC-U's are the same globals and functions at the US addresses.

| | PAL | NTSC-U | Read | NTSC-U's source |
|---|---|---|---|---|
| hero `g_hero` | `0x0013F450` | `0x0013F350` | as is | `rac1_version.h` |
| pad `g_controller_state` | `0x0013CA40` | `0x0013C940` | as is | `rac1_version.h`, `pad.h` |
| `g_frame_scale_sq`, `g_frame_time`, `g_frame_time_sq` | `0x0015EE64`, `0x0015EE6C`, `0x0015EE70` | `0x0015ED64`, `0x0015ED6C`, `0x0015ED70` | relocate | `rac1_version.h`, `names_ntsc.tsv` |
| `frame_scale` (`scale_ticks`'s factor) | `0x0015EE60` | `0x0015ED60` | relocate | `rac1_version.h` |
| `cinematic_mode` | `D_L00_0015F6A8` | `0x0015F5C4` | relocate_level(0) | `rac1_version.h` |
| `camera_yaw` | `D_L00_00166ED8` | `0x00166DD8` | relocate_level(0) | NTSC decomp `stick_target` (`FUN_L00_002118c8`) reads `D_L00_00166DD8` |
| `g_current_follow_cam` | `D_L00_00166F00` | `0x00166E00` | relocate_level(0) | `rac1_version.h`, `names_ntsc.tsv` |
| `follow_cam_turn_blend_weight` | `D_L00_0015F040` | `0x0015EF40` | relocate_level(0) | NTSC decomp `follow_camera_setup.c` |
| `hero_eases` (pose table) | `D_L00_0017A780` | `0x0017A680` | relocate_level(0) | `names_ntsc.tsv` |
| `set_follow_camera_orbit_clamp_mode` | `func_L00_002E9A18` | `FUN_L00_002e8568` | function | `us_map.tsv` (every level) |
| `follow_camera_start_slide` | `func_L00_002E9AF8` | `FUN_L00_002e8648` | function | `us_map.tsv` (every level) |
| `follow_camera_set_lateral_spring` | `func_L00_002E9A40` | `FUN_L00_002e8590` | function | `us_map.tsv` (every level) |
| `Moby_PlaySound` | `func_0022ED80` | `FUN_0022da68` | function | `us_map.tsv` (executable) |

**PAL's 50 Hz** (`set_video_timing`: `g_frame_time` 1/50, `g_frame_scale_sq` 1.44, `frame_scale`
1.2). OpenRAC does not run it today: it keeps the PAL game in its 60 Hz mode and ticks every game 60
times a second (`openrac_game_set_video_mode` in `port/game/common/program/game_host.cpp`, which logs
"PAL 50 Hz: the port keeps 60"). So on OpenRAC the PAL disc plays with NTSC-U's timing. The mod reads
the timing the game set each tick, so it is right in either mode; the unit and load tests check the
50 Hz path. rac2's per-tick numbers become this tick's the way rac1 scales its own: speeds and turn limits
by 60 × `g_frame_time`, accelerations by 3600 × `g_frame_time_sq`, turn spring coefficients by
`g_frame_scale_sq`. That is exactly rac2's own PAL release: its strafe turn there is
`func_L00_002B3EF8(0, 0.053280003, 0.36, 0.24434614)` (rac2-decomp `src/pal/levels/shared/L00_002B3210.c`),
0.037 × 1.44, 0.25 × 1.44 and 0.2036 × 1.2.
- **rac1's own tick counts** (the jump windows 9 and 7, the run's 4 ticks in the air, its 25 ticks
  to idle, the flip's 12 before landing) are counted as rac1 counts them: `scale_ticks(n)`
  (`func_001F9850`) = `n × frame_scale + 0.5`, cut. That is 11, 8, 5, 30 and 14 on PAL.
- **rac2's own counts** (the strafe's 5-tick side timers, the stick's swing 1 and 7 polls back) stay
  as they are. rac2's PAL release keeps them (the same `L00_002B3210.c`, and `func_L00_0032F038(1)`,
  `(7)` throughout `src/pal`).

Hero offsets (`include/structs/hero.h`; seen in the code cited):

| Field | Offset | Field | Offset |
|---|---|---|---|
| facing (`rot.z`) | `+0x98` | `momentum` | `+0x150` |
| `speed_xy` | `+0x160` | `flat_speed` | `+0x164` |
| `move_yaw`, `yaw_rate`, `yaw_error` | `+0x180`, `+0x184`, `+0x188` | `target_speed`, `ground_speed`, `state_timer` | `+0x190`, `+0x194`, `+0x198` |
| `unk308` | `+0x308` (s16) | `unk3B8` (along the stick), `unk3BE` (turnaround) | `+0x3B8`, `+0x3BE` (s16) |
| `past_jump_apex` | `+0x41E` (s16) | `takeoff_frame` | `+0x420` |
| `air_stick_speed` | `+0x424` | `frames_to_land` | `+0x44C` |
| `air_target_speed`, `high_jump_drift_speed` | `+0x454`, `+0x458` | `unk930` | `+0x930` |
| `anim_speed`, `anim_frame_rate` | `+0xA90`, `+0xA94` | `anim_blend_curve`, `anim_loop_frame`, `queued_frame_request` | `+0xAA0`, `+0xAB0`, `+0xAB8` |
| item slot i: state, id | `+0x10B4 + i·0x50` (2 in hand), `+0x10B8` | `on_ice_ground` | `+0x12E2` (u8) |
| stick x, y | `+0x1D20`, `+0x1D24` | moby | `+0x2080` |
| state, step, control mode, prev | `+0x2084`, `+0x2088`, `+0x208C`, `+0x2090` | vehicle mode, item flag, wading, `unk20AA`, gravity mode | `+0x20A4`, `+0x20A8`, `+0x20A9`, `+0x20AA`, `+0x20B3` (u8) |
| stick amount | `+0x229C` | | |

Pad (`pad.h`): `analog` `+0x100`, `held` `+0x1A0`, `pressed` `+0x1A4`, `released` `+0x1A8`,
`prev_held` `+0x1AC`, `raw_held` `+0x1B0`, `raw_pressed` `+0x1B4`, `raw_released` `+0x1B8`,
`hist_idx` `+0x18E` (s16), `hist_count` `+0x190`, `hist_btn` `+0x1E0`, `hist_ang` `+0x258`,
`unk348` `+0x348`. Moby: `pclass` `+0x24`, `frame` `+0x50`, `prev_frame` `+0x51`, `seq` `+0x52`,
`prev_seq` `+0x53`, `anim_time` `+0x54`, `cur_frame_data` `+0x68`, `prev_frame_data` `+0x6C`.
Camera record: vars `+0x70`, kind `+0x86`.

## 6. Composition with other mods

- **difficulty** (`approach_speed`) scales the target in memory and `up`/`down` by its factor, then
  puts the target back. The mod reads that factor from `down` against rac1's own `down` for the state
  (8.5, or 17 with `unk3BE`). It applies the factor to rac2's target, `up` and `down`, and puts back
  the target it found, so difficulty's own restore still works, in either load order.
- **lock-strafe** takes L2/R2 and the same hook points: the manifest declares the conflict.
- `turn_to_heading` calls inside the hero's update in a state the mod handles are the state's own
  call (one per tick in every level copy). The mod gives rac2's values whatever came in, and the
  trace shows what came in.
