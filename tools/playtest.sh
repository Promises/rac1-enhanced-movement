#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# One scripted play-test run of the mod in OpenRAC's rac1 NTSC-U port, hidden and silent: the
# port's own development switches (OpenRAC docs/port/RAC1_PAL_TESTING.md, DEBUGGING.md,
# REGRESSION.md) start a level, press the pad on a schedule counted from control, and write one JSON
# line a tick (OPENRAC_TRACE: the hero's position hp, rotation hr, state hs, the camera's forward fw).
# PLAYTEST.md has the routes and what each run showed.
#
#   tools/playtest.sh OUT NAME LEVEL SETTINGS [VAR=VALUE...]
#
# OUT       a scratch folder (made): the run's NAME.jsonl, NAME.log and its plugins list
# SETTINGS  none (no mods), or the mod's settings as JSON: '{"ground": true, ...}'
# VAR=VALUE more of the port's switches, and ROUTE= (OPENRAC_PRESS entries) and CF= (frames after
#           control) to replace the standard route
#
# The port and its data: OPENRAC (an OpenRAC checkout with port/build/release built, default
# ../OpenRAC), DATA (the launcher's rac1 data folder, default ~/OpenRAC/active/rac1/data). LEVELS
# (an export of the levels, OpenRAC's assets/port/rac1/ntsc) makes the port draw the world, for
# OPENRAC_SHOT pictures; the movement does not need it.
set -euo pipefail

HERE=$(cd "$(dirname "$0")/.." && pwd)
OPENRAC=${OPENRAC:-$HERE/../OpenRAC}
DATA=${DATA:-$HOME/OpenRAC/active/rac1/data}
OUT=$1
NAME=$2
LEVEL=$3
SETTINGS=$4
shift 4

# The standard route, in frames from control (LX:LY 0-255, 128 centred, 0 left or up; masks: 100 L2,
# 4000 Cross): a full-stick run, letting go, half stick, a turnaround, a running X, strafing left,
# right and back on L2, a strafe flip, the camera turned away, L2 held standing.
ROUTE="c30:0:90:128:0,c180:0:60:128:41,c260:0:50:128:0,c310:0:50:128:255,c400:0:60:128:0,c430:4000:5:128:0"
ROUTE="$ROUTE,c500:100:60:0:128,c560:100:60:255:128,c620:100:40:128:255,c680:100:10:0:128,c690:4100:6:0:128"
ROUTE="$ROUTE,c696:100:30:0:128,c760:0:40:128:128:255:128,c820:100:60:128:128"
CF=900

# The route and its length from the arguments.
for kv in "$@"; do
    case $kv in
        ROUTE=*) ROUTE=${kv#ROUTE=} ;;
        CF=*) CF=${kv#CF=} ;;
    esac
done

mkdir -p "$OUT"
MODS=(--no-mods)

# The mod from this checkout's build, listed with the settings asked for.
if [ "$SETTINGS" != none ]; then
    mkdir -p "$OUT/plugins-$NAME"
    ln -sfn "$HERE/build/package/rac1_better_movement" "$OUT/plugins-$NAME/rac1-better-movement"
    printf '{"format": 1, "mods": [{"id": "rac1-better-movement", "enabled": true, "settings": %s}]}\n' \
        "$SETTINGS" > "$OUT/plugins-$NAME/plugins.json"
    MODS=(--plugins "$OUT/plugins-$NAME/plugins.json")
fi

EXTRA=()
if [ -n "${LEVELS:-}" ]; then
    EXTRA=(--levels "$LEVELS")
fi

env "$@" SDL_AUDIODRIVER=dummy OPENRAC_AUDIO_TICKED=1 OPENRAC_HIDDEN=1 OPENRAC_UNCAPPED=1 OPENRAC_DIRECT=1 \
    OPENRAC_PRESS="100:4000:5,k30:8:3,$ROUTE" OPENRAC_CONTROL_FRAMES="$CF" OPENRAC_TRACE="$OUT/$NAME.jsonl" \
    timeout 300 "$OPENRAC/port/build/release/openrac-rac1-ntsc" --data "$DATA" --window --no-card \
    --level "$LEVEL" --skip-movies --frames 12000 "${MODS[@]}" "${EXTRA[@]}" > "$OUT/$NAME.log" 2>&1 || true

echo "$NAME: $(grep -c . "$OUT/$NAME.jsonl") ticks, control $(grep -c 'control at' "$OUT/$NAME.log")"
