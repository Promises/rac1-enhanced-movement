#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The standard play-test route (tools/playtest.sh), segment by segment: one run against another.

    python3 tools/playtest_analyze.py BASE.jsonl MOD.jsonl
    python3 tools/playtest_analyze.py --states RUN.jsonl
    python3 tools/playtest_analyze.py --swings RUN.log
    python3 tools/playtest_analyze.py --camera RUN.jsonl START HOLD [START HOLD ...]
    python3 tools/playtest_analyze.py --same A.jsonl B.jsonl

For each segment of the route (frames from control), each run's states in order (id x ticks), its top
speed and how soon it reached 90% of it, the distance covered, how far the facing is from the
camera's yaw on average, and how far the movement is from the facing (strafing: about pi/2 sideways,
pi back). Speeds come from the position the port's trace (OPENRAC_TRACE) gives each tick.

--states lists one run's states in order with the frame from control each began on, its ticks, the
distance covered and the height gained: the wrench routes (PLAYTEST.md), on either game's port (rac2's
trace has the same lines). --swings reads the mod's own trace (OPENRAC_ENHANCED_MOVEMENT_TRACE=1, in
the run's log) and lists each swing of the wrench (states 0x13 to 0x15, a new one at each new attack
record or restart of the animation): its ticks, record, last frame, and the steers the mod made.

--camera measures the camera's turn (from the trace's forward row) through each press of the right
stick (START frames from control, held HOLD): the yaw's rate held (the mean of the hold's last third),
the ticks to half and 90% of it, the degrees coasted after letting go and the ticks to stop (under 1%
of the rate); and the pitch's swing, its fastest change and where it is 40 ticks after letting go.
--same counts the ticks from control where two runs differ in position, rotation, state or camera.
"""
import json, math, re, sys

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

def states(path):
    rows = load(path)
    seq = []
    for c in sorted(rows):
        r = rows[c]
        if not seq or seq[-1][1] != r["hs"]:
            seq.append([c, r["hs"], 0, r["hp"], r["hp"]])
        seq[-1][2] += 1
        seq[-1][4] = r["hp"]
    for c, st, n, a, b in seq:
        print(f"c{c:5} state {st:#5x} x{n:4}  moved {math.dist(a[:2], b[:2]):6.3f}  dz {b[2] - a[2]:+.3f}")

def swings(path):
    line = re.compile(r"state (0x[0-9a-f]+|0) .* anim ([-0-9.]+) rec (-?\d+) .*?square -?\d+ cross -?\d+(.*)$")
    cur = None
    def show(s):
        print(f"state {s[0]:#x} rec {s[1]} ticks {s[2]:3} last frame {s[3]:6.2f} {' '.join(sorted(s[4]))}")
    for text in open(path, errors="replace"):
        m = line.search(text)
        if not m:
            continue
        st, frame, rec, words = int(m[1], 16), float(m[2]), int(m[3]), m[4].split()
        if st not in (0x13, 0x14, 0x15):
            if cur:
                show(cur)
            cur = None
            continue
        if cur is None or cur[0] != st or cur[1] != rec or frame < cur[3] - 1.0:
            if cur:
                show(cur)
            cur = [st, rec, 0, frame, set()]
        cur[2] += 1
        cur[3] = frame
        cur[4].update(w for w in words if w.startswith(("swing-", "steer-", "square-", "wrench-", "slam-")))
    if cur:
        show(cur)

def camera(path, presses):
    rows = load(path)
    yaw = lambda c: math.atan2(rows[c]["fw"][1], rows[c]["fw"][0])
    pitch = lambda c: math.degrees(math.asin(max(-1.0, min(1.0, rows[c]["fw"][2]))))
    rate = lambda c: math.degrees(wrap(yaw(c) - yaw(c - 1))) * 60
    for start, hold in presses:
        rates = [rate(c) for c in range(start, start + hold + 1)]
        tail = rates[2 * len(rates) // 3:]
        held = sum(tail) / len(tail)
        sign = 1 if held >= 0 else -1
        half = next((k for k, v in enumerate(rates) if sign * v >= 0.5 * abs(held)), None)
        most = next((k for k, v in enumerate(rates) if sign * v >= 0.9 * abs(held)), None)
        coast, stop = 0.0, None
        for k in range(1, 90):
            c = start + hold + k
            if c not in rows or sign * rate(c) < 0.01 * abs(held):
                stop = k if c in rows else None
                break
            coast += rate(c) / 60
        swing = [pitch(c) for c in range(start, start + hold + 1)]
        before = pitch(start - 1)
        far = max(swing, key=lambda v: abs(v - before))
        fastest = max(abs(swing[k] - swing[k - 1]) for k in range(1, len(swing))) * 60
        after = pitch(start + hold + 40) if start + hold + 40 in rows else float("nan")
        print(f"c{start}+{hold}: yaw {held:6.1f} deg/s, half after {half} ticks, 90% after {most}, coast "
              f"{coast:5.1f} deg, stopped {stop} ticks after; pitch {before:5.1f} to {far:5.1f}, fastest "
              f"{fastest:5.1f} deg/s, {after:5.1f} 40 ticks after")

def same(a_path, b_path):
    a, b = load(a_path), load(b_path)
    both = sorted(set(a) & set(b))
    differ = [c for c in both if any(a[c].get(f) != b[c].get(f) for f in ("hp", "hr", "hs", "cam", "fw"))]
    print(f"{len(both)} ticks, {len(differ)} differ" + (f", the first at c{differ[0]}" if differ else ""))

if sys.argv[1] == "--camera":
    args = list(map(int, sys.argv[3:]))
    camera(sys.argv[2], list(zip(args[::2], args[1::2])))
    sys.exit(0)
if sys.argv[1] == "--same":
    same(sys.argv[2], sys.argv[3])
    sys.exit(0)
if sys.argv[1] == "--states":
    states(sys.argv[2])
    sys.exit(0)
if sys.argv[1] == "--swings":
    swings(sys.argv[2])
    sys.exit(0)

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
