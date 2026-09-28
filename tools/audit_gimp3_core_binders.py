#!/usr/bin/env python3
"""Compare the remaining in-tree Undo, canvas, and data binder slots."""

import csv
import subprocess
from collections import Counter
from pathlib import Path

from audit_gimp3_layer_vfuncs import signature
from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-core-binder-review.tsv"
OWNERS = {
    "app/core/gimpclonelayerundo.cpp": ("app/core/gimpundo.h", "08.004", {"pop"}),
    "app/display/gimpcanvasperspectiveguide.cpp":
        ("app/display/gimpcanvasitem.h", "26.005", {"draw", "get_extents"}),
    "app/presets/gimpjsonresource.cpp":
        ("app/core/gimpdata.h", "28.002", {"save", "get_extension", "duplicate"}),
}


def main():
    with (ROOT / "binder-site-review.tsv").open(encoding="utf-8", newline="") as file:
        inventory = list(csv.DictReader(file, delimiter="\t"))
    selected = [row for row in inventory if row["role"] == "VFUNC_BINDING"
                and row["legacy_site"].rsplit(":", 1)[0] in OWNERS
                and row["slot"] in OWNERS[row["legacy_site"].rsplit(":", 1)[0]][2]]
    assert len(selected) == 6, len(selected)
    cache = {}

    def source(revision, path):
        if (revision, path) not in cache:
            cache[revision, path] = (subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
                if revision == "old" else Path(path).read_text(encoding="utf-8"))
        return cache[revision, path]

    tasks = Path("tasks.md").read_text(encoding="utf-8")
    rows = []
    for item in selected:
        site = item["legacy_site"]
        path, task, _ = OWNERS[site.rsplit(":", 1)[0]]
        slot = item["slot"]
        old, old_line = signature(source("old", path), slot)
        new, new_line = signature(source("new", path), slot)
        assert old != "-" and new != "-", (site, slot)
        assert f"| {task} |" in tasks, task
        rows.append((site, slot, f"{path}:{old_line}", old,
                     f"{path}:{new_line}", new,
                     "SAME_SIGNATURE" if old == new else "CHANGED_SIGNATURE",
                     task, "DONE"))

    counts = Counter(row[6] for row in rows)
    assert counts == {"SAME_SIGNATURE": 3, "CHANGED_SIGNATURE": 3}, counts
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_binding", "slot", "legacy_header", "legacy_signature",
                         "gimp3_header", "gimp3_signature", "classification",
                         "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} core binder slots compared: {dict(counts)}")


if __name__ == "__main__":
    main()
