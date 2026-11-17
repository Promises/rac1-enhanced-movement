#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs one scripted play-test of the mod in OpenRAC's rac1 port, hidden and silent. The port's
# development switches (OpenRAC docs/port/RAC1_PAL_TESTING.md, DEBUGGING.md, REGRESSION.md) start a
# level, press the pad on a schedule counted from control, and write one JSON line a tick
# (OPENRAC_TRACE: the hero's position hp, rotation hr and state hs, and the camera's forward fw).
# PLAYTEST.md has the routes and what each run showed.
#
#   tools/playtest.sh OUT NAME LEVEL SETTINGS [VAR=VALUE...]
#
# OUT       a scratch folder, created if needed, for NAME.jsonl, NAME.log and the plugins list
# SETTINGS  none (no mods), or the mod's settings as JSON: '{"ground": true, ...}'
# VAR=VALUE more of the port's switches; ROUTE= (OPENRAC_PRESS entries) and CF= (frames after
#           control) replace the standard route
#
# The environment picks the port and its data: DISC (ntsc, the default, or pal; runs
# openrac-rac1-DISC), OPENRAC (an OpenRAC checkout with port/build/release built, default
# ../OpenRAC), DATA (the disc's data folder as the launcher or tools/extractor.py writes it, default
# ~/OpenRAC/active/rac1/data) and PACKAGE (the mod package to run, default this checkout's
# build/package/rac1_enhanced_movement; point it at another build to compare the two). LEVELS (an
# export of the levels, OpenRAC's assets/port/rac1/ntsc) makes the port draw the world for
# OPENRAC_SHOT pictures; the movement does not need it.
set -euo pipefail

HERE=$(cd "$(dirname "$0")/.." && pwd)
DISC=${DISC:-ntsc}
OPENRAC=${OPENRAC:-$HERE/../OpenRAC}
DATA=${DATA:-$HOME/OpenRAC/active/rac1/data}
OUT=$1
NAME=$2
LEVEL=$3
SETTINGS=$4
shift 4

# The standard route, in frames from control. LX:LY run from 0 to 255, 128 centred and 0 left or
# up; the masks are 100 for L2 and 4000 for Cross. It has a full-stick run, letting go, half stick,
# a turnaround, a running X, strafing left, right and back on L2, a strafe flip, the camera turned
# away, and L2 held standing.
ROUTE="c30:0:90:128:0,c180:0:60:128:41,c260:0:50:128:0,c310:0:50:128:255,c400:0:60:128:0,c430:4000:5:128:0"
ROUTE="$ROUTE,c500:100:60:0:128,c560:100:60:255:128,c620:100:40:128:255,c680:100:10:0:128,c690:4100:6:0:128"
ROUTE="$ROUTE,c696:100:30:0:128,c760:0:40:128:128:255:128,c820:100:60:128:128"
CF=900

# ROUTE= and CF= arguments replace the standard route and its length.
for kv in "$@"; do
    case $kv in
        ROUTE=*) ROUTE=${kv#ROUTE=} ;;
        CF=*) CF=${kv#CF=} ;;
    esac
done

mkdir -p "$OUT"
MODS=(--no-mods)

# List the mod's package with the requested settings in a plugins file of the run's own.
if [ "$SETTINGS" != none ]; then
    mkdir -p "$OUT/plugins-$NAME"
    ln -sfn "${PACKAGE:-$HERE/build/package/rac1_enhanced_movement}" "$OUT/plugins-$NAME/rac1-enhanced-movement"
    printf '{"format": 1, "mods": [{"id": "rac1-enhanced-movement", "enabled": true, "settings": %s}]}\n' \
        "$SETTINGS" > "$OUT/plugins-$NAME/plugins.json"
    MODS=(--plugins "$OUT/plugins-$NAME/plugins.json")
fi

# Draw the world only when an export of the levels is given.
EXTRA=()
if [ -n "${LEVELS:-}" ]; then
    EXTRA=(--levels "$LEVELS")
fi

# Run the port hidden, with silent ticked audio and no frame cap, for at most 300 seconds.
env "$@" SDL_AUDIODRIVER=dummy OPENRAC_AUDIO_TICKED=1 OPENRAC_HIDDEN=1 OPENRAC_UNCAPPED=1 OPENRAC_DIRECT=1 \
    OPENRAC_PRESS="100:4000:5,k30:8:3,$ROUTE" OPENRAC_CONTROL_FRAMES="$CF" OPENRAC_TRACE="$OUT/$NAME.jsonl" \
    timeout 300 "$OPENRAC/port/build/release/openrac-rac1-$DISC" --data "$DATA" --window --no-card \
    --level "$LEVEL" --skip-movies --frames 12000 "${MODS[@]}" "${EXTRA[@]}" > "$OUT/$NAME.log" 2>&1 || true

# Report the ticks traced and whether control was reached.
echo "$NAME: $(grep -c . "$OUT/$NAME.jsonl") ticks, control $(grep -c 'control at' "$OUT/$NAME.log")"
