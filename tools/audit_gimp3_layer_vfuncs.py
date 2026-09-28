#!/usr/bin/env python3
"""Compare CloneLayer and FilterLayer vfunc slots to the GIMP 3 C headers."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-layer-vfunc-review.tsv"
LAYERS = {"app/core/gimpclonelayer.cpp": "clone",
          "app/core/gimpfilterlayer.cpp": "filter"}
ITEM_SLOTS = {"duplicate", "translate", "scale", "resize", "flip", "rotate",
              "transform", "is_editable"}
PROGRESS_SLOTS = {"start", "end", "is_active", "set_text", "set_value",
                  "get_value", "pulse", "message"}


def header(slot):
    if slot == "constructed":
        return None
    basename = ("gimpobject.h" if slot == "get_memsize" else
                "gimpviewable.h" if slot == "get_size" else
                "gimpitem.h" if slot in ITEM_SLOTS else
                "gimpdrawable.h" if slot in {"estimate_memsize", "project_region"} else
                "gimppickable.h" if slot == "get_opacity_at" else
                "gimpprogress.h" if slot in PROGRESS_SLOTS else None)
    assert basename, slot
    return "app/core/" + basename


def signature(source, slot):
    pattern = re.compile(r"(?m)^[ \t]*(?P<result>[A-Za-z_]\w*(?:\s*\*)?)\s*"
                         r"\(\s*\*\s*" + re.escape(slot) +
                         r"\s*\)\s*\((?P<args>.*?)\)\s*;", re.S)
    matches = list(pattern.finditer(source))
    assert len(matches) <= 1, (slot, len(matches))
    if not matches:
        return "-", "-"
    match = matches[0]
    result = re.sub(r"\s*\*\s*", "*", match["result"].strip())
    args = []
    for parameter in match["args"].split(","):
        name = parameter.strip()
        if name == "void":
            continue
        # The audited GIMP class fields have simple C parameter lists.
        assert re.search(r"\s|\*", name), (slot, name)
        name = re.sub(r"\b[A-Za-z_]\w*\s*$", "", name).strip()
        name = re.sub(r"\s*\*\s*", "*", name)
        name = re.sub(r"\s+", " ", name)
        args.append(name)
    text = result + "(" + ",".join(args) + ")"
    return text, str(source.count("\n", 0, match.start()) + 1)


def followup(layer, slot):
    if slot == "constructed":
        return "06.010"
    if slot == "is_editable":
        return ("14.006" if layer == "clone" else "15.006") + "/editability-contract"
    if slot == "get_opacity_at":
        return ("14.003/clone-pickable-opacity" if layer == "clone"
                else "15.006/filter-pickable-opacity")
    if slot == "project_region":
        return "15.006/gegl-result-source"
    if slot == "start":
        return "15.006/filter-progress-start"
    if slot == "estimate_memsize":
        return ("14.003/gegl-source-node" if layer == "clone"
                else "15.006/gegl-result-source")
    if slot == "duplicate":
        return "14.010" if layer == "clone" else "15.010"
    if slot in {"translate", "scale", "resize", "flip", "rotate", "transform"}:
        return "14.006" if layer == "clone" else "15.005"
    if slot == "get_size":
        return "14.005"
    if slot in PROGRESS_SLOTS:
        return "06.016"
    return "06.014"


def main():
    with (ROOT / "binder-site-review.tsv").open(encoding="utf-8", newline="") as file:
        all_rows = list(csv.DictReader(file, delimiter="\t"))
    selected = [r for r in all_rows if r["role"] == "VFUNC_BINDING"
                and r["legacy_site"].rsplit(":", 1)[0] in LAYERS]
    assert len(selected) == 34
    cache = {}

    def source(revision, path):
        if (revision, path) not in cache:
            cache[revision, path] = (subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True,
                encoding="utf-8") if revision == "old" else
                Path(path).read_text(encoding="utf-8"))
        return cache[revision, path]

    rows = []
    for item in selected:
        location = item["legacy_site"]
        layer = LAYERS[location.rsplit(":", 1)[0]]
        slot = item["slot"]
        path = header(slot)
        if path is None:
            legacy = target = "GObjectClass.constructed (external GLib C ABI)"
            old_ref = target_ref = "glib-object.h"
            status = "EXTERNAL_GOBJECT"
        else:
            legacy, old_line = signature(source("old", path), slot)
            target, new_line = signature(source("new", path), slot)
            assert legacy != "-", (location, path)
            old_ref = path + ":" + old_line
            target_ref = path + (":" + new_line if new_line != "-" else ":NO_SLOT")
            status = ("REMOVED_SLOT" if target == "-" else
                      "SAME_SIGNATURE" if legacy == target else "CHANGED_SIGNATURE")
        task = followup(layer, slot)
        assert f"| {task} |" in Path("tasks.md").read_text(encoding="utf-8"), task
        rows.append((location, layer, slot, old_ref, legacy, target_ref, target,
                     status, task, "DONE"))

    count = Counter(row[7] for row in rows)
    assert sum(count.values()) == 34 and count["REMOVED_SLOT"] == 3
    assert count["EXTERNAL_GOBJECT"] == 2
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_binding", "layer", "slot", "legacy_header",
                         "legacy_signature", "gimp3_header", "gimp3_signature",
                         "classification", "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} layer binder slots compared: {dict(count)}")


if __name__ == "__main__":
    main()
