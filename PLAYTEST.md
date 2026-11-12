# rac1-better-movement: the play-test (PLAN.md Phase 5)

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

## Not covered by script

These need a save that has reached them, or a teleport the route does not do yet:
- ice (Orxon, Hoven);
- swimming;
- the hoverboard (Rilgar);
- Kalebo III's arena camera;
- Clank and Giant Clank;
- grind rails and the Swingshot.

The load test covers each of their early returns (vehicle mode, control modes, a camera that is not
the follow camera). Play them once by hand.
