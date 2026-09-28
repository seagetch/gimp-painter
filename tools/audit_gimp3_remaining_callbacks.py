#!/usr/bin/env python3
"""Reconcile bridge, Undo, options, and editor direct class callbacks."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from audit_gimp3_brush_callbacks import (GOBJECT, NEW_GOBJECT, OLD_GOBJECT,
                                         callback_definition, normalize_type)
from audit_gimp3_external_binders import SLOTS
from audit_gimp3_layer_vfuncs import signature
from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-remaining-callback-review.tsv"
EXTERNAL = {**GOBJECT,
            "dispose": (333, 343, "void(GObject*)"),
            "constructed": (344, 354, "void(GObject*)")}
INTERNAL = {
    "pop": "app/core/gimpundo.h",
    "free": "app/core/gimpundo.h",
    "set_data": "app/widgets/gimpdataeditor.h",
    "set_context": "app/widgets/gimpdocked.h",
}
OWNERS = {
    "app/base/glib-cxx-impl.hpp": 3,
    "app/paint/gimpmypaintcoreundo.cpp": 5,
    "app/paint/gimpmypaintoptions.cpp": 4,
    "app/widgets/gimpmypaintbrusheditor.cpp": 3,
}


def bridge_definition(source, line, symbol):
    """Read a static member defined at the source anchor, including its return type."""
    code = "\n".join(source.splitlines()[line - 1:line + 15])
    match = re.match(r"\s*static\s+(?P<result>(?:const\s+)?\w+\s*\*?)\s+" +
                     re.escape(symbol) + r"\s*\((?P<args>.*?)\)\s*\{", code, re.S)
    assert match, (line, symbol)
    args = []
    for arg in match["args"].split(","):
        if arg.strip() == "void":
            continue
        args.append(normalize_type(re.sub(r"\b[A-Za-z_]\w*\s*$", "", arg).strip()))
    return normalize_type(match["result"]) + "(" + ",".join(args) + ")"


def followup(path, slot):
    if path == "app/base/glib-cxx-impl.hpp":
        return "05.011" if slot == "finalize" else "05.013/legacy-exit-removal"
    if path == "app/paint/gimpmypaintcoreundo.cpp":
        return "09.011/mypaint-undo-stroke"
    if path == "app/paint/gimpmypaintoptions.cpp":
        return "08.007"
    return "24.001/editor-context-contract"


def main():
    with (ROOT / "vfunc-assignment-review.tsv").open(encoding="utf-8", newline="") as file:
        inventory = list(csv.DictReader(file, delimiter="\t"))
    selected = [row for row in inventory if row["role"] != "CLASS_METADATA"
                and row["assignment_site"].rsplit(":", 1)[0] in OWNERS]
    counts = Counter(row["assignment_site"].rsplit(":", 1)[0] for row in selected)
    assert counts == OWNERS, counts
    cache = {}

    def old_source(path):
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
        return cache[path]

    tasks = Path("tasks.md").read_text(encoding="utf-8")
    rows = []
    for item in selected:
        site, slot = item["assignment_site"], item["slot"]
        path = site.rsplit(":", 1)[0]
        definition = item["target_definition"]
        impl, number = definition.rsplit(":", 1)
        symbol = item["assigned_value"].split("::")[-1]
        actual = (bridge_definition(old_source(impl), int(number), symbol)
                  if path == "app/base/glib-cxx-impl.hpp" else
                  callback_definition(old_source(impl), int(number), symbol))
        if slot in EXTERNAL:
            old_line, new_line, old = EXTERNAL[slot]
            old_ref, new_ref, new = (f"{OLD_GOBJECT}#L{old_line}",
                                     f"{NEW_GOBJECT}#L{new_line}", old)
            if slot == "constructed":
                assert (old_ref, old, new_ref, new) == SLOTS["constructed"]
        else:
            header = INTERNAL[slot]
            old, old_line = signature(old_source(header), slot)
            new, new_line = signature(Path(header).read_text(encoding="utf-8"), slot)
            old_ref, new_ref = f"{header}:{old_line}", f"{header}:{new_line}"
        assert actual == old == new, (definition, actual, old, new)
        task = followup(path, slot)
        assert f"| {task} |" in tasks, task
        rows.append((site, definition, actual, slot, old_ref, old, new_ref,
                     new, "SAME_SIGNATURE", task, "DONE"))

    assert len(rows) == 15
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_assignment", "legacy_definition", "definition_signature",
                         "slot", "legacy_header", "legacy_signature", "gimp3_header",
                         "gimp3_signature", "classification", "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} remaining direct callbacks compared: all signatures unchanged")


if __name__ == "__main__":
    main()
