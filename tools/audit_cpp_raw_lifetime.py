#!/usr/bin/env python3
"""Inventory C++ allocation, free, and weak pointer lifetime syntax."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION

OUTPUT = Path("migration/inventory/cpp-raw-lifetime-candidates.tsv")
PATTERNS = (
    ("NEW", re.compile(r"\bnew\s+[A-Za-z_][A-Za-z0-9_:<>]*")),
    ("DELETE", re.compile(r"\bdelete\s+[A-Za-z_]")),
    ("G_FREE", re.compile(r"\bg_free\s*\(")),
    ("WEAK_POINTER", re.compile(r"\bg_object_add_weak_pointer\s*\(")),
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
    print(f"{len(rows)} raw lifetime syntax candidates: {dict(counts)}")


if __name__ == "__main__":
    main()
