#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""rac2.h against tests/data/rac2_constants.tsv: every RAC2_ literal is rac2-decomp's, as written.

    python3 tests/rac2_constants_test.py

tools/rac2_constants.py reads the table out of rac2-decomp (each row has its file and line). This
check needs neither the decompilation nor the disc: it compares the committed table with rac2.h, so
a literal mistyped in rac2.h, or one added there without a row, fails. Every row must be used.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "src" / "rac1" / "rac2.h"
TABLE = ROOT / "tests" / "data" / "rac2_constants.tsv"


def main() -> int:
    # A float literal, or a count (decimal or hexadecimal).
    defined = dict(re.findall(r"^#define RAC2_(\w+) (-?[0-9.]+f|0x[0-9A-Fa-f]+|[0-9]+)\s*$", HEADER.read_text(), re.M))
    table = {}

    for line in TABLE.read_text().splitlines():
        if line and not line.startswith("#"):
            name, literal, source = line.split("\t")
            table[name] = (literal, source)

    errors = []

    for name, (literal, source) in table.items():
        if name not in defined:
            errors.append(f"RAC2_{name} ({source}) is not in rac2.h")
        elif defined[name] != literal:
            errors.append(f"RAC2_{name} is {defined[name]} in rac2.h, {literal} at {source}")

    for name in defined:
        if name not in table:
            errors.append(f"RAC2_{name} has no row in {TABLE.name}: run tools/rac2_constants.py")

    for error in errors:
        print(error)

    print(f"rac2_constants: {len(table)} rows, {'FAILED' if errors else 'ok'}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
