#!/usr/bin/env python3
"""Enumerate GObject data/qdata calls in legacy C files changed by painter."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from audit_cpp_object_data_sites import CALL, split_args
from inventory_call_boundaries import REVISION

OUTPUT = Path("migration/inventory/c-object-data-candidates.tsv")


def main():
    with Path("migration/inventory/changed-files.tsv").open(encoding="utf-8", newline="") as file:
        paths = [row["path"] for row in csv.DictReader(file, delimiter="\t")
                 if row["path"].startswith("app/") and row["path"].endswith(".c")
                 and row["source_git_blob"]]
    assert len(paths) == 220
    rows = []
    for path in paths:
        source = subprocess.check_output(["git", "show", f"{REVISION}:{path}"],
                                         text=True, errors="replace")
        lines = source.splitlines()
        for match in CALL.finditer(source):
            line = source.count("\n", 0, match.start()) + 1
            if lines[line - 1].lstrip().startswith(("//", "*", "#")):
                continue
            operation = match["op"]
            args = split_args(source, match.end())
            expected = 4 if operation.endswith("_full") else 3 if "set" in operation else 2
            assert len(args) == expected, (path, line, operation, args)
            rows.append((f"{path}:{line}", operation, args[0], args[1],
                         args[2] if len(args) > 2 else "-",
                         args[3] if len(args) > 3 else "-", "REVIEW"))
    counts = Counter(row[1] for row in rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "owner_expr", "key_expr",
                         "value_expr", "destroy_expr", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} candidate calls in {len(paths)} changed C paths: {dict(counts)}")


if __name__ == "__main__":
    main()
