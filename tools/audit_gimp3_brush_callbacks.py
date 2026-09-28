#!/usr/bin/env python3
"""Compare the old MyPaint resource C callbacks with GIMP 3 class slots."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from audit_gimp3_layer_vfuncs import signature
from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-brush-callback-review.tsv"
OLD_GOBJECT = "https://raw.githubusercontent.com/GNOME/glib/2.32.4/gobject/gobject.h"
NEW_GOBJECT = "https://raw.githubusercontent.com/GNOME/glib/2.80.0/gobject/gobject.h"
GOBJECT = {
    "finalize": (334, 344, "void(GObject*)"),
    "set_property": (325, 335, "void(GObject*,guint,const GValue*,GParamSpec*)"),
    "get_property": (329, 339, "void(GObject*,guint,GValue*,GParamSpec*)"),
}
HEADERS = {
    "get_memsize": "app/core/gimpobject.h",
    "get_size": "app/core/gimpviewable.h",
    "get_new_preview": "app/core/gimpviewable.h",
    "get_description": "app/core/gimpviewable.h",
    "dirty": "app/core/gimpdata.h",
    "get_extension": "app/core/gimpdata.h",
    "save": "app/core/gimpdata.h",
    "get_checksum": "app/core/gimptagged.h",
    "begin_use": "app/core/gimpmypaintbrush.h",
    "end_use": "app/core/gimpmypaintbrush.h",
    "select_mypaint_brush": "app/core/gimpmypaintbrush.h",
    "want_null_motion": "app/core/gimpmypaintbrush.h",
}


def normalize_type(type_text):
    return re.sub(r"\s+", " ", re.sub(r"\s*\*\s*", "*", type_text.strip()))


def callback_definition(source, line, symbol):
    lines = source.splitlines()
    assert symbol in lines[line - 1], (line, symbol)
    before = line - 2
    while not lines[before].strip():
        before -= 1
    result = re.sub(r"^static\s+", "", lines[before].strip())
    result = normalize_type(result)
    block = "\n".join(lines[line - 1:line + 18])
    match = re.match(r"\s*" + re.escape(symbol) + r"\s*\((?P<args>.*?)\)\s*\{",
                     block, re.S)
    assert match, (line, symbol)
    args = []
    for arg in match["args"].split(","):
        arg = arg.strip()
        if arg == "void":
            continue
        arg = re.sub(r"\b[A-Za-z_]\w*\s*$", "", arg).strip()
        args.append(normalize_type(arg))
    return result + "(" + ",".join(args) + ")"


def followup(slot):
    if slot in {"get_size", "get_new_preview"}:
        return "19.013/preview-contract"
    if slot == "save":
        return "19.011/output-stream-save"
    if slot in {"begin_use", "end_use", "select_mypaint_brush", "want_null_motion",
                "finalize", "set_property", "get_property"}:
        return "08.005"
    return "19.013"


def main():
    with (ROOT / "vfunc-assignment-review.tsv").open(encoding="utf-8", newline="") as file:
        inventory = list(csv.DictReader(file, delimiter="\t"))
    selected = [row for row in inventory if row["role"] != "CLASS_METADATA"
                and row["assignment_site"].startswith("app/core/gimpmypaintbrush.cpp:")]
    assert len(selected) == 15
    cache = {}

    def legacy(path):
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
        return cache[path]

    tasks = Path("tasks.md").read_text(encoding="utf-8")
    rows = []
    for item in selected:
        slot = item["slot"]
        definition = item["target_definition"]
        source_path, number = definition.rsplit(":", 1)
        actual = callback_definition(legacy(source_path), int(number),
                                     item["assigned_value"])
        if slot in GOBJECT:
            old_line, new_line, old = GOBJECT[slot]
            old_ref, new_ref, new = (f"{OLD_GOBJECT}#L{old_line}",
                                     f"{NEW_GOBJECT}#L{new_line}", old)
        else:
            path = HEADERS[slot]
            old, old_line = signature(legacy(path), slot)
            old_ref = f"{path}:{old_line}"
            if path == "app/core/gimpmypaintbrush.h":
                assert not Path(path).exists(), path
                new_ref, new = "TYPE_NOT_PORTED", "-"
            else:
                new, new_line = signature(Path(path).read_text(encoding="utf-8"), slot)
                new_ref = f"{path}:{new_line}"
        assert old != "-" and actual == old, (definition, actual, old)
        task = followup(slot)
        assert f"| {task} |" in tasks, task
        classification = ("CUSTOM_TYPE_PENDING" if new == "-" else
                          "SAME_SIGNATURE" if new == old else "CHANGED_SIGNATURE")
        rows.append((item["assignment_site"], definition, actual, slot, old_ref,
                     old, new_ref, new, classification, task, "DONE"))

    count = Counter(row[8] for row in rows)
    assert count == {"SAME_SIGNATURE": 9, "CHANGED_SIGNATURE": 2,
                     "CUSTOM_TYPE_PENDING": 4}, count
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_assignment", "legacy_definition", "definition_signature",
                         "slot", "legacy_header", "legacy_signature", "gimp3_header",
                         "gimp3_signature", "classification", "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} MyPaint brush callbacks compared: {dict(count)}")


if __name__ == "__main__":
    main()
