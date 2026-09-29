#!/usr/bin/env python3
"""Verify whether C reference candidates belong to actual added lines."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-reference-added-line-review.tsv"
HEADER = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@")


def added_lines(old_blob, new_blob, source_length):
    if not old_blob:
        return set(range(1, source_length + 1))
    diff = subprocess.check_output(
        ["git", "diff", "--unified=0", "--no-ext-diff", old_blob, new_blob],
        text=True, errors="replace", timeout=90)
    positions = set()
    number = 0
    for line in diff.splitlines():
        match = HEADER.match(line)
        if match:
            number = int(match[1])
        elif line.startswith("+") and not line.startswith("+++"):
            positions.add(number)
            number += 1
        elif line.startswith(" "):
            number += 1
    return positions


def main():
    with (ROOT / "changed-files.tsv").open(encoding="utf-8", newline="") as file:
        blobs = {row["path"]: row for row in csv.DictReader(file, delimiter="\t")}
    with (ROOT / "c-reference-hunk-review.tsv").open(encoding="utf-8", newline="") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    paths = sorted({row["legacy_site"].rsplit(":", 1)[0] for row in candidates})
    assert len(paths) == 21, len(paths)
    added = {}
    for path in paths:
        info = blobs[path]
        source = subprocess.check_output(["git", "show", info["source_git_blob"]],
                                         text=True, errors="replace")
        added[path] = added_lines(info["base_git_blob"], info["source_git_blob"],
                                  len(source.splitlines()))
    rows = []
    for row in candidates:
        path, number = row["legacy_site"].rsplit(":", 1)
        state = "ADDED" if int(number) in added[path] else "INHERITED_OR_CONTEXT"
        rows.append((row["legacy_site"], row["changed_hunk"], row["operation"],
                     state, row["syntax_role"], row["ownership_followup"], "DONE"))
    counts = Counter(row[3] for row in rows)
    assert len(rows) == 74, len(rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "changed_hunk", "operation", "line_origin",
                         "syntax_role", "ownership_followup", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C reference hunk sites verified: {dict(counts)}")


if __name__ == "__main__":
    main()
