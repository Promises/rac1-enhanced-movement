# Enhanced movement (Going Commando)

Ratchet moves as in **Ratchet & Clank: Going Commando**, in the first
Ratchet & Clank on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port:
- Going Commando's running and stopping, and its turning;
- more control in the air;
- Going Commando's strafe on L2 or R2, with its side and back flips and hip twist;
- Going Commando's wrench timing: the combo, the throw and the slam;
- a choice of how fast the right stick turns the camera: the first game's or
  Going Commando's;
- Clank, when he goes on alone: Going Commando's Clank, or (the default) a
  Clank a little past it.

The jump heights and the wrench's and Clank's damage stay the first game's, so every level plays as
it was built.

How each part was taken from Going Commando's code, with its evidence:
[DESIGN.md](DESIGN.md). The plan the mod was built to: [PLAN.md](PLAN.md).

**Discs:** Ratchet & Clank PAL v2.00 (SCES-50916) and NTSC-U v1.00
(SCUS-97199), the two the other mods for the first game support. PAL v1.00
and the Japanese disc are refused, with a reason, in the launcher. OpenRAC
runs the PAL disc in the game's 60 Hz mode, so on PAL the mod plays exactly as
on NTSC-U. It reads the game's own timing each tick, so a 50 Hz PAL game would
get Going Commando's numbers scaled as Going Commando's own PAL release scales
them.

## What changes

**Running** (setting *Going Commando running*):

- **Speed follows the stick.** The speed is 5.95 units a second times how far
  the stick is pushed. The first game has only a walk and a run (5.7).
- **Getting going and slowing down** are quicker: 18 and 20 units a second
  squared, against the first game's 7.5 and 8.5.
- **Turning** uses Going Commando's springs, steadier at a run.
- **The turnaround:** pushing the stick fully the other way turns him hard and
  slows him as Going Commando does. Moving off from slow, he goes where the
  stick points before he faces it.
- **Letting go of the stick** stops him on the spot. He skids out at no more
  than 1.5 units a second, without the first game's skid.
- **Standing, stopping and crouching** brake harder, as in Going Commando.
- **The jump:** X in the run gives the running jump only with the stick pushed
  past 0.85 and no strafe held. Otherwise it gives the plain jump.

**Air control** (setting *Going Commando air control*): in every jump, the
stick moves him at 5.95 units a second (5.7 before), and at 4.75 in the double
jump (3.5 before). Heights, gravity and the pack jumps are the first game's.

**Strafe** (setting *Strafe button*):

- Hold **L2 or R2**. Ratchet turns to face where the camera looks. The left
  stick moves him sideways, back and forward without turning him, and the right
  stick turns the camera and him with it. The camera slides along with him
  instead of swinging round.
- Going from one side to the other stops him before he goes the new way, as in
  Going Commando.
- Standing with the strafe held, he turns to the camera on his own.
- Going back, the walk or run plays backwards: the first game has no
  backpedal animation.
- **Flips** (setting *Strafe flips*): strafing, X with the stick pushed fully
  left, right or back does the first game's side or back flip that way.
- **Hip twist** (setting *Hip twist*): strafing, his hips turn toward where
  he goes and his upper body turns back to face ahead.
- **L1** still looks in first person and **R1** still crouches. With *L2 only*
  or *R2 only*, the other button stays the game's.

**Wrench** (setting *Going Commando wrench*): the first game's wrench, with
Going Commando's timing. The two games share the moves, the lunges and the
hits. What differs is when a swing chains or can be left:

- **Square is remembered longer**: 10 frames when standing or running (7
  before) and 14 in a jump (10 before), so a Square pressed a little early
  still swings or slams.
- **The combo**: the second swing chains a little sooner. With the stick
  pushed you run out of a swing as soon as Going Commando lets you: out of the
  first swing 10 frames sooner (26 against 36), out of the second 16 sooner
  (24 against 40). Left alone, the third swing holds a little longer, as in
  Going Commando.
- **The throw** (R1 + Square): once the wrench leaves his hand you can run
  (stick pushed fully) or jump while it flies back to you. Before, he stood
  still until he caught it. While it is out, Square does nothing, as in Going
  Commando, where the first game would still slam without a wrench.
- **Locking on** to an enemy turns him 13% faster (16.9 against 15.0 radians a
  second).
- **The slam** from high up (more than 4 above the ground) keeps driving
  forward at 4.5 units a second, as in Going Commando.
- **Square while strafing** swings toward the stick as usual, and he strafes
  again after it.

Damage is the first game's, so its enemies and bosses take the hits they were
built for. Hit timing, the throw's flight and its reach are the same in both
games.

**Camera** (setting *Camera rotation*): how fast the right stick turns the
camera behind Ratchet (the follow camera, Clank's too).

- **Going Commando** (the default): Going Commando's fastest camera, 132
  degrees a second at full stick, whatever the game's own camera speed is set
  to. Going Commando turns its camera with the first game's code and numbers,
  all but one: on the game's camera speed (Options, Camera: three steps, the
  middle one by default) the slow and middle steps are the same in both games,
  60 and 78 degrees a second, and only the fast step is quicker in Going
  Commando, 132 against 96. Since its default would change nothing, this choice
  takes Going Commando's top speed on every step.
- **Ratchet & Clank**: the first game's camera, untouched: 60, 78 or 96
  degrees a second by the game's camera speed.

First person (L1) is the same in both games and stays the game's in every
choice. The game's camera invert options keep working.

**Clank** (setting *Clank*): when Clank goes on alone (Orxon, Blarg). The first
game's Clank already moves at a speed that follows the stick and stops when you
let go; what makes him slow to handle is how he gets going, turns, jumps and
punches. Going Commando changed those with the same code, and the default goes a
little further, each step no bigger than the one Going Commando gave Ratchet:

| | First game | Going Commando | Enhanced (default) |
|---|---|---|---|
| Top speed | 3 units a second | 3 | 3.45 (15% faster; still well under Ratchet's 5.95) |
| Getting going and stopping | 7.5 and 8.5 units a second squared, slower while turning: 90% of his speed after 24 frames | 18 and 20: 11 frames | the same |
| Turning at a run | 90 degrees in 29 frames | 11 | the same |
| Turning around | runs a loop along his facing | turns hard and moves the new way at once | the same |
| Off the ground | 6 frames after X | 4 | the same |
| Moving in his jump | 2 units a second | 2 | 2.5 (Ratchet's double jump got the same share in Going Commando) |
| Turning to a target when punching | 15 radians a second | 16.9 | the same |
| A punch | 24 frames; X jumps out after 21 | 22; X jumps out after 18 | the same, and with the stick pushed he runs out after 18 |
| Square remembered | 9 frames before he can punch, 5 landing | the same | 13 and 7 (Ratchet's own step, 7 to 10) |

The same in every choice: jump heights, gravity, the glide (X held in the air),
the fall, the punch's hits, damage and reach, and the Gadgebot commands. Giant
Clank stays the game's. *Ratchet & Clank* leaves Clank exactly as the game has
him. Every number of the default is in `src/rac1/clank_tune.h`, with the reason
for it.

**Swimming** is the same code with the same numbers in both games, so it is
unchanged.

## Settings

| Key | Type | Default | |
|---|---|---|---|
| `ground` | bool | on | Going Commando's running, turning, turnaround and stopping |
| `air` | bool | on | Going Commando's air control (heights stay) |
| `strafe` | choice: `both`, `l2`, `r2`, `off` | `both` | the button held to strafe: L2 or R2, one of them, or no strafe |
| `flips` | bool | on | X while strafing with the stick fully sideways or back flips that way |
| `twist` | bool | on | strafing, his hips turn toward where he goes |
| `wrench` | bool | on | Going Commando's wrench timing: longer Square windows, running and jumping out of swings and the throw sooner, the faster lock-on turn, the slam's drive (damage stays) |
| `camera` | choice: `rac1`, `rac2` | `rac2` | how fast the right stick turns the camera: the first game's (78 degrees a second by default) or Going Commando's fastest (132). A `modern` saved by 0.3.0 counts as `rac2` |
| `clank` | choice: `rac1`, `rac2`, `enhanced` | `enhanced` | Clank alone: the first game's, Going Commando's, or a little past it (jump heights and punch damage stay; Giant Clank stays the game's) |
| `trace` | bool | off | for bug reports: one line in the game's log each tick with what the mod decided and every input it used, and one for the camera's update while the stick turns it (`OPENRAC_ENHANCED_MOVEMENT_TRACE=1` in the environment does the same). Each player's own |

Settings count from the game's next start. With everything off and the camera
and Clank on *Ratchet & Clank*, the mod takes no hooks and the game plays
exactly as the PlayStation 2's.

## Other mods

- **lock-strafe** also strafes on L2/R2 and takes the same hook points. The
  manifest declares the conflict, so turn one of them off.
- **wrench-throw** throws the wrench with R2 + Square, so it needs R2 on foot,
  which this mod's strafe takes by default. Set *Strafe button* to *L2 only*
  to use both: L2 strafes and R2 is wrench-throw's. With both on R2, one of
  the two R2 uses does nothing.
- **rac1-better-movement** is this mod's old name (0.1.0 and 0.2.0): remove it.
- **modern-controls** turns the camera with the right stick itself, at its own
  speed. Set *Camera rotation* to *Ratchet & Clank* to use both, so only one of
  the two sets the camera's speed. It also changes the movement's speed and
  acceleration through a hook point this mod shares; that pairing was not
  play-tested.
- **steam-deck-pack** turns the gyro and right trackpad into the right stick,
  so this mod's camera speed applies to them too; lower the pack's own
  sensitivity if that is too quick.
- **difficulty**: its *Ratchet speed* slider keeps working. Going Commando's
  speeds are scaled by it, in either load order, and so are Clank's where it
  scales his. No other mod in the index changes how Clank handles.
- The mod shares these hook points:
  - `rac1.player.update_motion_input`
  - `rac1.player.turn_to_heading`
  - `rac1.player.approach_speed`
  - `rac1.player.set_planar_velocity`
  - `rac1.pad.process_report`
  - `rac1.camera.follow.apply_overrides`
  - `rac1.player.advance_animation`

  Every one runs the game's code (or the next mod's) when it has nothing to do.

`multiplayer` is `input`: how one player's Ratchet moves is that player's own.

## Known limits

- **Jump heights are the first game's** on purpose. Going Commando's plain
  jump is lower (2.3 held against 2.62), and some of the first game's ledges
  need the higher jump.
- **Out of reach of the port's hook points** (no hook point reaches inside the
  functions that do them):
  - Going Commando eases into the hover's descent, where the first game sets it;
  - the heli glide's slightly different height thresholds;
  - Going Commando dropping the first game's per-tick limit on falling speed.
- **The running jump just after starting:** Going Commando gives it whenever
  the stick is past 0.85. The first game also wants him already running, and
  the mod only turns its running jump into a plain jump, not the other way.
- **No backpedal animation**: going back while strafing plays the walk or run
  backwards.
- **The strafe is off** under a level's own camera (Kalebo III's arena, seen
  from a wall), on magnetic walls (Magneboots), and in the long jump, as in
  Going Commando.
- **On ice**, and on a conveyor or other push, the first game's running stays.
- **The wrench, not ported** (each is inside game code no hook point reaches,
  or would need a guess):
  - Going Commando keeps the combo going after a short run (under 27 frames
    in all); here a swing after a run starts the combo again;
  - Going Commando's thrown wrench carries a little more of his own speed and
    snaps back to him past 32 units;
  - the slam keeps driving on a few ground types in Going Commando;
  - a swing stops at a ledge a little later in Going Commando.
- **Running out of a swing** takes one frame longer than in Going Commando:
  the first game's own code stands for a frame, then runs.
- **The camera, left as the first game's:**
  - first person (L1), the same in both games;
  - the stick's dead zone (the same in both games), which the whole game reads;
  - how hard the camera is pulled round behind him as he runs, and how long a
    swing toward a point of interest waits after the right stick moves: Going
    Commando's differ a little, but neither is the turn's speed.
- **Clank, left as the game's:**
  - Giant Clank (Quartu): Going Commando has no walking Giant Clank to take
    anything from, and his boss fight is built around his slowness;
  - Going Commando's punch also hits 2 frames sooner and lunges a little less
    far: only its quicker way out is taken, so his reach stays the first game's;
  - the glide's and the fall's speeds (the same in both games);
  - Going Commando looks on L1 only; the first game's Clank looks on L1 or L2.
- **Steered transitions.** The mod never calls the game's state changes. For
  one tick it sets what the game's own transition code reads (the state timer,
  a speed, the stick, and for a flip R1 and a stick press beside X; for the
  wrench the animation's frame or its blend flag, and Square or X in the pad's
  history), and puts each back right after. So each planet's own code makes the change. With
  `trace` on, the log shows each such tick. The camera is steered the same way: its turn's speed
  is set for one update and put back.

## Building

```sh
python3 tools/fetch_sdk.py                 # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build --output-on-failure
python3 tools/cpp_style.py                 # layout (clang-format and the blank-line rules)
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rac1_enhanced_movement -o dist
```

To try a build, add `build/package/rac1_enhanced_movement` in the launcher's
**Mods** tab. `tools/playtest.sh` plays a scripted route in OpenRAC's port
(hidden, against the game without the mod), and `tools/playtest_analyze.py`
compares two runs. [PLAYTEST.md](PLAYTEST.md) has what they showed on every
level. `tools/rac2_constants.py` reads Going Commando's numbers out of
[rac2-decomp](https://github.com/OpenRAC/rac2-decomp) into
`tests/data/rac2_constants.tsv`, and the tests check `src/rac1/rac2.h`
against it.

## Notes

- **0.4.0**: Clank (setting *Clank*, *Enhanced* by default): when he goes on
  alone, he gets going, stops, turns and turns around as in Going Commando,
  leaves the ground sooner in a jump, turns to a target quicker and leaves a
  punch sooner; by default also 15% faster (3.45 units a second), with more control in his jump,
  Square remembered longer and a run out of a punch with the stick pushed. Jump
  heights and punch damage stay the first game's, Giant Clank stays the game's.
  *Ratchet & Clank* keeps him exactly as the game has him. Played against Going
  Commando's own Clank in its port and on both discs (PLAYTEST.md). Ratchet
  plays exactly as 0.3.1.
- **0.3.1**: the camera's *Modern* choice is gone. Played, it turned the
  camera far too fast, even on the game's slowest camera speed (three times
  the game's: 180, 234 or 288 degrees a second). *Going Commando* (132 degrees
  a second, the default) is now the fastest choice. A *Modern* saved by 0.3.0
  gives the default, *Going Commando*: the launcher drops the old value when
  it installs 0.3.1, and the game hands the default for it anyway. Everything
  else plays exactly as 0.3.0.
- **0.3.0**: Going Commando's wrench (setting *Going Commando wrench*, on by
  default): Square remembered longer, the combo's chaining and its way out with
  the stick, running and jumping while a thrown wrench flies back, no Square
  while it is out, the faster lock-on turn and the slam's drive from high up.
  Damage stays the first game's. Square and X together while strafing now
  swing, as in Going Commando, where 0.2 could turn them into a throw. Played
  against Going Commando itself in OpenRAC's port of it, and on both discs
  (PLAYTEST.md). And a choice of camera turn speed (setting *Camera rotation*):
  the first game's, Going Commando's (the default: its fastest camera, 132
  degrees a second where the first game turns 78 by default), or *Modern*, three times the game's speed (234 degrees a second by
  default) with the camera following the stick closely. Measured against Going
  Commando's own camera in its port. Everything else plays exactly as 0.2.1.
- **0.2.1**: renamed from `rac1-better-movement` (its 0.1.0 and 0.2.0). The
  launcher sees a new mod: remove the old one, then add this one
  (`https://github.com/kmomoney/rac1-enhanced-movement`). The two are marked
  as conflicting, so they never run together.
- **0.2.0**: the PAL v2.00 disc, play-tested on it as on NTSC-U (PLAYTEST.md).
  The mod also follows the game's 50 Hz timing (rac2's numbers scaled as rac2's
  PAL release scales them, the first game's tick windows counted as it counts
  them), which OpenRAC does not run today.
- **0.1.0**: the first version, NTSC-U.

Nothing from the game's discs is in this repository: only addresses, offsets
and numbers worked out from the decompilations. GPL-3.0-or-later.
