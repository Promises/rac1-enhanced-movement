# rac1-enhanced-movement: the play-test (PLAN.md Phase 5)

The mod played in OpenRAC's own rac1 NTSC-U port (built from OpenRAC `develop` at `6dc9cade`,
interface 1.14), on the player's disc data, by script. No person drove these runs: the port's
development switches did. Each check ran twice, the same route without the mod and with it, and
compared the two tick by tick.

## How

`tools/playtest.sh` runs one route, hidden (`OPENRAC_HIDDEN`) and silent (the dummy audio driver).
It starts the level directly (`OPENRAC_DIRECT` with `--level`) and skips its scenes with Start
(`k30:8:3`). It presses the pad on frames counted from control (`OPENRAC_PRESS` `cF:` entries) and
writes the port's per-tick trace (`OPENRAC_TRACE`: position, rotation, state, camera).
`tools/playtest_analyze.py` compares two runs segment by segment.

```sh
tools/playtest.sh /tmp/pt v_base 0 none
tools/playtest.sh /tmp/pt v_mod 0 '{"ground": true, "air": true, "strafe": "both", "flips": true, "twist": true, "trace": true}'
python3 tools/playtest_analyze.py /tmp/pt/v_base.jsonl /tmp/pt/v_mod.jsonl
```

The standard route, from control:
1. a full-stick run for 90 ticks, then letting go;
2. half stick for 60;
3. forward then fully back (a turnaround);
4. a run with X;
5. L2 with the stick left, right, then back;
6. L2 with the stick left and X (a flip);
7. the right stick turning the camera away;
8. L2 held standing.

Pictures come from `OPENRAC_SHOT`, with `--levels` pointing at OpenRAC's level export
(`assets/port/rac1/ntsc`), which the port needs to draw the world.

## What it showed

### Veldin (level 0, the first family of levels' state machine)

| Segment | Without the mod | With the mod |
|---|---|---|
| Full-stick run | top 5.70 u/s, 90% of it in 43 ticks | top 5.95 u/s, 90% in 19 ticks |
| Letting go | STOP (the skid) 48 ticks, then idle; slid 1.33 | idle at once; slid 0.27 |
| Half stick (0.5) | 0.90 u/s | 2.98 u/s (0.5 × 5.95) |
| Turnaround | covered 4.75 | covered 2.41, moving along the stick while he turns |
| X after a short run | plain jump (7) | running jump (9): rac2 gives it at full stick |
| L2, stick left / right / back | first-person look (state 1), as rac1 | strafe: facing within 0.01 rad of the camera, moving 1.59 / 1.70 / 3.07 rad off the facing |
| L2, stick left, X | look | side flip (state 0xB) for 69 ticks, drifting at 5.0 u/s |
| L2 held standing, camera turned away | look | turns to the camera through the walk (15 ticks: facing 0.30 → 1.48, camera 1.48–1.51), then idle |

In the mod run Ratchet was hit once (state 0x16) while standing with no input. The strafes had
taken him where the run without the mod never went. The mod's trace shows no decision that tick.

### Novalis (level 1) and Kerwan (level 3), the other two families

The run, letting go, half stick, the running jump, the flip and the standing strafe gave the same
numbers as on Veldin.

Two segments differed because of where the strafes had taken him, not because of the mod:
- **Novalis:** strafing left, the mod asked for 5.95 u/s (ground speed 0.0991 a tick) but he stood
  against a wall (flat speed 0.0003). The route had sent both runs off a ledge before it.
- **Kerwan:** strafing right showed rac2's side reversal. Coming from the left, his speed fell from
  4.15 to 0.37 and came back at 18 u/s². He then slid along an obstacle at 0.95 u/s.

### Every setting off

The mod loaded, took no hooks, and every tick of the 1747-tick route was the same as without the
mod: position, rotation, state, camera.

### Jumps and the shoulder buttons (Veldin)

| | Without the mod | With the mod |
|---|---|---|
| Standing jump, held | rise 1.867 | rise 1.867 |
| Double jump | rise 2.370 | rise 2.370 |
| L1 held | look (1) | look (1) |
| R1 held | crouch (4) | crouch (4) |

### The heli-pack (Kerwan, `--unlock-all-gadgets`)

Standing, the jump, the double jump (0xE) and the hover (state 8) were the same in both runs: 18,
31 and 32 ticks, rise 2.370.

Running, the mod's run reached the double jump over a raised step. It was 1.4 above it at the
double jump's 31st tick, under rac1's 1.5 for the hover, so rac1 gave no hover there. The run
without the mod, slower, was still over the lower ground and hovered.

### The hip twist (Veldin, pictures)

Strafing right, the camera behind him:
- **Twist on:** his legs and tail turn toward where he goes (the tail swings to the screen's left,
  as a pelvis turned to his right does). His head and upper body stay facing ahead.
- **Twist off:** he is square to the camera.

Pose record 0 is his pelvis and record 1 the body above, as in Going Commando: the assumption
DESIGN.md 4.2 had is confirmed.

### Every level (the standard route, with the default settings)

On all 19 levels, 0 to 18, the run with the mod reached control, ran to its end without a fault,
and the mod loaded.

| Levels | With the mod, against without |
|---|---|
| 0–9, 11–18, on foot | full-stick run 5.95 u/s against 5.70 (more where the ground slopes: Gaspar 8.15 against 7.29, Kalebo III 9.68 against 9.44); letting go: straight to idle where the game skids (STOP), wherever he was on level ground; the strafe flip (0xB) on every level where he reached it standing |
| 10, Orxon (Clank) | the same as without the mod, tick for tick in state (Clank's 0x43 and 0x44): the mod stays out of Clank (vehicle mode) |
| 8 Batalia, 18 Veldin 2 | the strafes took him off a cliff the route without the mod never reached (fatal fall, 0x77) |
| 9 Gaspar | he ran further and met a hazard (0x3C) |
| 12 Hoven | he sank (0x7F) before the strafe |
| 14 Oltanis | strafing left he slid along a wall: facing the camera exactly, moving 73 degrees off the stick at 1.67 u/s (5.95 × cos 73) |
| 2 Aridia | both runs fell into the same hazard and waited to respawn (0x68) through the strafe |

### PAL v2.00 (0.2.0)

The same checks on the PAL disc (`DISC=pal`, its data from OpenRAC's `tools/extractor.py`, the
program `openrac-rac1-pal` built from the PAL decompilation).

OpenRAC runs the PAL game in its 60 Hz mode (the port's log: "PAL 50 Hz: the port keeps 60"), so the
game itself plays PAL exactly as NTSC-U, tick for tick. Without the mod, the PAL runs gave NTSC-U's
numbers: run 5.70 u/s, 43 ticks to 90%, STOP for 48 ticks. With the mod:

- **Veldin, the standard route:** the same as on NTSC-U. Run 5.95 u/s, 90% in 19 ticks; straight to
  idle on letting go, sliding 0.27; half stick 2.98; strafing left, right and back; the flip (0xB)
  for 69 ticks at 5.0 u/s; the standing turn to the camera.
- **Every setting off:** identical to no mod on all 1722 ticks.
- **All 19 levels:** each run reached control and ended without a fault, and 0.2.0 loaded on each.
  Level for level the same as the NTSC-U sweep above: the run, the stop, the flips, Clank untouched
  on Orxon, and the same falls the route leads to.

The 50 Hz path itself (rac2's numbers scaled for it, rac1's windows counted by `scale_ticks`) cannot
run in OpenRAC today. The unit tests and the load test's PAL-tick scenario check it.

### The wrench (0.3.0)

The wrench was played against Going Commando itself, in OpenRAC's port of it (`openrac-rac2-ntsc2`,
the v2.00 disc's data). Both games ran the same pad routes on the same port switches. rac2 ran on
Oozla (its level 1), rac1 on Novalis (level 1) and Veldin (level 0). rac1 played each route three
ways: without the mod, with the mod and `wrench` off, and with the mod.

```sh
COMBO="c30:8000:4,c48:8000:4,c66:8000:4,c160:8000:4:128:0,c164:0:60:128:0,c260:800:10,c270:8800:4,c274:800:30,c330:0:60:128:0,c440:4000:6,c455:8000:4,c560:4000:6,c580:4000:4,c600:8000:4"
tools/playtest.sh /tmp/pt combo_on 1 '{"wrench": true, "trace": true}' ROUTE="$COMBO" CF=700
python3 tools/playtest_analyze.py --states /tmp/pt/combo_on.jsonl   # the states, from the port's trace
python3 tools/playtest_analyze.py --swings /tmp/pt/combo_on.log     # each swing, from the mod's trace
```

The routes (masks: `8000` Square, `800` R1, `4000` X, `100` L2):
- **Combo** (`COMBO` above): three Squares 18 frames apart; one Square with the stick held
  forward; R1 and Square (the throw); X then Square (the slam); a double jump, then Square.
- **Throw**: `c30:800:10,c40:8800:4,c44:800:6,c60:0:40:128:0,c200:800:10,c210:8800:4,c214:800:6,c240:4000:4,c400:8000:4,c418:8000:4,c425:0:40:128:0,c560:4000:4,c561:8000:3,c700:4000:4,c700:8000:2`.
  It throws, then pushes the stick; throws, then presses X; swings twice with the stick pushed
  after the second; presses Square one poll after X.
- **Veldin**: Ratchet is moved (`OPENRAC_WRITE`, the hero's position and facing, `+0x80` and `+0x98`)
  to the bolt crates (class 500) at (143.5, 96.8, 32.6), facing them, and at frame 560 to the horny
  toads (class 749) at (151, 121, 30.3). There: the combo into the crates, the throw, a Square with
  the stick left, L2 held with the stick right and Square, then X and Square by the toads.

Where the numbers come from:
- **Totals** (the states and their ticks) come from the port's trace with `--states`. The port
  writes the same trace for rac2.
- **rac1, swing by swing**: `--swings` reads the mod's trace from the run with `wrench` off, whose
  swings are rac1's own. The trace prints each tick after the mod's steers, so a steered tick shows
  the steered frame.
- **rac2, swing by swing**: a read-only probe library (`on_frame`) read the hero (`0x189E20`): state
  `+0x2294`, state timer `+0x1B8`, animation frame `+0xC38`, attack record `+0xBFC`, position
  `+0x80`, and `+0x123A` (the wrench out). It changes nothing and is not part of the mod. It read
  the same fields from rac1 too, and agreed with the mod's trace.

**The combo route, NTSC-U** (ticks; PAL gave the same numbers, tick for tick):

| | rac1 | rac1 with the mod | rac2 |
|---|---|---|---|
| Three swings: swing by swing | 25 / 17 / 46 | 23 / 17 / 56 | 23 / 17 / 56 |
| Three swings, in all | 88 | 96 | 96 |
| One swing, stick pushed | 36, idle 1, run | 26, idle 1, run | 26, run |
| Jump, Square: the jump, the slam | 15, 38 | not reached (below) | 15, 38 |
| Double jump, Square: the slam | 38 | 38 | 38 |

The lunges are the same everywhere: the three swings cover 3.15 units in all three columns, the
one swing 0.67. With the mod, the run out of the throw (the route pushes the stick there) took
Ratchet off a ledge. He was falling when the jump's X and Square came, and the Square swung on
landing. The slam's timing is in the next two tables.

**The throw route** (NTSC-U and PAL the same):

| | rac1 | rac1 with the mod | rac2 |
|---|---|---|---|
| Throw, then the stick pushed | 89 in the throw (to its frame 91), the stick ignored | 21 (frame 35), then the run while the wrench flies | 19 (frame 34), then the run |
| Throw, then X | 89, X lost | 30 (frame 39.5), then the jump | 30 (frame 39.5), then the jump |
| Two swings, stick pushed in the second | 25 / 40, idle | 23 / 24, idle 1, run | 23 / 24, run |
| X, Square one poll later | the jump, no slam | the jump 11, the slam 39 | the jump 11, the slam 39 |

- **The throw and the stick.** Each run pushed the stick when its throw was at a different frame.
  The mod ran out at the first tick past frame 34 with the stick pushed, as rac2's decider does.
- **The wrench in flight.** It came back to him while he ran (`thrown` 1 → 0 51 ticks after the
  throw) and while he jumped, with no catch frame queued on the run or the jump.
- **Left alone, the throw** stood at frame 96 with the mod (rac1 91), as rac2's does.

**Veldin, by the crates and the toads** (NTSC-U; PAL the same apart from which toad). The probe
also took `rac1.moby.record_death` (read only, the original always called) and logged each crate
and toad that died:

| | rac1 | rac1 with the mod |
|---|---|---|
| The combo into the two crates | both broken, at the same tick as with the mod | both broken |
| The slam by the toads | a toad killed | a toad killed (the same one on NTSC-U) |
| Square, stick left | 36 in the swing, then idle | 26, then the run |
| L2 held, stick right, Square | first-person look (L2 is rac1's look); with the mod and `wrench` off: the strafe, the swing 38 ticks, then standing | the strafe; the swing, 28 ticks to frame 26, turning toward the stick; then strafing again |

rac2 with R2 held, the stick right and Square (Oozla): the swing, 28 ticks to frame 26, then the
run, strafing. The mod's swing covered 1.04 units, rac2's 1.07.

The mod's trace said which steer it made each tick:
- `swing-chain-for-stay steer-frame` at the first swing's frame 25;
- `swing-run-for-stay steer-frame` running out;
- `swing-stay-for-idle steer-hold` for the third swing's frames 21 to 26 and the throw's 91 to 96;
- `square-buffer` for the Square one poll after X;
- `wrench-out` once per throw.

**Nothing else moved.** The standard movement route (above) with 0.3.0 at its defaults, `wrench`
on, against 0.2.1 (built from tag `rac1-enhanced-movement-v0.2.1`, `c1a8b55`), compared tick by
tick on position, rotation, state and camera:

| | NTSC-U | PAL |
|---|---|---|
| Veldin | 901 ticks, 0 differ | 901 ticks, 0 differ |
| Novalis | 901 ticks, 0 differ | |
| Kerwan | 901 ticks, 0 differ | |
| Every setting off against no mod (Veldin) | 901 ticks, 0 differ, no hooks taken | 901 ticks, 0 differ, no hooks taken |

So the run (5.95 u/s, 90% in 19 ticks), the stop, the half stick, the turnaround, the flips and the
strafe give the numbers above unchanged.

### The camera (0.3.0)

The camera's turn was measured from the trace's forward row (`fw`), each tick, in rac1 without the
mod, with each `camera` choice (the other settings at their defaults), and in OpenRAC's rac2 port
on the same route (Oozla, level 1; the rac2 routes as the wrench's above). rac1 ran on Veldin
(level 0) on both discs.

**`modern` was removed in 0.3.1** (PLAN.md section 10; the next section). Its rows and notes below
are 0.3.0's, kept for the record and marked *removed in 0.3.1*: they are not how the mod plays now.

```sh
CAM="c60:0:45:128:128:255:128,c165:0:45:128:128:0:128,c270:0:60:128:128:200:128,c390:0:40:128:128:128:0,c470:0:40:128:128:128:255,c560:0:6:128:128:255:128"
tools/playtest.sh /tmp/pt cam_rac2 0 '{"camera": "rac2"}' ROUTE="$CAM" CF=640
tools/playtest.sh /tmp/pt cam_rac2_fast 0 '{"camera": "rac2"}' ROUTE="$CAM" CF=640 OPENRAC_WRITE="1:15EDE4:2:30000"
python3 tools/playtest_analyze.py --camera /tmp/pt/cam_rac2.jsonl 165 44 270 60 560 6 390 40
```

The route (the right stick, `RX:RY` after the left stick's `LX:LY`): full right, then full left, for
45 ticks each; a third of the way right (200: 0.33 past the dead zone) for 60; up, then down, for 40
each; a 6-tick flick. The turn to the right ran into scenery on Oozla in rac2, so the tables use the
turn to the left. The game's camera speed option was left at its default, the middle step, or set to
the fast step by `OPENRAC_WRITE` (rac1's `opt_cam_yaw_rate`, NTSC `0x0015EDE4`, PAL `0x0015EEE4`;
rac2's at `0x001A7BB4`). `OPENRAC_WATCH` read the camera's numbers in the running games: rac1's table
1.0 / 1.3 / 1.6 degrees and option 1, the springs 0.015 / 0.2 and the pitch's steps 0.02 / 0.01 /
0.0305 on both discs; rac2's table 1.0 / 1.3 / 2.2 and option 1.

**Full stick, the game's default camera speed** (degrees a second; ticks from the press):

| | Yaw held | Half of it after | 90% after | Coasted after letting go | Stopped after | A third of the stick |
|---|---|---|---|---|---|---|
| rac1 without the mod | 77.5 | 11 | 24 | 16.1 degrees | 32 | 25.7 |
| rac2, its own port | 77.5 | 11 | 24 | 16.1 | 31 | 25.7 |
| `camera` `rac1` | 77.5 | 11 | 24 | 16.1 | 32 | 25.7 |
| `camera` `rac2` | **131.2** | 11 | 24 | 27.3 | 33 | 43.5 |
| `camera` `modern` (0.3.0; removed in 0.3.1) | 234.0 | 2 | 6 | 9.7 | 11 | 76.2 |

**The fast step:**

| | Yaw held | 90% after | Coasted | A third of the stick |
|---|---|---|---|---|
| rac1 without the mod, and `rac1` | 95.4 | 24 | 19.8 | 31.7 |
| rac2, its own port | 131.0 | 24 | 27.3 | 43.5 |
| `camera` `rac2` (every step) | 131.2 | 24 | 27.3 | 43.5 |
| `camera` `modern` (0.3.0; removed in 0.3.1) | 288.0 | 6 | 11.9 | 93.8 |

PAL v2.00 gave every number in both tables to the decimal (OpenRAC runs it at 60 Hz).

`camera` `rac2` is Going Commando's fastest step on every step of the game's option (PLAN.md section
10). Re-measured on 2026-11-14 with that: 131.2 degrees a second, 90% after 24 ticks and 27.3 coasted,
on the default step (NTSC-U and PAL), the fast step (NTSC-U) and the slow step (PAL,
`OPENRAC_WRITE="1:15EEE4:0:30000"`), as rac2's own port on its fastest step (131.0, 27.3).

- **The 6-tick flick**: rac1 and rac2's port 16.6 degrees a second held (the ramp has not got going),
  6.6 degrees coasted, stopped 34 ticks later; `rac2` 28.2, 11.2, 35; 0.3.0's `modern` (removed in
  0.3.1) 191.3, 6.1, 11.
- **The pitch** (the stick up for 40 ticks from level, -6 degrees): rac1 and `rac2` reach -30.5 at
  58.6 degrees a second at the fastest, still moving when the stick lets go, and come back to -27.7
  40 ticks later; rac2's port the same (58.9, -25.7). 0.3.0's `modern` (removed in 0.3.1) reached
  the game's 40-degree limit (-60.1; 90% of the way after 20 ticks) at up to 207.9 a second, and came
  back to -9.7 in 40 ticks: the game's return, three times as quick.
- **0.3.0's `modern` spring** (removed in 0.3.1): `OPENRAC_WATCH` showed rac1's springs at 0.2 / 0.7
  from the press until 20 ticks after letting go, then 0.015 / 0.2 again.
- **First person** (L1 held, the right stick full right): 87.4 degrees a second in rac1 and in rac2's
  port, half after 15 ticks; at a third of the stick 9.5 in both. The same with every choice: the
  setting does not touch it.
- **Clank** (Orxon, the standard route's right-stick turn at c760): 76.8 degrees a second without
  the mod, 234.0 with 0.3.0's `modern` (removed in 0.3.1).
- **The camera alone** (every other setting off, `camera` `modern` in 0.3.0, removed in 0.3.1): the
  mod took `rac1.pad.process_report` and `rac1.camera.follow.apply_overrides` only, and turned at
  234.0. 0.3.1 with a saved `modern` takes the same two and turns at 131.2 (`rac2`; the next section).

**Strafing with the camera** (`c60:100:30:128:128,c90:100:40:128:128:255:128,c130:100:40:128:128,c170:100:60:0:128:255:128,c230:100:30:128:128`:
L2 held standing and the right stick full right, then circle-strafing, the left stick left and the
right stick right):

| | Standing: camera | Facing off the camera (mean / most) | Circling: camera | Facing off the camera |
|---|---|---|---|---|
| `rac1` | 60.5 degrees a second | 8.6 / 14.6 degrees | 71.2 | 8.0 / 10.4 |
| `modern` (0.3.0; removed in 0.3.1) | 232.1 | 27.4 / 30.7 | 159.0 | 17.9 / 26.9 |

Under 0.3.0's `modern` he still strafed and faced the camera, on Going Commando's strafe turn (DESIGN.md
4.1), whose lag grows with the camera's speed. `rac1` played this route tick for tick as the build
before the camera did. `rac2` at Going Commando's fastest step, measured on 2026-11-14 as the hold's
last third and the facing off the camera over each hold (by that measure `rac1` gives 76.5 and 7.6 /
14.6 standing, 78.2 and 7.0 / 10.4 circling): 129.5 degrees a second and 11.0 / 17.4 standing, 132.4
and 11.4 / 15.0 circling.

**Nothing else moved.** With `camera` `rac1` (and, before `rac2` took Going Commando's fastest step,
with `rac2` on the game's default camera speed), compared tick by tick on position, rotation, state and camera against the build before the camera
(0.3.0's wrench alone, `PACKAGE=` pointing `tools/playtest.sh` at it; `--same`):

| Route | NTSC-U | PAL |
|---|---|---|
| The standard route, Veldin | 901 ticks, 0 differ (both choices) | 901 ticks, 0 differ (both) |
| Novalis, Kerwan, Orxon | 901 ticks each, 0 differ (both) | |
| The wrench combo route, Novalis | 701 ticks, 0 differ (both); `--swings` the same | 701 ticks, 0 differ (both) |
| Every setting off, `camera` `rac1`, against no mod | 901 ticks, 0 differ, no hooks taken | 901 ticks, 0 differ, no hooks taken |

With `rac2` at Going Commando's fastest step (2026-11-14): the wrench combo route, 701 ticks, 0
differ; the standard route on Veldin, 141 of 901 differ on both discs, the first at c760, the right
stick's first turn of the camera.

So the movement and wrench numbers above stand. With 0.3.0's `modern` (removed in 0.3.1) the
standard route first differed at c760 on both discs, the tick the right stick first turns the camera;
Ratchet himself first differed at c827 (PAL c830), where the strafe held standing turns him to the
camera the stick had turned further.

### `modern` removed (0.3.1)

0.3.1 against the 0.3.0 release (its `rac1-enhanced-movement-0.3.0.zip` unzipped, run with
`PACKAGE=`), on Veldin (level 0), the other settings at their defaults, on both discs (2026-11-14).
"A saved `modern`" is `{"camera": "modern"}` in the run's `plugins.json`, as 0.3.0 saved it:

```sh
PACKAGE=/path/to/0.3.0 tools/playtest.sh /tmp/pt cam_rac2_030 0 '{"camera": "rac2"}' ROUTE="$CAM" CF=640
tools/playtest.sh /tmp/pt cam_modern_031 0 '{"camera": "modern"}' ROUTE="$CAM" CF=640
python3 tools/playtest_analyze.py --same /tmp/pt/cam_modern_031.jsonl /tmp/pt/cam_rac2_030.jsonl
```

| 0.3.1 against 0.3.0 (`--same`) | NTSC-U | PAL |
|---|---|---|
| `rac1` against `rac1`: the camera route; the standard route | 641 ticks, 0 differ; 901, 0 differ | the same |
| `rac2` against `rac2`: the camera route; the standard route | 641, 0 differ; 901, 0 differ | the same |
| A saved `modern` against `rac2`: the camera route; the standard route | 641, 0 differ; 901, 0 differ | the same |
| The camera route on the slowest step (`OPENRAC_WRITE` option 0): `rac2`, and on NTSC-U a saved `modern`, against `rac2` | 641, 0 differ | 641, 0 differ (`rac2`) |
| The camera alone (every other setting off), a saved `modern` against `rac2` | 641, 0 differ | the same |
| A saved `modern` against 0.3.0's own `modern` (the check sees a difference) | 581 of 641 differ, the first at c60 | the same |

`--camera` on 0.3.1 (the turn to the left, the default step): `rac1` 77.5 degrees a second, half
after 11 ticks, 90% after 24, 16.1 coasted, stopped after 32; `rac2` and a saved `modern` 131.2,
11, 24, 27.3, 33, on both discs, and `rac2` the same on the slowest step: 0.3.0's numbers above.
0.3.0's own `modern` turned at 234.0 on the same route. With a saved `modern` the port logs
`setting "camera" is not a choice within its limits: its default instead` and the mod's line says
`camera Going Commando's`; with every other setting off it took `rac1.pad.process_report` and
`rac1.camera.follow.apply_overrides` only.

### Clank (0.4.0)

Clank alone was played on Orxon (level 10), which starts as Clank, on both discs, in the game
without the mod and with each `clank` choice; and in OpenRAC's rac2 port (`openrac-rac2-ntsc2`),
whose Aranos Prison (level 14) starts as Clank. Clank stands on a small lift there, so a write of his
position (`OPENRAC_WRITE`, the hero `0x189E20` + `0x80`) puts him on open floor at (300, 159, 110)
five frames after control. The rac2 run used the switches the wrench's used (`OPENRAC_SKIP_SCENES=1`,
`OPENRAC_PRESS="300:4000:10,…"`).

```sh
CLANK="c30:0:60:128:0,c130:0:60:128:214,c230:0:40:128:255,c270:0:40:128:0,c350:0:30:128:255,c380:0:40:255:128,c460:4000:4,c468:0:30:255:128,c520:0:80:0:128,c540:4000:4,c620:4000:4,c632:4000:40,c720:8000:3,c735:8000:3,c750:8000:3,c820:8000:3:128:0,c822:0:30:128:0,c880:8000:3,c890:4000:3,c980:8000:3,c991:8000:3,c1040:4000:4,c1070:8000:3"
tools/playtest.sh /tmp/pt clank_enh 10 '{"clank": "enhanced", "trace": true}' ROUTE="$CLANK" CF=1120
python3 tools/playtest_analyze.py --clank /tmp/pt/clank_enh.jsonl
# rac2: the same presses, and his position: OPENRAC_WRITE=c5:189EA0:43960000:1,c5:189EA4:431F0000:1,c5:189EA8:42DC6666:1
```

The route, from control:
1. a full-stick run for 60 ticks, then let go;
2. about half the stick (0.51 past the dead zone) for 60;
3. 40 ticks one way, then 40 fully the other way (a turnaround);
4. a run, then the stick turned 90 degrees;
5. a jump, with the stick pushed sideways in the air;
6. a running jump;
7. a jump held into the glide;
8. three Squares (the second 15 ticks into the first punch);
9. Square with the stick pushed;
10. Square then X;
11. Square, and Square again 11 ticks into that punch;
12. a jump with Square 30 ticks into it.

`--clank` measures each from the port's trace (rac2's states read as rac1's). A read-only probe
library, not part of the mod, read the punch's animation frame, its record and his ground distance
each tick in both games, and the hits and deaths (`rac1.combat.get_hit`, `rac1.moby.record_death`,
`rac1.player.take_damage`, the original always called).

**The route** (ticks; NTSC-U; PAL v2.00 gave every number, tick for tick):

| | rac1 without the mod, and `rac1` | `rac2` | `enhanced` | rac2's own port |
|---|---|---|---|---|
| Run, full stick | 3.00 u/s, 90% after 24 | 3.00, 11 | **3.45**, 12 | 3.00, 10 |
| Letting go | stopped after 2, slid 0.045 | 2, 0.039 | 2, 0.046 | 1, 0.000 |
| Half stick (0.51) | 1.53 u/s | 1.52 | 1.75 | 1.52 |
| Turnaround: moving the new way / facing it / carried the old way | 19 / 30 / 0.383 | 1 / 16 / 0 | 1 / 16 / 0 | 1 / 16 / 0 |
| 90-degree turn: within 10 degrees after | 19 | 7 | 7 | 7 |
| Jump: off the ground after X / apex / landed after | 6 / 1.004 / 51 | 4 / 1.004 / 49 | 4 / 1.004 / 49 | 4 / 1.004 / 49 |
| Air stick in the jump: speed, 90% after | 2.00 u/s, 4 | 2.00, 4 | **2.51**, 6 | 2.00, 4 |
| Running jump: distance (speed), apex | 1.10 (1.99 u/s), 1.004 | 1.10, 1.004 | **1.37 (2.49)**, 1.004 | 1.10, 1.004 |
| Glide: ticks, falling | 27, 1.44 u/s | 32, 1.44 | 32, 1.44 | 32, 1.44 |
| Three Squares: punch, idle, punch | 24, 6, 24 (the second Square lost: 9 polls before the first punch ended) | 22, 1, 22 | 22, 1, 22 | 22, 1, 22 |
| Square with the stick pushed: punching, running after | 24, 25 | 22, 23 | **18, 19** | 22, 23 |
| Square then X: jumping after | 21 | 18 | 18 | 18 |
| Square, Square 11 polls into the punch | one punch | one punch | **two** | one punch |
| Square 6 polls before the jump nears the ground | no punch | no punch | **a punch on landing** | no punch |

- The game itself starts and stops the run a tick after rac2's port: rac2 moves on the tick the stick
  goes, rac1 a tick later. The mod does not change that. With it, the ramp is rac2's to the tick (0.3
  u/s more each tick: 18 u/s²).
- **The punch** under `rac2` and `enhanced`, frame by frame from the probe: the blend (5 ticks), then
  half a frame a tick from frame 1, the hit from frame 3 to 6 (ticks 9 to 15, as without the mod). The
  mod's trace says `clank-punch-exit` where it stands at frame 9.5, `clank-jump-out` where X jumps out
  at 7.5 or later, and `clank-run-out` where the stick runs out at 7.5. rac2's port reaches frame 3 at
  tick 7 and lunges 0.69 units; the mod's punches lunge 0.81, as the game's.
- **Heights** are the same in every column (1.004 for the route's 4-tick X).
- **`rac1`** played the route tick for tick as the game without the mod: 1121 ticks, 0 differ (both
  discs).

**Orxon as built** (the start, its gates, the first enemy):
- **Crates.** Turned round from the start, toward the crates behind it, with Square every 26 ticks
  (`c20:0:110:203:231` then Square from c150). Each choice breaks the same five crates (bolt, oom-oom
  and metal crates): without the mod and with `rac1` at c240 to c284, with `rac2` at c215 to c263, with
  `enhanced` at c214 to c233.
- **The first toxic crab.** Forward through `gate_1` (it opens for Clank by itself) and the building to
  `gate_2` (open), then pushing toward the crab behind it, with Square every 25 ticks (`c20:0:480:128:0`,
  `c500:0:700:56:24`, Square from c590).

  | | At `gate_2` | Punches | Hits on the crab | Hurt at | Dying |
  |---|---|---|---|---|---|
  | Without the mod, and `rac1` (0 differ) | c559 | 16 | 0 | c598, 664, 730, 794 | c830 |
  | `rac2` | c547 | 17 | 0 | c585, 650, 717, 782 | c818 |
  | `enhanced` (the shorter first leg below) | | 17 | 0 | c520, 586, 650, 720 | c756 |

  `enhanced` (top speed 3.45) ran a first leg 45 ticks shorter (`c20:0:435:128:0`,
  `c455:0:745:56:24`, Square from c545), as the route's fixed stick path otherwise reaches its turn
  further on and pushes him into a corner the crab does not come to; at 3.13 the route above gave
  c527 at `gate_2` and hurt at c568, 634, 699, 764, dying at c800.

  The encounter plays the same in every choice, sooner: the crab hits him every 65 ticks, four hits
  end him, he respawns at the start. The script never reaches the crab with a punch, in the game
  without the mod either: this checks that the encounter is unchanged, not that it is won.
- PAL v2.00 played both routes tick for tick as NTSC-U, without the mod and with `enhanced`.

**Ratchet unchanged.** Against the build before Clank (0.3.1's tree, `PACKAGE=`), with every setting
at its default (`clank` `enhanced`), compared tick by tick (`--same`):

| Route | NTSC-U | PAL |
|---|---|---|
| The standard route, Veldin | 901 ticks, 0 differ | 901, 0 differ |
| The standard route, Novalis and Kerwan | 901, 0 differ each | |
| The wrench combo route, Novalis | 701, 0 differ | 701, 0 differ |
| The camera route, Veldin | 641, 0 differ | 641, 0 differ |
| Orxon's standard route with `clank` `rac1` | 901, 0 differ | 901, 0 differ |
| Every setting off (`clank` `rac1`) against no mod, Veldin | 901, 0 differ, no hooks taken | 901, 0 differ, no hooks taken |

### Camera choices (1.0.0)

The camera route above (`CAM`, Veldin, the turn to the left at c165 for 44 ticks), with each choice
on each step of the game's camera speed option (`OPENRAC_WRITE="1:15EDE4:<step>:30000"` on NTSC-U),
measured with `--camera`. Yaw held, degrees a second; every run reached half its rate after 11 ticks
and 90% after 24:

| The game's step | `rac1` | `rac2` | `enhanced` (default) |
|---|---|---|---|
| Slowest (0) | 59.6 | 59.6 | 100.9 |
| Middle (1, the game's default) | 77.5 | 77.5 | 131.2 |
| Fastest (2) | 95.4 | 131.2 | 161.5 |

- `rac2` matches Going Commando's own port on every step (77.5 on the middle, 131.0 on the fastest
  above).
- `enhanced` on the middle step is 0.4.1's `rac2` to the tick: the standard route with the default
  settings against the 0.4.1 release, 901 ticks, 0 differ.
- PAL v2.00, `enhanced` on the default step: 131.2, the same ramp and coast.
- The load line says `Camera: Enhanced` with the defaults.

## By hand

The user played the whole game through, start to finish, with the mod installed.

## Not covered by script

These need a save that has reached them, or a teleport the route does not do yet:
- ice (Orxon, Hoven);
- swimming;
- the hoverboard (Rilgar);
- Kalebo III's arena camera;
- Giant Clank, Blarg's Clank part (it starts at its split door, with Triangle), and Orxon's past the
  first toxic crab (the Gadgebots' containers break on his punch; the way there is not a straight line
  from the start);
- grind rails and the Swingshot;
- the wrench on ice, on magnet ground (Magneboots) and in the grind rails' attack;
- the camera with a real stick or gyro (steam-deck-pack) rather than scripted full and partial
  deflections, Giant Clank's camera, and the camera beside modern-controls (the README says to set
  `rac1` with it).

The slam's drive from more than 4 above the ground and the lock-on turn's 16.93 rad/s are checked by
the unit and load tests only. No route slams from that high, and a turn toward a moving target does
not show its rate.

The load test covers each of their early returns (vehicle mode, control modes, a camera that is not
the follow camera). Play them once by hand.
