#!/usr/bin/env python3
"""Enumerate signal, idle, timeout, and cancellation syntax in changed legacy C paths."""

import csv
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

from inventory_call_boundaries import REVISION

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-signal-source-candidates.tsv"
CALL = re.compile(r"\b(?P<operation>g_signal_connect\w*|g_idle_add\w*|g_timeout_add\w*|g_source_set_callback|g_source_remove)\s*\(")
HUNK = re.compile(r"-> (\d+)(?:,(\d+))?")


def read(name):
    with (ROOT / name).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def main():
    paths = {r["path"] for r in read("changed-files.tsv")
             if r["path"].startswith("app/") and r["path"].endswith(".c") and r["source_git_blob"]}
    assert len(paths) == 220
    hunks = defaultdict(list)
    for row in read("changed-hunks.tsv"):
        if row["path"] in paths:
            match = HUNK.search(row["old_new_lines"])
            if match:
                hunks[row["path"]].append((int(match[1]), int(match[2] or 1), row["child_id"]))
    rows = []
    for path in sorted(paths):
        source = subprocess.check_output(["git", "show", f"{REVISION}:{path}"], text=True, errors="replace")
        for line_no, line in enumerate(source.splitlines(), 1):
            if line.lstrip().startswith(("//", "*", "#")):
                continue
            for match in CALL.finditer(line):
                hunk = next((id for start, length, id in hunks[path]
                             if start <= line_no < start + length), "-")
                op = match["operation"]
                role = "CANCEL" if op == "g_source_remove" else "DEFERRED_SOURCE" if op.startswith(("g_idle", "g_timeout", "g_source_set_callback")) else "SIGNAL"
                rows.append((f"{path}:{line_no}", op, role, hunk, line.strip(), "REVIEW"))
    assert len(rows) == 517, len(rows)
    assert len({r[0] for r in rows}) == len(rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "role", "changed_hunk", "source_line", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C signal/source sites in {len(paths)} changed C paths: {dict(Counter(r[2] for r in rows))}; {sum(r[3] != '-' for r in rows)} in changed hunks")


if __name__ == "__main__":
    main()
