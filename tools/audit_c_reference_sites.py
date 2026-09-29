#!/usr/bin/env python3
"""Inventory reference operations in changed legacy C paths and hunks."""

import csv
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

from inventory_call_boundaries import REVISION

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-reference-candidates.tsv"
CALL = re.compile(r"\b(?P<op>g_object_(?:ref|ref_sink|unref)|g_value_(?:init|unset|copy|transform|set_object|dup_object))\s*\(")
HUNK = re.compile(r"-> (\d+)(?:,(\d+))?")


def main():
    with (ROOT / "changed-files.tsv").open(encoding="utf-8", newline="") as file:
        paths = [row["path"] for row in csv.DictReader(file, delimiter="\t")
                 if row["path"].startswith("app/") and row["path"].endswith(".c")
                 and row["source_git_blob"]]
    assert len(paths) == 220
    hunks = defaultdict(list)
    with (ROOT / "changed-hunks.tsv").open(encoding="utf-8", newline="") as file:
        for row in csv.DictReader(file, delimiter="\t"):
            if row["path"] not in paths:
                continue
            match = HUNK.search(row["old_new_lines"])
            if match:
                hunks[row["path"]].append((int(match[1]), int(match[2] or 1), row["child_id"]))
    rows = []
    for path in paths:
        source = subprocess.check_output(["git", "show", f"{REVISION}:{path}"],
                                         text=True, errors="replace")
        for number, line in enumerate(source.splitlines(), 1):
            if line.lstrip().startswith(("//", "*", "#")):
                continue
            for match in CALL.finditer(line):
                matched = next((id for start, count, id in hunks[path]
                                if start <= number < start + count), "-")
                rows.append((f"{path}:{number}", match["op"], matched,
                             line.strip(), "REVIEW"))
    counts = Counter("IN_HUNK" if row[2] != "-" else "CONTEXT" for row in rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "changed_hunk", "source_line", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} reference candidates in {len(paths)} C paths: {dict(counts)}")


if __name__ == "__main__":
    main()
