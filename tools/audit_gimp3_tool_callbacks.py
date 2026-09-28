#!/usr/bin/env python3
"""Audit direct MyPaint tool callback definitions against GIMP 3 tool slots."""

import csv
import subprocess
from pathlib import Path

from audit_gimp3_brush_callbacks import (GOBJECT, NEW_GOBJECT, OLD_GOBJECT,
                                         callback_definition)
from audit_gimp3_external_binders import SLOTS
from audit_gimp3_layer_vfuncs import signature
from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-tool-callback-review.tsv"


def main():
    with (ROOT / "vfunc-assignment-review.tsv").open(encoding="utf-8", newline="") as file:
        inventory = list(csv.DictReader(file, delimiter="\t"))
    entries = [row for row in inventory if row["role"] != "CLASS_METADATA"
               and row["assignment_site"].startswith("app/tools/gimpmypainttool.cpp:")]
    assert len(entries) == 10
    cache = {}

    def source(path):
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
        return cache[path]

    tasks = Path("tasks.md").read_text(encoding="utf-8")
    rows = []
    for item in entries:
        slot = item["slot"]
        definition = item["target_definition"]
        path, number = definition.rsplit(":", 1)
        actual = callback_definition(source(path), int(number), item["assigned_value"])
        if slot == "constructed":
            old_ref, old, new_ref, new = SLOTS["constructed"]
        elif slot == "finalize":
            old_line, new_line, old = GOBJECT[slot]
            old_ref, new_ref, new = (f"{OLD_GOBJECT}#L{old_line}",
                                     f"{NEW_GOBJECT}#L{new_line}", old)
        else:
            header = ("app/tools/gimpdrawtool.h" if slot == "draw" else
                      "app/tools/gimptool.h")
            old, old_line = signature(source(header), slot)
            new, new_line = signature(Path(header).read_text(encoding="utf-8"), slot)
            old_ref, new_ref = f"{header}:{old_line}", f"{header}:{new_line}"
        assert actual == old == new, (definition, actual, old, new)
        task = "08.008/tool-core-lifetime" if slot in {"constructed", "finalize"} else "08.008"
        assert f"| {task} |" in tasks, task
        rows.append((item["assignment_site"], definition, actual, slot,
                     old_ref, old, new_ref, new, "SAME_SIGNATURE", task, "DONE"))

    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_assignment", "legacy_definition", "definition_signature",
                         "slot", "legacy_header", "legacy_signature", "gimp3_header",
                         "gimp3_signature", "classification", "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} MyPaint tool callbacks compared: all signatures unchanged")


if __name__ == "__main__":
    main()
