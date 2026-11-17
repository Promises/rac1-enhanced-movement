#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check openrac-mod.json against the code (GUIDE.md section 12, manifest tests).

    python3 tests/manifest_test.py

- id and version: those of src/version.h;
- games: the discs src/rac1/discs.h has rows for;
- settings: every key game.cpp reads, with the type it reads it as, and each default among its
  choices. The choices of strafe, camera and clank are the values strafe_buttons(), camera_choice()
  and clank_choice() name, plus the manifest default each falls back to. trace is each player's own;
- hooks: the ids of the hook point types game.cpp takes (openrac/games/rac1.hpp names them);
- the conflicts with lock-strafe and with the mod's old id, and the multiplayer kind.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The hook point types game.cpp may take, and their permanent ids.
HOOK_IDS = {
    "Pad_ProcessReport": "rac1.pad.process_report",
    "Hero_UpdateMotionInput": "rac1.player.update_motion_input",
    "Hero_TurnToHeading": "rac1.player.turn_to_heading",
    "Hero_SetPlanarVelocity": "rac1.player.set_planar_velocity",
    "FollowCamera_ApplyOverrides": "rac1.camera.follow.apply_overrides",
    "Hero_AdvanceAnimation": "rac1.player.advance_animation",
    "Hero_ApproachSpeed": "rac1.player.approach_speed",
}
TYPES = {"bool": "bool", "std::string": "choice"}


def main() -> int:
    """Check the manifest against the code and print the result."""
    manifest = json.loads((ROOT / "openrac-mod.json").read_text())
    version_h = (ROOT / "src" / "version.h").read_text()
    discs_h = (ROOT / "src" / "rac1" / "discs.h").read_text()
    game = (ROOT / "src" / "rac1" / "game.cpp").read_text()
    errors: list[str] = []

    def expect(ok: bool, message: str) -> None:
        """Record `message` as an error unless `ok`."""
        if not ok:
            errors.append(message)

    # The id and version the library reports.
    expect(f'"{manifest["id"]}"' in version_h, f"id {manifest['id']} is not version.h's")
    expect(f'"{manifest["version"]}"' in version_h, f"version {manifest['version']} is not version.h's")

    # The manifest's games are the discs discs.h has rows for.
    rows = set(re.findall(r'\{\s*"(\w+)",', discs_h))
    expect(set(manifest["games"]["rac1"]) == rows, f"games {manifest['games']['rac1']} against discs.h's {sorted(rows)}")
    expect(list(manifest["games"]) == ["rac1"], "the mod is for rac1 only")

    # The settings game.cpp reads, and their types.
    read = dict((key, TYPES[t]) for t, key in re.findall(r'setting<(bool|std::string)>\("(\w+)"\)', game))
    settings = {s["key"]: s for s in manifest["settings"]}
    expect(set(read) == set(settings), f"settings {sorted(settings)} against game.cpp's {sorted(read)}")

    for key, kind in read.items():
        if key in settings:
            expect(settings[key]["type"] == kind, f"{key} is {settings[key]['type']}, game.cpp reads {kind}")

    for key, s in settings.items():
        expect(bool(s.get("description")), f"{key} has no description")

        if s["type"] == "choice":
            values = [c["value"] for c in s["choices"]]
            expect(s["default"] in values, f"{key}'s default {s['default']} is not a choice")

    # Each choice setting's values: its function in game.cpp names all but the default it falls
    # back to.
    for key, function in (("strafe", "strafe_buttons"), ("camera", "camera_choice"), ("clank", "clank_choice")):
        body = re.search(function + r"\(const std::string& \w+\) \{(.*?)\n\}", game, re.S)
        named = set(re.findall(r'\w+ == "(\w+)"', body.group(1))) if body else set()
        values = {c["value"] for c in settings[key]["choices"]} if key in settings else set()
        expect(body is not None, f"no {function}() in game.cpp")
        expect(named | {settings.get(key, {}).get("default")} == values,
               f"{key} choices {sorted(values)} against {function}()'s {sorted(named)} and the default")
    expect(settings["trace"].get("multiplayer") == "own", "trace is each player's own")

    # The hooks: the types game.cpp takes, by id.
    taken = set(re.findall(r"hook<hp::(\w+)>", game))
    expect(all(t in HOOK_IDS for t in taken), f"a hook type without an id here: {sorted(taken - set(HOOK_IDS))}")
    expect(set(manifest["hooks"]) == {HOOK_IDS[t] for t in taken if t in HOOK_IDS},
           f"hooks {sorted(manifest['hooks'])} against game.cpp's {sorted(HOOK_IDS[t] for t in taken if t in HOOK_IDS)}")

    # The conflicts with lock-strafe and the old id, and the multiplayer kind.
    expect([c["id"] for c in manifest.get("conflicts", [])] == ["lock-strafe", "rac1-better-movement"],
           "conflicts with lock-strafe and with the mod's old id")
    expect(manifest.get("multiplayer") == "input", "multiplayer is input")

    for error in errors:
        print(error)

    print(f"manifest: {'FAILED' if errors else 'ok'}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
