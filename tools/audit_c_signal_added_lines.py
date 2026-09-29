#!/usr/bin/env python3
"""Distinguish added C signal/source lines from inherited hunk context."""

import csv
import subprocess
from collections import Counter
from pathlib import Path

from audit_c_reference_added_lines import added_lines

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-signal-added-line-review.tsv"


def read(name):
    with (ROOT / name).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def main():
    candidates = [r for r in read("c-signal-source-candidates.tsv") if r["changed_hunk"] != "-"]
    assert len(candidates) == 57
    blobs = {r["path"]: r for r in read("changed-files.tsv")}
    paths = {r["legacy_site"].rsplit(":", 1)[0] for r in candidates}
    added = {}
    for path in paths:
        info = blobs[path]
        source = subprocess.check_output(["git", "show", info["source_git_blob"]], text=True, errors="replace")
        added[path] = added_lines(info["base_git_blob"], info["source_git_blob"], len(source.splitlines()))
    rows = []
    for row in candidates:
        path, number = row["legacy_site"].rsplit(":", 1)
        origin = "ADDED" if int(number) in added[path] else "INHERITED_OR_CONTEXT"
        rows.append((row["legacy_site"], row["operation"], row["role"], row["changed_hunk"], origin, row["source_line"]))
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "role", "changed_hunk", "line_origin", "source_line"))
        writer.writerows(rows)
    print(f"{len(rows)} C hunk signal/source sites: {dict(Counter(r[4] for r in rows))}")


if __name__ == "__main__":
    main()
