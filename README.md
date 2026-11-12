# Better movement (Going Commando)

Ratchet moves as in **Ratchet & Clank: Going Commando**, in the first
Ratchet & Clank on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port:
- Going Commando's running and stopping, and its turning;
- more control in the air;
- Going Commando's strafe on L2 or R2, with its side and back flips and hip twist.

The jump heights stay the first game's, so every level plays as it was built.

How each part was taken from Going Commando's code, with its evidence:
[DESIGN.md](DESIGN.md). The plan the mod was built to: [PLAN.md](PLAN.md).

**Disc:** Ratchet & Clank NTSC-U (SCUS-97199). Other discs are refused with a
reason in the launcher.

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
| `trace` | bool | off | for bug reports: one line in the game's log each tick with what the mod decided and every input it used (`OPENRAC_BETTER_MOVEMENT_TRACE=1` in the environment does the same). Each player's own |

Settings count from the game's next start. With everything off, the mod takes
no hooks and the game plays exactly as the PlayStation 2's.

## Other mods

- **lock-strafe** also strafes on L2/R2 and takes the same hook points. The
  manifest declares the conflict, so turn one of them off.
- **difficulty**: its *Ratchet speed* slider keeps working. Going Commando's
  speeds are scaled by it, in either load order.
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
- **Steered transitions.** The mod never calls the game's state changes. For
  one tick it sets what the game's own transition code reads (the state timer,
  a speed, the stick, and for a flip R1 and a stick press beside X), and puts
  each back right after. So each planet's own code makes the change. With
  `trace` on, the log shows each such tick.

## Building

```sh
python3 tools/fetch_sdk.py                 # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build --output-on-failure
python3 tools/cpp_style.py                 # layout (clang-format and the blank-line rules)
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rac1_better_movement -o dist
```

To try a build, add `build/package/rac1_better_movement` in the launcher's
**Mods** tab. `tools/playtest.sh` plays a scripted route in OpenRAC's port
(hidden, against the game without the mod), and `tools/playtest_analyze.py`
compares two runs. [PLAYTEST.md](PLAYTEST.md) has what they showed on every
level. `tools/rac2_constants.py` reads Going Commando's numbers out of
[rac2-decomp](https://github.com/OpenRAC/rac2-decomp) into
`tests/data/rac2_constants.tsv`, and the tests check `src/rac1/rac2.h`
against it.

## Notes

- **0.1.0**: the first version, NTSC-U.

Nothing from the game's discs is in this repository: only addresses, offsets
and numbers worked out from the decompilations. GPL-3.0-or-later.
