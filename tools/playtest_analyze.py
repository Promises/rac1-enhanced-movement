#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The standard play-test route (tools/playtest.sh), segment by segment: one run against another.

    python3 tools/playtest_analyze.py BASE.jsonl MOD.jsonl

For each segment of the route (frames from control), each run's states in order (id x ticks), its top
speed and how soon it reached 90% of it, the distance covered, how far the facing is from the
camera's yaw on average, and how far the movement is from the facing (strafing: about pi/2 sideways,
pi back). Speeds come from the position the port's trace (OPENRAC_TRACE) gives each tick.
"""
import json, math, sys

def load(path):
    rows = {}
    for line in open(path):
        r = json.loads(line)
        if r.get("c") is not None:
            rows[r["c"]] = r
    return rows

def seg(rows, a, b):
    out = []
    for c in range(a, b):
        r, p = rows.get(c), rows.get(c - 1)
        if not r or not p:
            continue
        dx, dy = r["hp"][0] - p["hp"][0], r["hp"][1] - p["hp"][1]
        speed = math.hypot(dx, dy) * 60
        move = math.atan2(dy, dx) if speed > 0.05 else None
        cam = math.atan2(r["fw"][1], r["fw"][0])
        out.append((c, r["hs"], speed, r["hr"][2], cam, move))
    return out

def wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi

def summary(name, s):
    states = []
    for _, st, *_ in s:
        if not states or states[-1][0] != st:
            states.append([st, 0])
        states[-1][1] += 1
    speeds = [x[2] for x in s]
    top = max(speeds) if speeds else 0
    t90 = next((x[0] - s[0][0] for x in s if x[2] >= 0.9 * top), None) if top > 0.5 else None
    dist = sum(speeds) / 60
    off = [abs(wrap(x[3] - x[4])) for x in s]
    side = [abs(wrap(x[5] - x[3])) for x in s if x[5] is not None]
    print(f"  {name:4} states {' '.join(f'{st:#x}x{n}' for st, n in states)}")
    print(f"       top {top:5.2f} u/s, 90% after {t90} ticks, distance {dist:5.2f}, "
          f"facing-camera mean {sum(off)/max(len(off),1):4.2f} rad, move-vs-facing mean "
          f"{(sum(side)/len(side)) if side else 0:4.2f} rad")

base, mod = load(sys.argv[1]), load(sys.argv[2])
for label, a, b in [
    ("full-stick run c30-120", 30, 120), ("let go c120-180", 120, 180), ("half stick c180-240", 180, 240),
    ("turnaround c310-360", 310, 360), ("run+X c400-470", 400, 470), ("strafe left c500-560", 500, 560),
    ("strafe right c560-620", 560, 620), ("strafe back c620-660", 620, 660), ("flip c680-760", 680, 760),
    ("camera turn c760-800", 760, 800), ("L2 standing c820-880", 820, 880),
]:
    print(label)
    summary("base", seg(base, a, b))
    summary("mod", seg(mod, a, b))
