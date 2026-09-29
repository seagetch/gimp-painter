#!/usr/bin/env python3
"""Enumerate legacy C++ reference and pointer lifetime syntax for 01.007."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION

OUTPUT = Path("migration/inventory/cpp-reference-candidates.tsv")
PATTERNS = (
    ("CXX_POINTER", re.compile(r"\bCXXPointer\s*<")),
    ("G_VALUE", re.compile(r"\bGValue\b|\bg_value_(?:init|unset|set_object|dup_object)\s*\(")),
    ("REF_UNREF", re.compile(r"\bg_object_(?:ref|ref_sink|unref)\s*\(")),
    ("HOLD", re.compile(r"\bhold\s*\(")),
)


def main():
    paths = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", REVISION, "app"],
                                    text=True).splitlines()
    paths = [path for path in paths if path.endswith((".cpp", ".hpp", ".cc", ".hh", ".cxx"))]
    assert len(paths) == 71
    rows = []
    for path in paths:
        source = subprocess.check_output(["git", "show", f"{REVISION}:{path}"],
                                         text=True, errors="replace")
        for number, line in enumerate(source.splitlines(), 1):
            if line.lstrip().startswith(("//", "*", "#")):
                continue
            for kind, pattern in PATTERNS:
                if pattern.search(line):
                    rows.append((f"{path}:{number}", kind, line.strip(), "REVIEW"))
    counts = Counter(row[1] for row in rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "kind", "source_line", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C++ lifetime syntax candidates: {dict(counts)}")


if __name__ == "__main__":
    main()
