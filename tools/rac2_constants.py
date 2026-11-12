#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Going Commando's movement numbers, read out of rac2-decomp, into tests/data/rac2_constants.tsv.

    python3 tools/rac2_constants.py [--decomp ../rac2-decomp] [--lit <level 0's lit.bin>]

Each constant src/rac1/rac2.h uses is a literal of rac2-decomp's C (USA v1.01, level 0's shared hero
code), found here by a pattern in its file. The table records the literal as written and where it is,
and tests/rac2_constants_test.py checks rac2.h against it, so a typo in rac2.h fails the build's
tests. The run speed is not a literal but level data (D_001A7DB0[4], read by func_L00_002B3D10):
with --lit (a level 0 lit.bin from rac2-decomp's `tools/levels.py dump`, whose section starts at
0x1A7C80) it is read from there; without, the row the table already has is kept.

Numbers only: nothing of the game's own bytes goes into the table but the one float it names.
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / "tests" / "data" / "rac2_constants.tsv"

SHARED = "src/levels/shared/"
STATE_MACHINE = SHARED + "L00_002BA7D8.c"
RUN_TURN = SHARED + "L00_002B3F68.c"
TARGET = SHARED + "L00_002B3D10.c"
DECIDER = SHARED + "L00_002CB3A8-decider.c"
ENTRY = "src/levels/00_aranos_tutorial/L00_002C42E0-state-g8.c"
STRAFE = SHARED + "L00_002B35A0.c"
STICK = SHARED + "L00_002AFBC0-input.c"
AIR = SHARED + "L00_002B6BC8.c"
POSE = SHARED + "L00_002B7B80.c"
JUMP = SHARED + "L00_002C2DE8.c"
TURNING = SHARED + "L00_002AFBC0.c"

F = r"(-?[0-9]+\.[0-9]+f?)"
# |yaw error| as func_L00_002BA7D8 writes it.
ERR = r"func_00282C48\(\(\(Hero \*\)D_00189E20\)->f\[0x1A8/4\]\)"

# name, file, pattern with one group: the literal. The first match counts.
SPECS: list[tuple[str, str, str]] = [
    # The run (func_L00_002BA7D8, case 2 and 0x70) and its target speed (func_L00_002B3D10).
    ("WADING_SCALE", TARGET, r"\* " + F + r";\s*if \(H->f\[0x1B0 / 4\] < "),
    ("WADING_MIN", TARGET, r"if \(H->f\[0x1B0 / 4\] < " + F),
    ("STICK_RELEASE", STATE_MACHINE, r"f\[0x24B8/4\] < " + F + r"\) \{\s*if \(\(\(Hero \*\)D_00189E20\)->i\[0x410/4\]"),
    ("RUN_ACCEL", STATE_MACHINE, r"k005 = var_f20_7 \* " + F),
    ("RUN_BRAKE", STATE_MACHINE, r"var_f20_8 = " + F + r";\s*if \(\(\(Hero \*\)D_00189E20\)->b\[0x40F\] != 0\) \{\s*var_f1_2"),
    ("TURNAROUND_BRAKE", STATE_MACHINE, r"var_f20_8 = " + F + r";\s*\(\(Hero \*\)D_00189E20\)->f\[0x1B0/4\] = \(f32\)"),
    ("K_STICK", STATE_MACHINE, r"f\[0x24B8/4\] < " + F + r"\) \|\| \(\(\(\(Hero"),
    ("K_ANGLE", STATE_MACHINE, r"\(temp_f2_2 > " + F + r"\)\) \{\s*var_f20_7 = 0"),
    ("SLOW_STICK", STATE_MACHINE, r"f\[0x24B8/4\] < " + F + r"\) && \(temp_f2_3 > "),
    ("SLOW_ANGLE", STATE_MACHINE, r"\(temp_f2_3 > " + F),
    ("SLOW_FACTOR", STATE_MACHINE, r"var_f1_3 = 1\.0f - \(temp_f2_3 \* " + F),
    ("TURNAROUND_CAP", STATE_MACHINE, r"var_f2_2 = " + F + r" - "),
    ("TURNAROUND_CAP_SLOPE", STATE_MACHINE, r"var_f2_2 = [0-9.]+f - \(" + ERR + r" \* " + F),
    ("TURNAROUND_SCALE_BASE", STATE_MACHINE, r"var_f1_2 = " + F + r" - "),
    ("ALONG_SPEED", STATE_MACHINE, r"f\[0x174/4\] < " + F + r"\) \{\s*\(\(Hero \*\)D_00189E20\)->b\[0x40C\] = 1U"),
    ("TURNAROUND_SLOW_ANGLE", STATE_MACHINE, r"f\[0x1A8/4\]\) > " + F + r"\) && \(\(\(Hero \*\)D_00189E20\)->f\[0x24B8/4\] == 1\.0f\)\) \{\s*\(\(Hero \*\)D_00189E20\)->b\[0x40F\] = 1U;\s*\}"),
    ("TURNAROUND_FLICK_ANGLE", STATE_MACHINE, r"func_L00_0032F038\(7\)\) > " + F),
    ("TURNAROUND_FLICK_ERROR", STATE_MACHINE, r"func_L00_0032F038\(7\)\) > [0-9.]+f\) && \(" + ERR + r" > " + F),
    ("ALONG_CLEAR", STATE_MACHINE, r"f\[0x1A8/4\]\) < " + F + r"\) \{\s*\(\(Hero \*\)D_00189E20\)->b\[0x40C\] = 0U"),
    ("TURNAROUND_CLEAR", STATE_MACHINE, r"b\[0x40F\] != 0\) && \(" + ERR + r" < " + F),
    ("RUNBAND_TURN_ACCEL", STATE_MACHINE, r"func_L00_002B3EF8\(0, " + F + r", 0\.17f, 0\.16580631f\)"),
    ("RUNBAND_TURN_DAMP", STATE_MACHINE, r"func_L00_002B3EF8\(0, 0\.02f, " + F + r", 0\.16580631f\)"),
    ("RUNBAND_TURN_LIMIT", STATE_MACHINE, r"func_L00_002B3EF8\(0, 0\.02f, 0\.17f, " + F + r"\)"),
    ("IDLE_BRAKE", STATE_MACHINE, r"\} else \{\s*var_f20 = " + F + r";\s*\}\s*func_L00_002B41B8\(0\.0f, var_f20\)"),
    ("STOP_BRAKE", STATE_MACHINE, r"var_f20 = var_f12 \* " + F),
    ("CROUCH_BRAKE", STATE_MACHINE, r"\} else if \(temp_v1_3 == 4\) \{\s*var_f20 = " + F),
    ("CROUCH_ICE_BRAKE", STATE_MACHINE, r"b\[0x1492\] != 0\) \{\s*var_f20 = " + F),
    # The walk band's, the turnaround's and the firing turn (func_L00_002B3F68).
    ("WALK_TURN_ACCEL_MAX", RUN_TURN, r"if \(speed == 1\.0f\) \{\s*first = " + F),
    ("WALK_TURN_ACCEL_PER_STICK", RUN_TURN, r"first = speed \* " + F),
    ("WALK_TURN_ACCEL_MIN", RUN_TURN, r"if \(first < " + F),
    ("WALK_TURN_LIMIT_MAX", RUN_TURN, r"== 1\.0f\) \{\s*third = " + F),
    ("WALK_TURN_LIMIT_PER_STICK", RUN_TURN, r"third = speed \* " + F),
    ("WALK_TURN_LIMIT_MIN", RUN_TURN, r"if \(third < " + F),
    ("WALK_TURN_DAMP", RUN_TURN, r"second = " + F + r";\s*if \(\(\(MovementView"),
    ("TURNAROUND_TURN_ACCEL", RUN_TURN, r"->alternate\) \{\s*first = " + F),
    ("TURNAROUND_TURN_LIMIT", RUN_TURN, r"->alternate\) \{\s*first = [0-9.]+f;\s*third = " + F),
    ("FIRING_FULL_STICK", RUN_TURN, r"if \(speed > " + F),
    ("FIRING_ACCEL", RUN_TURN, r"if \(speed > [0-9.]+f\) \{\s*first = " + F),
    ("FIRING_DAMP", RUN_TURN, r"if \(speed > [0-9.]+f\) \{\s*first = [0-9.]+f;\s*second = " + F),
    ("FIRING_LIMIT", RUN_TURN, r"if \(speed > [0-9.]+f\) \{\s*first = [0-9.]+f;\s*second = [0-9.]+f;\s*third = " + F),
    ("FIRING_ACCEL_PER_STICK", RUN_TURN, r"third = [0-9.]+f;\s*\} else \{\s*first = speed \* " + F),
    ("FIRING_DAMP_SLOW", RUN_TURN, r"\} else \{\s*first = speed \* [0-9.]+f;\s*second = " + F),
    ("FIRING_LIMIT_PER_STICK", RUN_TURN, r"\} else \{\s*first = speed \* [0-9.]+f;\s*second = [0-9.]+f;\s*third = speed \* " + F),
    # Leaving the run and the jump X gives (func_L00_002CB3A8, case 2).
    ("EXIT_STICK", DECIDER, r"if \(\(\(FLD\(D_00189E20, f32, 0x24B8\) < " + F + r"\) \|\| \(FLD\(D_00189E20, s16, 0x1E0\)"),
    ("EXIT_TURNED", DECIDER, r"turned = func_L00_002B3268\(" + F + r"\);\s*st = "),
    ("EXIT_CLAMP", DECIDER, r"func_002AC9F8\(D_00189E20 \+ 0x160, " + F),
    ("RUN_JUMP_STICK", DECIDER, r"if \(FLD\(D_00189E20, f32, 0x24B8\) > " + F + r"\) \{\s*if \(FLD\(D_00189E20, s16, 0x1454\) == 0\)"),
    # The jump arm of the state entry (func_L00_002C42E0).
    ("AIR_STICK_SPEED", ENTRY, r"c4A4 = " + F),
    ("DOUBLE_JUMP_AIR_SPEED", ENTRY, r"HALF\(0x518\) = 14; REAL\(0x514\) = 1\.0f; REAL\(0x4A4\) = " + F),
    ("FLIP_DRIFT_START", ENTRY, r"BYTE\(0x52D\) = 1; REAL\(0x4DC\) = " + F),
    # The strafe: held, facing, sides (func_L00_002B35A0), sectors (func_L00_002B39F0).
    ("STRAFE_TURN_ACCEL", STRAFE, r"func_L00_002B3EF8\(0," + F),
    ("STRAFE_TURN_DAMP", STRAFE, r"func_L00_002B3EF8\(0,[0-9.]+f," + F),
    ("STRAFE_TURN_LIMIT", STRAFE, r"func_L00_002B3EF8\(0,[0-9.]+f,[0-9.]+f," + F),
    ("STRAFE_REVERSE", STRAFE, r"if\(\*\(short \*\)\(p\+0x146c\)!=0\) f=" + F),
    ("STRAFE_SECTOR", STICK, r"angle,1\.5707964897155762f\)<" + F),
    ("STRAFE_BACKPEDAL_STICK", STICK, r"x24b8>" + F),
    ("STRAFE_START_ANGLE", DECIDER, r"turned = func_L00_002B3268\(" + F + r"\);\s*if \(go == 0\)"),
    ("STRAFE_START_STICK", DECIDER, r"\(turned != 0\) \|\| \(FLD\(D_00189E20, f32, 0x24B8\) > " + F),
    ("FLIP_STICK", JUMP, r"if \(" + F + r" < \*\(float \*\)\(p \+ 0x24B8\)\)"),
    ("FLIP_DRIFT_PER_STICK", AIR, r"float drift = HOP_PLAYER\.stick_magnitude \* " + F),
    ("FLIP_DRIFT_MIN", AIR, r"if \(drift < " + F),
    ("FLIP_DRIFT_STEP", AIR, r"func_002AA2E8\(&HOP_PLAYER\.high_jump_drift_speed, drift, " + F),
    # The twist (func_L00_002B7B80).
    ("TWIST_STICK", POSE, r"f\[0x24B8 / 4\] > " + F + r" && \(\(Hero \*\)D_00189E20\)->i\[0x2294 / 4\] != 11"),
    ("TWIST_HIP_DAMP", POSE, r"->f\[0xA8 / 4\] = " + F),
    ("TWIST_HIP_ACCEL", POSE, r"->f\[0xA4 / 4\] = " + F),
    ("TWIST_BODY_ACCEL", POSE, r"->f\[0x154 / 4\] = " + F),
    ("TWIST_BODY_DAMP", POSE, r"->f\[0x158 / 4\] = " + F),
]


def extract(decomp: Path) -> list[tuple[str, str, str]]:
    """Each spec's literal and where it is, or an error naming the specs that matched nothing."""
    rows, missing = [], []
    cache: dict[str, str] = {}

    for name, rel, pattern in SPECS:
        text = cache.setdefault(rel, (decomp / rel).read_text(encoding="utf-8", errors="replace"))
        found = re.search(pattern, text)

        if not found:
            missing.append(f"{name} ({rel})")
            continue

        line = text.count("\n", 0, found.start(1)) + 1
        rows.append((name, found.group(1), f"rac2-decomp/{rel}:{line}"))

    if missing:
        sys.exit("no match for: " + ", ".join(missing))

    return rows


def run_speed(lit: Path | None) -> tuple[str, str, str]:
    """D_001A7DB0[4] from a level 0 lit.bin (section at 0x1A7C80), or the table's row."""
    if lit is not None:
        value = struct.unpack_from("<f", lit.read_bytes(), 0x1A7DC0 - 0x1A7C80)[0]
        return ("RUN_SPEED", f"{value:.6g}f", "level 0 data D_001A7DB0[4] (0x1A7DC0)")

    for line in TABLE.read_text(encoding="utf-8").splitlines():
        if line.startswith("RUN_SPEED\t"):
            name, literal, source = line.split("\t")
            return (name, literal, source)

    sys.exit("no RUN_SPEED: pass --lit <level 0 lit.bin>")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--decomp", type=Path, default=ROOT.parent / "rac2-decomp")
    parser.add_argument("--lit", type=Path, help="level 0's lit.bin, for the run speed")
    args = parser.parse_args()

    rows = [run_speed(args.lit)] + extract(args.decomp)
    TABLE.parent.mkdir(parents=True, exist_ok=True)
    with TABLE.open("w", encoding="utf-8", newline="\n") as out:
        out.write("# name\tliteral\tsource (written by tools/rac2_constants.py)\n")
        for row in rows:
            out.write("\t".join(row) + "\n")

    print(f"{len(rows)} constants into {TABLE.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
