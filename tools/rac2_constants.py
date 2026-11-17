#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read Going Commando's movement numbers out of rac2-decomp into tests/data/rac2_constants.tsv.

    python3 tools/rac2_constants.py [--decomp ../rac2-decomp] [--lit <level 0's lit.bin>]
                                    [--data <level 0's data.bin>]

Each constant src/rac1/rac2.h uses is a literal in rac2-decomp's C (USA v1.01, level 0's shared
hero code), found by a pattern in its file: a float, or an integer count of ticks or polls. The
table records each literal as written and where it is, and tests/rac2_constants_test.py checks
rac2.h against it, so a typo in rac2.h fails the tests.

Three kinds of number are level data, not literals:
- The run speed (D_001A7DB0[4], read by func_L00_002B3D10), read with --lit from a level 0 lit.bin
  that rac2-decomp's `tools/levels.py dump` writes (its section starts at 0x1A7C80).
- The follow camera's numbers (the speed table D_L00_001B15B8 that func_L00_004263B8 reads, the
  pitch steps func_L00_00425118 reads and the eye's springs func_L00_00425CD0 reads), also read
  with --lit. They are at the v2.00 disc's addresses, 0x60 below v1.01's (func_L00_004263B8's
  `addiu $a1, $gp, 0x2568` with $gp 0x1AEFF0, and the other two's $gp loads).
- The attack records (D_L00_0025D990, 0x2C bytes each, read by func_L00_002C9448 and
  func_L00_002CB3A8), read with --data from the same dump's level 0 data.bin, whose manifest.json
  gives the section's address: the frames of records 0 to 3 (the wrench's) and 7 and 8 (Clank's
  punches). The dumps used are of the v2.00 disc (`--version us2`), where the table is at
  0x25D910 (func_L00_002C9448's lui/addiu at 0x2C9A5C/0x2C9A68), 0x80 below v1.01's.
Without --lit or --data, the rows already in the table are kept.

Only the numbers go into the table, never the game's own bytes.
"""
from __future__ import annotations

import argparse
import json
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
WRENCH = SHARED + "L00_002C9448-g8.c"

F = r"(-?[0-9]+\.[0-9]+f?)"
# A count: decimal, or hexadecimal as m2c writes some.
I = r"(0x[0-9A-Fa-f]+|[0-9]+)"
# The absolute yaw error, as func_L00_002BA7D8 writes it.
ERR = r"func_00282C48\(\(\(Hero \*\)D_00189E20\)->f\[0x1A8/4\]\)"

# Each spec is a name, a file and a pattern whose one group is the literal. The first match counts.
SPECS: list[tuple[str, str, str]] = [
    # The run (func_L00_002BA7D8 cases 2 and 0x70) and its target speed (func_L00_002B3D10).
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
    # The walk band's, the turnaround's and the firing turn springs (func_L00_002B3F68).
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
    # Leaving the run, and the jump X gives (func_L00_002CB3A8 case 2).
    ("EXIT_STICK", DECIDER, r"if \(\(\(FLD\(D_00189E20, f32, 0x24B8\) < " + F + r"\) \|\| \(FLD\(D_00189E20, s16, 0x1E0\)"),
    ("EXIT_TURNED", DECIDER, r"turned = func_L00_002B3268\(" + F + r"\);\s*st = "),
    ("EXIT_CLAMP", DECIDER, r"func_002AC9F8\(D_00189E20 \+ 0x160, " + F),
    ("RUN_JUMP_STICK", DECIDER, r"if \(FLD\(D_00189E20, f32, 0x24B8\) > " + F + r"\) \{\s*if \(FLD\(D_00189E20, s16, 0x1454\) == 0\)"),
    # The jump arm of the state entry (func_L00_002C42E0).
    ("AIR_STICK_SPEED", ENTRY, r"c4A4 = " + F),
    ("DOUBLE_JUMP_AIR_SPEED", ENTRY, r"HALF\(0x518\) = 14; REAL\(0x514\) = 1\.0f; REAL\(0x4A4\) = " + F),
    ("FLIP_DRIFT_START", ENTRY, r"BYTE\(0x52D\) = 1; REAL\(0x4DC\) = " + F),
    # The strafe: its turn and sides (func_L00_002B35A0), sectors and backpedal (func_L00_002B39F0),
    # standing start (func_L00_002CB3A8) and flip (func_L00_002C9FC8, func_L00_002B6BC8).
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
    # The hip twist (func_L00_002B7B80).
    ("TWIST_STICK", POSE, r"f\[0x24B8 / 4\] > " + F + r" && \(\(Hero \*\)D_00189E20\)->i\[0x2294 / 4\] != 11"),
    ("TWIST_HIP_DAMP", POSE, r"->f\[0xA8 / 4\] = " + F),
    ("TWIST_HIP_ACCEL", POSE, r"->f\[0xA4 / 4\] = " + F),
    ("TWIST_BODY_ACCEL", POSE, r"->f\[0x154 / 4\] = " + F),
    ("TWIST_BODY_DAMP", POSE, r"->f\[0x158 / 4\] = " + F),
    ("STRAFE_SIDE_TICKS", STRAFE, r"\*\(short \*\)\(p\+0x146e\)=" + I + r";"),
    # The wrench (func_L00_002BA7D8): the lock-on turn of the swings (case 0x13) and the throw's
    # (0x15), and the slam's drive (0x14).
    ("WRENCH_TURN_ACCEL", STATE_MACHINE, r"func_L00_002B3EF8\(0, " + F + r", 0\.2f, 0\.2821616f\);\s*\} else \{\s*var_v1_3 = 1;"),
    ("WRENCH_TURN_DAMP", STATE_MACHINE, r"func_L00_002B3EF8\(0, 0\.05f, " + F + r", 0\.2821616f\);\s*\} else \{\s*var_v1_3 = 1;"),
    ("WRENCH_TURN_LIMIT", STATE_MACHINE, r"func_L00_002B3EF8\(0, 0\.05f, 0\.2f, " + F + r"\);\s*\} else \{\s*var_v1_3 = 1;"),
    ("THROW_TURN_ACCEL", STATE_MACHINE, r"func_L00_002B3EF8\(1, " + F + r", 0\.2f, 0\.2821616f\)"),
    ("SLAM_DRIVE_HEIGHT", STATE_MACHINE, r"f\[0x31C/4\] > " + F + r"\)\) \{\s*var_s1_2 = 1;"),
    ("SLAM_DRIVE_TICKS", STATE_MACHINE, r"\(var_s1_2 != 0\) && \(\(\(Hero \*\)D_00189E20\)->i\[0x1B8/4\] < " + I + r"\)"),
    ("SLAM_DRIVE", STATE_MACHINE, r"i\[0x1B8/4\] < 0x46\)\) \{\s*\(\(Hero \*\)D_00189E20\)->f\[0x1B0/4\] = " + F),
    # Leaving a swing and the throw (func_L00_002CB3A8 cases 0x13 and 0x15).
    ("THROW_RUN_FRAME", DECIDER, r"if \(\(FLD\(D_00189E20, f32, 0xC38\) > " + F + r"\) && \(FLD\(D_00189E20, f32, 0x24B8\) > [0-9.]+f\)\) \{\s*func_L00_002C42E0\(2, 0\);"),
    ("THROW_RUN_STICK", DECIDER, r"0xC38\) > [0-9.]+f\) && \(FLD\(D_00189E20, f32, 0x24B8\) > " + F + r"\)\) \{\s*func_L00_002C42E0\(2, 0\);"),
    ("RUN_OUT_STICK", DECIDER, r"if \(\(f32\)lim < h\) \{\s*if \(FLD\(D_00189E20, f32, 0x24B8\) > " + F),
    ("EXIT_IDLE_FRAMES", DECIDER, r"if \(\(f32\)\(lim \+ " + I + r"\) < h\)"),
    ("THROW_JUMP_WINDOW", DECIDER, r"if \(st == 0x15\) \{\s*frames = " + I + r";"),
    # Square's windows (func_L00_002C9448): standing or running, in a jump, and in a swing.
    ("WRENCH_WINDOW", WRENCH, r"func_L00_0032EF10\(mask, " + I + r", &scratch\.input_age\)"),
    ("WRENCH_AIR_WINDOW", WRENCH, r"buffered_window = " + I + r";\s*if \(D_00189E20\.mode == 11"),
    ("WRENCH_AIR_LATE_WINDOW", WRENCH, r"D_00189E20\.mode == 14\)\s*buffered_window = " + I + r";"),
    ("CHAIN_WINDOW", WRENCH, r"action = action < 16 \? action : " + I + r";"),
    ("THROW_AGAIN_TICKS", WRENCH, r"D_00189E20\.mode == 21 && D_00189E20\.frame >= " + I + r"\)"),
    # Clank (DESIGN.md section 9): his run (func_L00_002BA7D8 cases 0x40 and 0x4C, rac1's 0x44 and
    # 0x50), idle (0x3F, rac1's 0x43), punch (0x4D, rac1's 0x51) and glide (0x4B, rac1's 0x4F).
    ("CLANK_RUN_SPEED", STATE_MACHINE, r"case 0x4C:\s+/\* switch 1 \*/\s+func_L00_002B39F0\(" + F),
    ("CLANK_TURN_LIMIT", STATE_MACHINE, r"var_f14 = " + F + r";\s*var_f12_21 = 0\.033f;"),
    ("CLANK_TURN_ACCEL", STATE_MACHINE, r"var_f14 = 0\.16580631f;\s*var_f12_21 = " + F),
    ("CLANK_TURNAROUND_LIMIT", STATE_MACHINE, r"\{\s*var_f14 = " + F + r";\s*var_f12_21 = 0\.037f;"),
    ("CLANK_TURNAROUND_ACCEL", STATE_MACHINE, r"var_f14 = 0\.19489513f;\s*var_f12_21 = " + F),
    ("CLANK_TURN_DAMP", STATE_MACHINE, r"func_L00_002B3EF8\(0, var_f12_21, " + F),
    ("CLANK_RUN_ACCEL", STATE_MACHINE, r"var_f12_21, 0\.15f, var_f14\);\s*func_L00_002B41B8\(" + F),
    ("CLANK_RUN_BRAKE", STATE_MACHINE, r"var_f12_21, 0\.15f, var_f14\);\s*func_L00_002B41B8\([0-9.]+f, " + F),
    ("CLANK_ALONG_SPEED", STATE_MACHINE, r"func_L00_002B41B8\(0\.005f, 0\.0055555557f\);\s*if \(\(\(Hero \*\)D_00189E20\)->f\[0x174/4\] < " + F),
    ("CLANK_TURNAROUND_ANGLE", STATE_MACHINE, r"b\[0x40C\] = 1U;\s*if \(\(" + ERR + r" > " + F + r"\) && \(\(\(Hero \*\)D_00189E20\)->f\[0x24B8/4\] > [0-9.]+f\)\) \{"),
    ("CLANK_TURNAROUND_STICK", STATE_MACHINE, r"b\[0x40C\] = 1U;\s*if \(\(" + ERR + r" > [0-9.]+f\) && \(\(\(Hero \*\)D_00189E20\)->f\[0x24B8/4\] > " + F),
    ("CLANK_FLICK_FROM", STATE_MACHINE, r"temp_f20_18 = func_L00_0032F038\(" + I + r"\);"),
    ("CLANK_FLICK_TO", STATE_MACHINE, r"func_00283C80\(temp_f20_18, func_L00_0032F038\(" + I + r"\)\)"),
    ("CLANK_FLICK_ANGLE", STATE_MACHINE, r"func_00283C80\(temp_f20_18, func_L00_0032F038\(2\)\) > " + F),
    ("CLANK_FLICK_ERROR", STATE_MACHINE, r"func_L00_0032F038\(2\)\) > [0-9.]+f\) && \(" + ERR + r" > " + F),
    ("CLANK_FLICK_STICK", STATE_MACHINE, r"func_L00_0032F038\(2\)\) > [0-9.]+f\) && \(" + ERR + r" > [0-9.]+f\) && \(\(\(Hero \*\)D_00189E20\)->f\[0x24B8/4\] > " + F),
    ("CLANK_ALONG_CLEAR", STATE_MACHINE, r"func_L00_0032F038\(2\)\)[^\n]*\n[^\n]*\n[^\n]*\n\s*\}\s*if \(" + ERR + r" < " + F),
    ("CLANK_TURNAROUND_CLEAR", STATE_MACHINE, r"b\[0x40C\] = 0U;\s*\}\s*if \(\(\(Hero \*\)D_00189E20\)->b\[0x40F\] != 0\) \{\s*if \(" + ERR + r" < " + F),
    ("CLANK_IDLE_BRAKE", STATE_MACHINE, r"case 0x43:\s+/\* switch 1 \*/\s+func_L00_002B9F00\(3\.7f, 0\.0f, 0\.35f\);\s*func_L00_002B39F0\(1\.0f, 0\);\s*[^\n]*\n\s*func_L00_002B41B8\(0\.0f, " + F),
    ("CLANK_PUNCH_TURN_LIMIT", STATE_MACHINE, r"func_L00_002B3EF8\(0, 0\.05f, 0\.2f, " + F + r"\);\s*\} else \{\s*func_L00_002B75A0\(5\.0f"),
    ("CLANK_PUNCH_ACCEL", STATE_MACHINE, r"func_L00_002B75A0\(5\.0f, 0\.78539824f, 0\.78539824f\);\s*\}\s*func_L00_002B41B8\(" + F),
    ("CLANK_PUNCH_BRAKE", STATE_MACHINE, r"func_L00_002B75A0\(5\.0f, 0\.78539824f, 0\.78539824f\);\s*\}\s*func_L00_002B41B8\([0-9.]+f, " + F),
    ("CLANK_GLIDE_SPEED", STATE_MACHINE, r"case 0x4B:\s+/\* switch 1 \*/\s+func_L00_002B39F0\(" + F),
    ("CLANK_GLIDE_TURN_LIMIT", STATE_MACHINE, r"func_L00_002B3EF8\(0, 0\.04f, 0\.2f, " + F + r"\);\s*func_L00_002B41B8\(0\.004166667f"),
    # His jump's entry values (func_L00_002C42E0 state 0x45, rac1's 0x49).
    ("CLANK_JUMP_HEIGHT", ENTRY, r"WORD\(0x2294\) == 0x45\) \{\s*REAL\(0x4A8\) = " + F),
    ("CLANK_TAKEOFF", ENTRY, r"WORD\(0x2294\) == 0x45\) \{\s*REAL\(0x4A8\) = [0-9.]+f; SHORT\(0x4AC\) = " + I + r";"),
    ("CLANK_APEX_FRAME", ENTRY, r"WORD\(0x2294\) == 0x45\) \{[^}]*?REAL\(0x4C8\) = " + F),
    ("CLANK_TARGET_FRAME", ENTRY, r"WORD\(0x2294\) == 0x45\) \{[^}]*?REAL\(0x4CC\) = " + F),
    ("CLANK_JUMP_HEIGHT_HELD", ENTRY, r"WORD\(0x2294\) == 0x45\) \{[^}]*?REAL\(0x50C\) = " + F),
    ("CLANK_JUMP_GRAVITY", ENTRY, r"WORD\(0x2294\) == 0x45\) \{[^}]*?REAL\(0x520\) = " + F),
    ("CLANK_AIR_STICK_SPEED", ENTRY, r"WORD\(0x2294\) == 0x45\) \{[^}]*?REAL\(0x4A4\) = " + F),
    # His windows (func_L00_002CB3A8 cases 0x3F and 0x45): X and Square standing or running, and
    # Square landing.
    ("CLANK_JUMP_WINDOW", DECIDER, r"func_L00_0032EF10\(0x40, " + I + r", NULL\) != 0\) \{\s*func_L00_002C42E0\(0x45, 1\);\s*break;\s*\}\s*if \(func_L00_0032EF10\(0x80, 9"),
    ("CLANK_SQUARE_WINDOW", DECIDER, r"func_L00_0032EF10\(0x80, " + I + r", NULL\) != 0\) \{\s*func_L00_002C42E0\(0x4D, 1\);\s*break;\s*\}\s*func_L00_002B39F0\(0, 1\.0f\);"),
    ("CLANK_LAND_SQUARE_WINDOW", DECIDER, r"0x31C\) < 0\.2f\) && \(func_L00_0032EF10\(0x80, " + I + r", NULL\)"),
]

# The attack records' frames (level 0 data): name, record and byte offset in the 0x2C-byte record.
RECORDS = [(f"REC{r}_{name}", r, off) for r in range(4)
           for name, off in (("FIRST", 0xC), ("OPEN", 0x10), ("JUMP", 0x14), ("EXIT", 0x18))]
# Clank's punches, records 7 and 8 (func_L00_002C42E0's case 0x4D takes 7 + the chain), also with
# the frame under which the next punch chains (+0x8) and the hit window (+0x1C to +0x20).
RECORDS += [(f"REC{r}_{name}", r, off) for r in (7, 8)
            for name, off in (("CHAIN", 0x8), ("FIRST", 0xC), ("OPEN", 0x10), ("JUMP", 0x14), ("EXIT", 0x18),
                              ("HIT_FROM", 0x1C), ("HIT_TO", 0x20))]
RECORD_TABLE = 0x25D910  # D_L00_0025D990 on v1.01, 0x80 lower on v2.00, which the dumps are of

# The follow camera's numbers (level 0's lit): name, v2.00 address and the v1.01 name rac2-decomp
# gives.
CAMERA = [
    ("CAMERA_SPEED_SLOW", 0x1B1558, "D_L00_001B15B8[0]"),
    ("CAMERA_SPEED_MEDIUM", 0x1B155C, "D_L00_001B15B8[1]"),
    ("CAMERA_SPEED_FAST", 0x1B1560, "D_L00_001B15B8[2]"),
    ("CAMERA_PITCH_TRACKING", 0x1B1458, "D_L00_001B14B8"),
    ("CAMERA_PITCH_RETURN", 0x1B145C, "D_L00_001B14BC"),
    ("CAMERA_PITCH_STEP", 0x1B1460, "D_L00_001B14C0"),
    ("CAMERA_YAW_SPRING_ACCEL", 0x1B1488, "D_L00_001B14E8"),
    ("CAMERA_YAW_SPRING_DAMP", 0x1B148C, "D_L00_001B14EC"),
    ("CAMERA_PITCH_SPRING_ACCEL", 0x1B1490, "D_L00_001B14F0"),
    ("CAMERA_PITCH_SPRING_DAMP", 0x1B1494, "D_L00_001B14F4"),
]
LIT_BASE = 0x1A7C80


def extract(decomp: Path) -> list[tuple[str, str, str]]:
    """Return each spec's literal and where it is; exit naming any spec that matched nothing."""
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
    """Return the run speed, D_001A7DB0[4] from a level 0 lit.bin, or else the table's row."""
    if lit is not None:
        value = struct.unpack_from("<f", lit.read_bytes(), 0x1A7DC0 - 0x1A7C80)[0]
        return ("RUN_SPEED", f"{value:.6g}f", "level 0 data D_001A7DB0[4] (0x1A7DC0)")

    for line in TABLE.read_text(encoding="utf-8").splitlines():
        if line.startswith("RUN_SPEED\t"):
            name, literal, source = line.split("\t")
            return (name, literal, source)

    sys.exit("no RUN_SPEED: pass --lit <level 0 lit.bin>")


def shortest(value: float) -> str:
    """Return a float32 as the shortest literal that reads back as the same value, with its f."""
    for digits in range(1, 10):
        text = f"{value:.{digits}g}"

        if struct.unpack("<f", struct.pack("<f", float(text)))[0] == value:
            return text + "f"

    return f"{value:.9g}f"


def kept_rows(names: list[str], flag: str) -> list[tuple[str, str, str]]:
    """Return the table's rows of these names; exit asking for `flag` if any is missing."""
    kept = {}

    for line in TABLE.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            name, literal, source = line.split("\t")
            kept[name] = (name, literal, source)

    missing = [name for name in names if name not in kept]

    if missing:
        sys.exit("no " + ", ".join(missing) + f": pass {flag}")

    return [kept[name] for name in names]


def camera(lit: Path | None) -> list[tuple[str, str, str]]:
    """Return the follow camera's rows from a level 0 lit.bin, or else the table's rows."""
    if lit is None:
        return kept_rows([name for name, _, _ in CAMERA], "--lit <level 0 lit.bin>")

    raw = lit.read_bytes()
    rows = []

    for name, at, v101 in CAMERA:
        value = struct.unpack_from("<f", raw, at - LIT_BASE)[0]
        rows.append((name, shortest(value), f"level 0 data {v101} (v2.00 {at:#x})"))

    return rows


def attack_records(data: Path | None) -> list[tuple[str, str, str]]:
    """Return the frames of records 0 to 3, 7 and 8 from a level 0 data.bin, or the table's rows."""
    if data is not None:
        manifest = json.loads((data.parent / "manifest.json").read_text(encoding="utf-8"))
        base = next(s["address"] for s in manifest["sections"] if s["name"] == "data")
        raw = data.read_bytes()
        rows = []

        for name, record, offset in RECORDS:
            at = RECORD_TABLE + record * 0x2C + offset
            value = struct.unpack_from("<i", raw, at - base)[0]
            rows.append((name, str(value), f"level 0 data D_L00_0025D990[{record}]+{offset:#x} (v2.00 {at:#x})"))

        return rows

    kept = {}

    for line in TABLE.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            name, literal, source = line.split("\t")
            kept[name] = (name, literal, source)

    missing = [name for name, _, _ in RECORDS if name not in kept]

    if missing:
        sys.exit("no " + ", ".join(missing) + ": pass --data <level 0 data.bin>")

    return [kept[name] for name, _, _ in RECORDS]


def main() -> int:
    """Write the table from the decompilation and the dumps given, and say how many rows."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--decomp", type=Path, default=ROOT.parent / "rac2-decomp",
                        help="a rac2-decomp checkout (default: ../rac2-decomp)")
    parser.add_argument("--lit", type=Path, help="level 0's lit.bin, for the run speed and the camera")
    parser.add_argument("--data", type=Path, help="level 0's data.bin, for the attack records")
    args = parser.parse_args()

    rows = [run_speed(args.lit)] + extract(args.decomp) + attack_records(args.data) + camera(args.lit)
    TABLE.parent.mkdir(parents=True, exist_ok=True)
    with TABLE.open("w", encoding="utf-8", newline="\n") as out:
        out.write("# name\tliteral\tsource (written by tools/rac2_constants.py)\n")
        for row in rows:
            out.write("\t".join(row) + "\n")

    print(f"{len(rows)} constants into {TABLE.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
