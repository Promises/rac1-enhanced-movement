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
**`rac1.camera.follow.apply_overrides`**), then turns by the right stick in the same update (8.1).

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
| The animation's frame and blend flag (the wrench, 5.2) | `advance_animation`, before its original works them out again | whenever the field still holds what the mod wrote, the state changed or not |
| Pad fields (R1 in `held`; a stick press or a Square added to the press history, a Square or X hidden from it) | `process_report`, before the next poll processes its report | always |

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

## 5. Wrench (setting `wrench`)

rac2's wrench is rac1's code carried on. Both games have the same three states: the combo (0x13),
the slam, also called the hyper-strike (0x14), and the crouch swing, which throws the wrench (0x15,
rac2's comet strike). Each state moves the hero with the same numbers in both games, and each hit
is built the same way. What differs is when a swing chains, ends or can be left: the attack records'
frames, the way rac2's decider ends a swing, Square's windows, the lock-on turn and the slam's drive.
The mod ports those and keeps rac1's damage and hit windows (section 5.7).

Sources:
- rac1: `hero_update_movement` cases 0x13 to 0x15 (`hero_water_and_ground_motion.c`),
  `process_hero_equipped_item_input` (`func_L00_00228770`, `hero_vehicle_and_collision.c`),
  `hero_update_transitions` (`hero_item_input_and_hud.c`), `HeroEnterMoveState`'s case 0x13 to 0x15
  and `launch_hero_wrench` (`func_L00_00217DE0`).
- rac2: `func_L00_002BA7D8` cases 0x13 to 0x15, `func_L00_002C9448` (`L00_002C9448-g8.c`, "RAC1's
  process_hero_equipped_item_input is the counterpart"), `func_L00_002CB3A8` cases 0x13, 0x14 and
  0x15, `func_L00_002C42E0`'s case 0x13 to 0x15 and `func_L00_002BA508` (the throw).

### 5.1 The attack records

`attack_records` (rac1, 0x2C bytes a record, indexed by `attack_record` at `+0xA60`) and
`D_L00_0025D990` (rac2, `+0xBFC`) hold, for each swing, the animation frames the transitions compare
with. The fields that matter here:
- `+0xC` **first**: the windows count from it, `(frame - first) × 2` polls;
- `+0x10` **open**: Square chains the next swing from this frame;
- `+0x14` **jump**: X cancels into a jump after this frame;
- `+0x18` **exit**: the swing ends after this frame.

Records 0 to 2 are the combo's three swings and 3 is the throw. In both games, 0x13's entry takes
record `(previous + 1) mod 3` while chaining, so the combo has three swings (animations 0x17 to 0x19).

| Record | rac1 first / open / jump / exit | rac2 first / open / jump / exit |
|---|---|---|
| 0, first swing | 19 / **26** / 24 / **31** | 19 / **25** / 24 / **26** |
| 1, second swing | 7 / 12 / 13 / **23** | 7 / 12 / 13 / **15** |
| 2, third swing | 16 / 23 / 19 / 21 | the same |
| 3, the throw | 83 / 88 / **87** / 91 | 83 / 88 / **35** / 91 |
| 4, the slam | 32 / 28 / 28 / 30 | the same |

Evidence:
- **rac1**: these are level data, not literals. They were read from the PAL disc's level programs
  (rac1-decomp's `tools/extract` reader; `attack_records` `D_L00_0017BD28` and its 13 named level
  copies, `novalis_attack_records` and the others), and all 14 are the same. On NTSC-U they were read
  in the running game (the loaded level's copy of `0x0017BC28`, through `relocate_level_data`) on
  levels 0, 1, 3, 4, 7, 9, 10 and 13: the same values.
- **rac2**: level 0's data at `D_L00_0025D990` (`0x25D910` on the v2.00 disc whose dump this was read
  from: `func_L00_002C9448` forms it with `lui`/`addiu` at `0x2C9A5C`/`0x2C9A68`, `0x80` below v1.01's).
  `tools/rac2_constants.py --data` reads them into `tests/data/rac2_constants.tsv`.

rac2's combo entry adds one rule. The combo also continues (record `(b + 1) mod 3`) when the state
before the last was 0x13 and both states together lasted under 27 ticks (`WORD(0x22AC) == 0x13 &&
WORD(0x1B8) + WORD(0x22A8) < 27` in `func_L00_002C42E0`), which carries the combo through a short
run. rac1's entry wants its previous control mode to be the attack's (`prev_control_mode == 6`).
The mod leaves this out (5.8).

### 5.2 How a swing ends

`func_L00_002CB3A8` cases 0x13 and 0x15 against `hero_update_transitions` case 0x13/0x14/0x15. Each
game first runs its item code (`func_L00_002C9448` / `process_hero_equipped_item_input`), and Square
there chains the next swing, from the open frame, within `(frame - first) × 2` polls (at least 1,
at most 15). rac2 takes no Square while the wrench is out (its `input_locked`, `+0x123A`) or on slip
ground.

| In order | rac1 | rac2 |
|---|---|---|
| X | after the jump frame, within `(frame - first) × 2` polls: the jump (with R1 its crouch jump) | the same; in the throw within **8** polls (`frames = 8`) |
| A new animation blending in (`anim_changing`, rac2 `+0xC2C`) | nothing else | nothing else |
| The stick past 0.9 after frame 34 | — | **the run** (`func_L00_002C42E0(2, 0)`) |
| After the exit frame | **standing at once** (`HeroEnterMoveState(0, 0)`) | the stick past 0.8: **the run**; else standing **5 frames later** (`lim + 5`), or when the animation ends (`+0xC28 & 2`) |

Together with 5.1:
- **The combo.** The first swing chains from frame 25 instead of 26. With the stick pushed, it runs
  out at frame 26 instead of standing at 31, the second swing at 15 instead of 23, the third at 21.
  Without the stick, the second swing stands at 20 instead of 23, and the third waits until 26
  instead of 21.
- **The throw.** With the stick fully pushed, it runs past frame 34, just after the wrench leaves
  at 33. X jumps out past frame 35 (rac1 not before 88, after the catch). With no input, it stands
  at 96 instead of 91.

**The mod** works out, in `update_motion_input`'s post-step (just before rac1's transitions), what
rac2 would do this tick (`bm_swing_rac2`) and what rac1 will do (`bm_swing_rac1`), and makes rac1's
own code do rac2's (`bm_swing_steer_for`, `src/rac1/wrench.h`):

| rac2 | rac1 would | Steer for the tick |
|---|---|---|
| chain | stay | `anim_frame` (`+0xAA8`) set to rac1's open frame (26 for 25.x). rac1's window there takes the same press |
| jump | stay | `anim_frame` set to the first frame past rac1's jump frame whose window takes the press (88 for the throw). Square is hidden for 15 polls, so 88 chains nothing |
| run or stand | stay | `anim_frame` set just past rac1's exit (31.5, 23.5, 91.5). Square (15 polls) and X (the whole history) are hidden, since rac1's windows there are longer than rac2's now. rac1 stands. rac2's run follows on the next tick from idle, which starts the run past 0.22 of stick |
| run | stand | nothing: idle runs on the next tick |
| stay | stand or jump | `anim_changing` (`+0xA9C`) set to 1: rac1 then leaves the swing alone this tick |

Put back:
- `anim_frame` and `anim_changing` in `advance_animation` before its original runs, even when the
  state changed. The advance works both out again from the moby; the old `anim_frame` only feeds
  `anim_frame_delta`.
- The hidden presses in `process_report`, before the next poll.

What else reads them in between, from the transitions to the next tick:
- `update_hero_equipped_items`: the wrench's hit test runs only in the attack's control mode, and
  the steered frames are past each record's hit frames (`+0x1C` to `+0x20`: 17 to 23, 8 to 13, 12 to
  16; the throw's record has none, 99).
- `dispatch_hero_animation_timed_events`: only the walk and run sequences.

The same checks are in every family's copy:
- `process_hero_equipped_item_input`: `func_L00_00228770` in levels 0, 3, 4, 7, 9, 10, 13 and 17,
  `func_L02_0022F568` in 2, 6, 8, 11, 12, 14, 15 and 18, `func_L05_002523C8` in 5 and 16, and
  Novalis's `handle_hero_gadget_button`. All use 7, 10, 18, 15, 30 and `(frame - first) × 2`.
- `hero_update_transitions`: every level copy has the same jump and exit tests. Novalis's, Kerwan's
  and the Fleet's go straight to the jump with R1 held, where the shared one asks
  `hero_handle_jump_press`.

**The wrench in flight.** Leaving the throw before the catch is new to rac1's throw, but not to its
code. The first-person throw from the look state (`launch_hero_wrench` from state 1/0x1E) leaves the
hero free while the wrench flies. Every state's entry with an animation calls `set_hero_animation`,
which clears the queued loop (`clear_hero_queued_animation`), and so does the idle state each tick.
So the returning wrench (`bolts_and_wrench.c`, state 11: `selected_weapon != -1`) queues no catch
frame on another animation. It arrives and clears `thrown`. rac2 does the same (`i[0xC40] != -1`).
The play-test ran and jumped with it out (PLAYTEST.md).

### 5.3 Square's windows

| | rac1 (`scale_ticks`) | rac2 |
|---|---|---|
| Standing or running (`control_mode < 2`, or the jump's on a rail) | 7 | **10** (`func_L00_0032EF10(mask, 10, …)`) |
| In a jump after its 10th tick: jumps, landing, falling | 10 | **14** (`buffered_window = 14`) |
| The double jump past frame 32, the high jump past 18 (20) | 18 | 18 |
| Crouching (the throw) | 15 | 15 |
| While the wrench is out | the stand/run window only (`m->state != 0`); the air one slams and the crouch throws again | none (`input_locked`) |

The mod (post-step, `steer_wrench`):
- With the wrench in hand, ready, a Square that rac2's window takes and rac1's does not is shown to
  rac1's item code as pressed this poll: the bit is added to the newest history slot for the tick.
- While `items[0].thrown` (`+0x10AA`), Square is hidden for 18 polls each tick.
- **The flips.** Square and X together while strafing: rac2's item code runs first and swings. The
  flip's steer shows rac1 R1 held, which would turn that Square into the throw. So the flip is not
  steered in a tick whose Square rac1's item code takes, with or without the wrench setting.

### 5.4 The lock-on turn

While a swing, the slam or the throw has a target or a locked heading, the hero turns toward it:
- rac1: `turn_to(0, 0.05, 0.2, 15.009831 × g_frame_time)` in all three states;
- rac2: `func_L00_002B3EF8(0, 0.05, 0.2, 0.2821616)`, which is **16.93 rad/s**; the throw's accel is
  **0.057**.

The mod: `turn_to_heading` in states 0x13 to 0x15 recognises rac1's numbers (within 1%) and passes
rac2's, each argument scaled by rac2's over rac1's. Every level copy has the 15.009831 turn in these
states (8 to 10 occurrences per copy, counted with grep).

Steering without a target is the same in both games: `update_hero_target_heading(5, 45°, 45°)` in the
combo, (14, 30°, 45°) in the throw and (8, 60°) in the slam. rac2's `func_L00_002B75A0` is the same
function. A stick past 0.5 locks the swing's heading to it.

### 5.5 The slam's drive

`func_L00_002BA7D8` case 0x14, off ice: with a target 2 to 5 away, both games drive at 4.5 u/s
(rac2 `0.075`). rac2 also drives at 4.5 when more than **4** above the ground (`f[0x31C/4] > 4.0f`),
with or without a target, and without one only for the slam's first **70** ticks (`0x46`). It does
the same on some ground flags (`+0x1495`, `+0x1496`, `+0x149B`).

The mod: `approach_speed` in state 0x14, off ice, more than 4 above the ground (`ground_distance`,
`+0x2DC`) and toward a target or under 70 ticks raises the target speed to 4.5 for the call and puts
rac1's back. The ground flags are left out: rac1's matching bytes (`+0x12E5`, `+0x12E6`, `+0x12EB`)
are unnamed, so their meaning is not known.

### 5.6 Counts at 50 Hz

rac2's PAL release counts its pad windows in its own 50 Hz polls: `(n × 5 + 2) / 6`. Its deciders in
`src/pal` give the throw's X window as 7 (8) and the crouch's Square window as 12 (15)
(`L03_002BF860-sw.c`). rac1 counts its own as `scale_ticks` does, `n × 1.2 + 0.5`.

The mod counts each game's as that game does (`bm_rac2_ticks`, `bm_scale_ticks`): rac2's 10, 14, 18,
8, 15, 31 and 70 against rac1's 7, 10, 18, 15 and 30. At 50 Hz both games' Square windows come out
equal (8 and 12; rac2's late one is shorter, 15 against 22), so nothing is buffered there, and the
throw's X window is 7.

OpenRAC runs PAL at 60 Hz (section 6), so this path only runs in the unit and load tests.

### 5.7 The same in both games (nothing to port)

- **Movement in the swings**: the lunge per swing kind (4.4; 5.7, 4.5 and 3.0; 3.7 u/s, times
  `speed_scale`). The reach toward a target (offsets 0.75, 0.6 and 1.3, ranges 0.8 and 1.7, caps 4,
  4 and 1.9, divided by 0.9). `speed_step(37, 28)`, gravity 25 and 54, the animation speed easing to
  1 by 0.2.
- **The slam**: rise 7.7 (× 0.5 and × 0.7 high up), 150 u/s², 9 ticks; gravity 130 after 14;
  falls at most 37 u/s (rac2 70 in one mode, `D_L20_001A8FF0`, not mapped). Its hit is
  `0x10000`, 2.0, 5627.925, class `0x47`, from frame 23.5. Its entry is the same as rac1's
  (record 4, animation 0x2B, 11 or 17 frames of blend).
- **The throw**: launched at frame 33. The wrench leaves at 23 u/s; its slowing grows by 170 u/s³;
  the return speeds up by 0.9 u/s²; it spins at 24.43 rad/s (rac2 `0.38333335`, `0.000787037`,
  `0.00025`, `0.40724355` in `L00_0038BF18-g8.c`). The first-person throw is the same too.
- **Damage and hit windows**: the wrench's hits are in its moby update (`bolts_and_wrench.c`), the
  slam's descriptor is in the state. The mod touches neither, so rac1's enemies take rac1's hits.
- **rac2 only**, left as rac1's:
  - the outward wrench carries 0.98 of the hero's momentum (rac1 0.7 of his velocity);
  - it drops back to his hand when more than 32 away;
  - its ledge test in the swings probes 0.35 down (rac1's
    `stop_hero_motion_without_forward_support` 0.2).

  All three sit inside functions no hook point reaches, or only `rac1.moby.wrench.update`, which
  the SDK marks experimental.
- **The throw's button**: rac1 throws with R1 or R2 held (`held & 0xA`), rac2 with R1. With the
  strafe on R2 (`both`, `r2`), R2 is already the strafe's, as in rac2. With `l2` or `off`, R2 stays
  rac1's crouch, and its throw with it.

### 5.8 Left out

- The combo continuing through a short other state (5.1). rac1's choice of the next swing is inside
  `HeroEnterMoveState`, from the previous control mode. Showing it the attack's mode for a tick would
  also change which branch rac1's item code takes. No hook point sits in the state entry.
- The thrown wrench's carry and its 32-unit recall, and the 0.35 ledge probe (5.7).
- The slam's ground flags (5.5).

## 6. Addresses (PAL v2.00 SCES-50916, NTSC-U v1.00 SCUS-97199)

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
| `opt_cam_yaw_rate` (the camera speed option, 8.1) | `0x0015EEE4` (`follow_camera_update`'s `vendor_stock[5]`) | `0x0015EDE4` | relocate | NTSC decomp `rnc/game/options.h` |
| `follow_cam_setting_value` (the turn's gain) | `D_L00_0015F044` | `0x0015EF44` | relocate_level(0) | NTSC decomp `follow_camera_update.c` |
| `follow_camera_pitch_input_tracking_step_limit`, `_return_step_limit`, `follow_camera_pitch_angle_step_limit` | `D_L00_00161D8C`, `D90`, `D94` | `0x00161CBC`, `CC0`, `CC4` | relocate_level(0) | NTSC decomp `follow_camera_turn_and_pitch_globals.h` |
| `orbit_yaw_spring_stiffness_gp`, `_damping_gp`, `orbit_pitch_spring_stiffness_gp`, `_damping_gp` | `D_L00_00161DC4`, `DC8`, `DCC`, `DD0` | `0x00161CF4`, `CF8`, `CFC`, `D00` | relocate_level(0) | `names_ntsc.tsv` |

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
  `(7)` throughout `src/pal`). The wrench's pad windows are the exception: rac2's PAL release
  shortens those to `(n × 5 + 2) / 6`, and the mod counts them so (5.6).

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
| `ground_distance` | `+0x2DC` | `rail_land_frames`, `unk450` | `+0x418`, `+0x450` |
| `melee_target`, `attack_record` | `+0xA54`, `+0xA60` | `anim_flags`, `anim_changing`, `anim_frame` | `+0xA98`, `+0xA9C`, `+0xAA8` |
| item slot 0: moby, thrown | `+0x1090`, `+0x10AA` (u8) | `on_slip_ground` | `+0x12EA` (u8) |

The wrench needs no new address: the hero's and the pad's offsets above, and rac1's attack records
as constants (5.1), the same in every level.

Pad (`pad.h`): `analog` `+0x100`, `held` `+0x1A0`, `pressed` `+0x1A4`, `released` `+0x1A8`,
`prev_held` `+0x1AC`, `raw_held` `+0x1B0`, `raw_pressed` `+0x1B4`, `raw_released` `+0x1B8`,
`hist_idx` `+0x18E` (s16), `hist_count` `+0x190`, `hist_btn` `+0x1E0`, `hist_ang` `+0x258`,
`unk348` `+0x348`. Moby: `pclass` `+0x24`, `frame` `+0x50`, `prev_frame` `+0x51`, `seq` `+0x52`,
`prev_seq` `+0x53`, `anim_time` `+0x54`, `cur_frame_data` `+0x68`, `prev_frame_data` `+0x6C`.
Camera record: vars `+0x70`, kind `+0x86`. The follow camera's vars (8.1): the turn's block at
`+0x1A8` (the eased stick x `+0x1A8`, the pitch's aim `+0x1B0`, its flags `+0x1B8`, the gain
`+0x1BC`, a swing's input `+0x1C4`), the orbit `+0x130`, the eye `+0x140`, its yaw and pitch spring
steps `+0x150`, `+0x154`.

Every camera number the mod reads was also read in the running game on both discs
(`OPENRAC_WATCH`, PLAYTEST.md): the speed table 1.0 / 1.3 / 1.6 degrees, the option 1, the pitch's
steps 0.02 / 0.01 / 0.0305, the springs 0.015 / 0.2, on PAL v2.00 and NTSC-U alike.

## 7. Composition with other mods

- **difficulty** (`approach_speed`) scales the target in memory and `up`/`down` by its factor, then
  puts the target back. The mod reads that factor from `down` against rac1's own `down` for the state
  (8.5, or 17 with `unk3BE`). It applies the factor to rac2's target, `up` and `down`, and puts back
  the target it found, so difficulty's own restore still works, in either load order.
- **lock-strafe** takes L2/R2 and the same hook points: the manifest declares the conflict.
- **wrench-throw** (openrac-mods index; read from its manifest and README only) takes R2 on foot in
  `rac1.pad.process_report` and throws from `rac1.player.update`. Both mods want R2 at their
  defaults: whichever runs its pad hook second sees R2 or not, so one of the two R2 uses does
  nothing. They can run together with this mod's strafe on L2 only, as wrench-throw's README
  asks of lock-strafe. That is a setting, not a reason to forbid both, so the manifest declares no
  conflict and the README says to set L2. Its throws while moving are rac1's own first-person throw,
  and this mod's wrench rules hold around them: no Square counts while the wrench is out (5.3).
- **accessibility-pack** (hold-to-mash Square) presses Square repeatedly: each press is a press to
  rac1's code and the mod alike. **damage-numbers** reads the hits, which the mod leaves rac1's.
- `turn_to_heading` calls inside the hero's update in a state the mod handles are the state's own
  call (one per tick in every level copy). The mod gives rac2's values whatever came in, and the
  trace shows what came in.
- **modern-controls** (openrac-mods index; read from its manifest and README only) takes
  `rac1.camera.follow.apply_overrides` too and drives the right-stick camera itself: its own speed
  (240 degrees a second by default), a radial dead zone, and a 1 to 100 row that replaces the game's
  camera speed row in Options. With both mods choosing the speed, the result would depend on the load
  order. They run together with this mod's `camera` set to `rac1`, which takes nothing of the camera's
  turn (the strafe camera's requests stay, and compose as before). That is a setting, so the manifest
  declares no conflict and the README says to set it. It also takes `rac1.player.approach_speed`
  for its movement speed and response; how that composes with this mod's was not tested.
- **steam-deck-pack** (the same, manifest and README only) takes no hook point: it turns Steam Input's
  gyro and right trackpad into the game's right stick. The game's camera turn then reads that stick,
  so this mod's `camera` choice applies to the gyro and trackpad as to the stick (three times as fast
  under `modern`). They work together; its own sensitivity setting adjusts the gyro on top.
- No other mod in the index names the camera or the right stick.

## 8. The camera (setting `camera`)

### 8.1 How rac1 turns the follow camera

Each tick the follow camera's moby runs `follow_camera_update` (rac1-decomp
`camera_modes_and_scripts.c`; NTSC `FUN_L00_002e9c28`):

1. `follow_cam_setting_value` = `follow_camera_yaw_rate_table[opt_cam_yaw_rate]`;
2. `follow_camera_apply_overrides`: hook **`rac1.camera.follow.apply_overrides`**;
3. `follow_camera_springs`, then `update_follow_camera_pose`: `follow_camera_auto_turn`,
   `follow_camera_read_stick`, `follow_camera_turn_and_pitch`, `follow_camera_avoid_mobies`,
   `follow_camera_orbit`, `follow_camera_build_view`;
4. `follow_camera_approach`, which resets the per-tick requests (the gain at `+0x1BC` to
   `follow_cam_setting_value`, a swing's input at `+0x1C4` to 0).

| | rac1 | Evidence |
|---|---|---|
| The stick | the right stick, `analog[0]` (x) and `analog[1]` (y) of the first pad. Not L1/R1 or the d-pad | `follow_camera_read_stick` (`func_L00_002E9E60`) |
| Dead zone | per axis: nothing within 0x30 of 0x7F (37.8%), then linear to full at 0x7C (`(d - 0x30) / 0x4C`) | `process_pad_input` (`func_002181F0`) |
| Yaw | rate control: each tick the orbit turns by gain × x, about the camera's up axis. x is eased by `cam_interp_values` at stiffness 1 and damping 1 (`follow_camera_input_stiffness` / `_damping`, `D_L00_00161D7C` / `D80`), which lands on x in one tick: no input ramp | `follow_camera_turn_and_pitch` |
| The gain | `follow_camera_yaw_rate_table` (`D_L00_00161E80`; NTSC `0x00161DB0`): **1.0, 1.3, 1.6 degrees a tick**, picked by the camera speed option in Options, Camera (`opt_cam_yaw_rate`, three steps, **1 by default**). 60, **78**, 96 degrees a second at full stick | read in the running game; the NTSC decomp declares two entries, the menu has three steps and the data three entries |
| The eye | follows the orbit through `rope_spring_turn` at 0.015 / 0.2 for yaw and for pitch (`orbit_yaw_spring_*`, `orbit_pitch_spring_*`). That is the only ramp: half the stick's rate after 11 ticks, 90% after 24, about 0.19 s of turn coasted after letting go (15 degrees at 78 a second), stopped in 30 ticks | `follow_camera_orbit`; measured, PLAYTEST.md |
| Pitch | position control: y past 0.3 (rescaled by 1/0.7) aims the orbit's pitch within ±40 degrees. The aim moves 0.02 a tick outward, 0.01 back (`_tracking_step_limit`, `_return_step_limit`), the orbit at most 1.75 degrees a tick toward it (`_angle_step_limit`). Let go, it returns to level at about 24 degrees a second. The same steps level the orbit after anything else tilts it (a jump, a slope) | `follow_camera_turn_and_pitch` |
| Invert | the options `opt_cam_invert` (x) and `invert52` (y) flip the sign | `follow_camera_read_stick` |
| Turned off | the camera's flags (`+0x1B8`): 1 stops the stick's turn, 2 its pitch | the same |
| Swings | the game's own swings (behind him at a ledge, toward a point of interest, a level's) write the gain at `+0x1BC` and an input at `+0x1C4`, which count only while the stick is centred. One caller (`trespasser_pad_and_background_ships.c`) passes no gain and so swings at the camera speed's | `set_follow_camera_angle_blend_from_direction`, `_from_position` |
| 50 Hz | nothing is scaled: the PAL v2.00 disc has NTSC's table and springs, so a 50 Hz game turns 5/6 as fast (65 degrees a second by default) | read on the PAL disc |

**First person** (L1 or L2 held: rac1's look state 1) is another camera,
`update_hero_aligned_camera_from_input` (NTSC `FUN_L00_002e9f38`). It reads the left stick first, then
the right, then the d-pad, eases the input at 0.03 / 0.4, and turns by `0 + 0.0262 × |input|` radians
a tick times the input: a squared curve, 1.5 degrees a tick (87 a second measured) at full stick, 9.5 a
second at a third. Its pitch eases off past 64 degrees.

**Strafing** with this mod keeps the follow camera: the right stick turns it and Ratchet turns to face
it (4.2), with the requests rac1's own state 0x81 makes. **Other cameras** (a level's volume and path
cameras, the hoverboard's, the ship's, the grind rails') are other camera types with updates of their
own; the setting acts only in the follow camera's overrides. **Clank and Giant Clank** use the follow
camera (its overrides set their distance and height, by vehicle mode).

### 8.2 rac2

The same four functions, carried over (rac2-decomp names, the v2.00 disc's addresses for its data):
`func_L00_004263B8` (`follow_camera_update`), `func_L00_004224D0` (the overrides),
`func_L00_00424F98` (`follow_camera_read_stick`, identical but for the state block at `+0x1C0`),
`func_L00_00425118` (`follow_camera_turn_and_pitch`), `func_L00_00425CD0` (`follow_camera_orbit`),
and the pad's `func_002B7E28` (rac1's dead zone, to the byte).

| | rac2 | Against rac1 |
|---|---|---|
| The gain | `D_L00_001B15B8` (v2.00 `0x1B1558`): **1.0, 1.3, 2.2 degrees a tick**, by its camera speed option `D_001A7BAC` (v2.00 `0x1A7BB4`; three steps, `(mode + 1) % 3` in its menu, **1 by default** in the boot data and in the port with no card) | the same but the fastest: 132 degrees a second against 96 |
| Input easing | 1.0 / 1.0 (`0x1B144C`, `0x1B1450`) | the same |
| Pitch | 0.3 dead zone, ±40 degrees (`0x1B1464`), steps 0.02 / 0.01 / 1.75 degrees (`0x1B1458` to `0x1B1460`) | the same |
| The eye's springs | yaw 0.015 / 0.2 (`0x1B1488`), pitch 0.015 / 0.2 (`0x1B1490`); distance 0.02 / 0.2, pulled back 0.04 / 0.3 | the same |
| First person | `func_L00_004266E8`: gain 0 to 0.0262 (`0x1B1568`, `0x1B156C`), spring 0.03 / 0.4 | the same |

Measured in OpenRAC's rac2 port (`openrac-rac2-ntsc2`, Oozla, PLAYTEST.md): 77.5 degrees a second at
full stick, half after 11 ticks, 90% after 24, 16 degrees coasted; 25.7 at a third of the stick; 131
on the fastest step; first person 87.4. Each number is the first game's, or the mod's `rac2` on it.

So **at the games' default camera speed, Going Commando turns the camera exactly as the first game
does.** The one difference in the turn itself is the fastest step. The setting's `rac2` therefore
takes that fastest step on every step of the game's option (the user's choice, PLAN.md section 10),
so the three choices rise one above the other: 78, 132 and 234 degrees a second by default.

Differences around the turn, reported and left as rac1's (none is the turn's speed or feel):
- **Pulled round behind him.** rac2's overrides write 0.85 where rac1's write 0.75 to the blend weight
  `follow_camera_auto_turn` reads (`D_L00_001A8900` / `follow_cam_turn_blend_weight`): its camera is
  dragged round a little more as he moves. They also write a rate at `+0xE4` of the target block each
  tick (0.002 up to `D_L00_001B14A4`, by a value at `+0xF0`), a write rac1's overrides do not make.
- **Points of interest.** Where a level names one, rac1's swing toward it restarts its 400-tick ramp
  as soon as the right stick moves past 0.3. rac2 waits 240 ticks (`targetTicks` set to 640, counted
  down to 400) before swinging again, and ramps its swing's gain to 1.571 where rac1's ramps to 0.209.
- Neither game limits the yaw or snaps it back behind him when the stick lets go. Both return the
  pitch to level.

### 8.3 The mod

No hook point reaches inside `update_follow_camera_pose`. `apply_overrides` runs after step 1 and
before step 3, so the mod sets there what the rest of the update reads, for that update only:

| Choice | What the update reads | Put back |
|---|---|---|
| `rac1` | nothing: the hook is not taken for it (with the strafe off) | |
| `rac2` | `follow_cam_setting_value` × rac2's fastest entry (2.2 degrees a tick) over rac1's entry for the option: 132 degrees a second on every step (2.2 for 1.0, 1.3 or 1.6). Another mod's scaling of the gain survives (the ratio); an option rac1's table does not have keeps the gain | rac1 sets it again from its table at its next update; the mod's pad hook also puts it back before the next tick |
| `modern` | the gain × 3 × 60 × `g_frame_time`; while the pitch is the player's (the stick's y past 0.3, or its aim at `+0x1B0` not back at 0), the pitch's three steps × the same; while the stick turns the camera and 20 ticks after, the eye's two springs at 0.2 / 0.7 (× `g_frame_scale_sq` and × `frame_scale` at 50 Hz) | the pad hook (`rac1.pad.process_report`, before the next tick) and the hook's own start: each word that still holds what the mod wrote, in the level whose program it is in |

**Why these numbers for `modern`** (PLAN.md section 10): three times the game's own gain is 234
degrees a second at the default step (180 and 288 at the others): three times both games' default,
1.8 times Going Commando's fastest. The pitch's steps scale with it. Three times the gain on rac1's
eye spring would ramp as slowly (90% after 24 ticks) and coast three times as far (48 degrees after
letting go). So while the stick turns the camera, the eye follows at 0.2 / 0.7: 90% of the stick's
rate within 6 ticks, about 10 degrees coasted, stopped within 10 ticks. That is a short ramp, not a
snap, and the stop stays crisp. `tests/camera_test.c` works both springs through `rope_spring_turn`
and gets the play-test's numbers.

Left out:
- **First person** stays the game's in every choice. Both games have the same numbers there, so
  `rac2` has nothing to change. It is an aiming view with its own curve, and its gain is a level
  constant read by a camera no hook point brackets.
- **The dead zone** stays rac1's (37.8% per axis, as rac2's). A smaller one would need the stick
  rewritten for the whole game, not only the camera.
- **The differences around the turn** (8.2) are not ported: they are not the turn's speed, and the
  point-of-interest swing's numbers are level design.

The setting changes nothing else: with `rac1` every route plays tick for tick as before, and with
`rac2` and `modern` the game's camera is untouched until the right stick moves (PLAYTEST.md).
