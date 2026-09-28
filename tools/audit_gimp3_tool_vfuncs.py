#!/usr/bin/env python3
"""Compare the legacy paint and interactive tool binders with GIMP 3 slots."""

import csv
import subprocess
from collections import Counter
from pathlib import Path

from audit_gimp3_layer_vfuncs import signature
from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-tool-vfunc-review.tsv"
OWNERS = {
    "app/tools/gimpbucketfillbrushtool.cpp": ("fill-brush", "27.009/stroke-lifecycle"),
    "app/tools/gimpimagegeneratortool.cpp": ("image-generator", "31.011"),
    "app/tools/gimpperspectiveguidetool.cpp": ("perspective-guide", "08.010"),
}


def main():
    with (ROOT / "binder-site-review.tsv").open(encoding="utf-8", newline="") as stream:
        entries = list(csv.DictReader(stream, delimiter="\t"))
    selected = [row for row in entries if row["role"] == "VFUNC_BINDING"
                and row["legacy_site"].rsplit(":", 1)[0] in OWNERS]
    assert len(selected) == 18, len(selected)
    cache = {}

    def read(revision, path):
        if (revision, path) not in cache:
            cache[revision, path] = (subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
                if revision == "old" else Path(path).read_text(encoding="utf-8"))
        return cache[revision, path]

    rows = []
    for entry in selected:
        location = entry["legacy_site"]
        owner, task = OWNERS[location.rsplit(":", 1)[0]]
        slot = entry["slot"]
        path = ("app/paint/gimppaintcore.h" if owner == "fill-brush" else
                "app/tools/gimpdrawtool.h" if slot == "draw" else
                "app/tools/gimptool.h")
        old, old_line = signature(read("old", path), slot)
        new, new_line = signature(read("new", path), slot)
        assert old != "-" and new != "-", (location, path, old, new)
        assert f"| {task} |" in Path("tasks.md").read_text(encoding="utf-8"), task
        classification = "SAME_SIGNATURE" if old == new else "CHANGED_SIGNATURE"
        rows.append((location, owner, slot, f"{path}:{old_line}", old,
                     f"{path}:{new_line}", new, classification, task, "DONE"))
    counts = Counter(row[7] for row in rows)
    assert counts == {"SAME_SIGNATURE": 17, "CHANGED_SIGNATURE": 1}, counts
    assert rows[0][1] == "fill-brush" and rows[0][2] == "paint"
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_binding", "owner", "slot", "legacy_header",
                         "legacy_signature", "gimp3_header", "gimp3_signature",
                         "classification", "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} tool binder slots compared: {dict(counts)}")


if __name__ == "__main__":
    main()
