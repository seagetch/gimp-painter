#!/usr/bin/env python3
"""Attach owner and release contracts to all added C reference operations."""

import csv
from collections import Counter
from pathlib import Path

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-reference-owner-review.tsv"
CONTRACTS = {
    "app/core/gimp.c": ("Gimp.tool_item_list", "release at Gimp shutdown", "tool item list lifetime"),
    "app/core/gimpcontext.c": ("GimpContext brush/template fields and GValue", "field replacement/finalize unrefs; GValue owns its object ref", "signal disconnect and null on removal"),
    "app/core/gimpdynamics.c": ("GimpDynamicsPrivate.blending_output and GValue", "private finalize releases field; GValue owns its assigned ref", "output replacement and serialization"),
    "app/core/gimpimage-perspective-guide.c": ("GimpImage private perspective guide container", "release previous pointer on replacement; incoming pointer assigned without new ref", "define transfer contract for guide setter and image teardown"),
    "app/core/gimpimage-undo.c": ("local undo object", "free and unref newly allocated undo on uneditable drawable rejection", "reject path before undo stack registration"),
    "app/core/gimpimage.c": ("GimpImage private perspective guide container", "container release in image teardown", "guide signals and teardown"),
    "app/core/gimptoolgroup.c": ("tool group children and GValue", "property GValue receives an object ref", "children model property lifetime"),
    "app/display/gimpimagewindow.c": ("right docks and reparented widgets", "temporary refs surround removal and reparent; window owns dock", "branch specific dock moves and final unref"),
    "app/gui/gimpuiconfigurer.c": ("display shell during UI reparent", "image shell temporary ref released after reparent; empty display branch lacks matching unref", "release temporary ref in empty display branch"),
    "app/paint/gimp-paint.c": ("local paint info", "release local reference after registration", "registration ownership"),
    "app/paint/gimpbrushcore.c": ("GimpBrushCore.texture", "release old field before retaining replacement; finalize releases current", "texture reset and GEGL paint lifecycle"),
    "app/pdb/mypaint-brush-select-cmds.c": ("local GimpProcedure", "release after registration", "procedure registry ownership"),
    "app/plug-in/gimpplugin-message.c": ("local procedure object", "release newly allocated procedure on invalid argument or return-name error", "plugin install early return paths"),
    "app/tools/gimp-tools.c": ("tool options toolbar data and local registration objects", "data-full releases sunk toolbar; local containers/tool infos released", "toolbar exit and repeated hide"),
    "app/tools/gimpbrushoptions-gui.c": ("BrushDialogPrivate.container/context", "ref in setter; destroy/reset unref", "reset signal disconnect and alias safety"),
    "app/tools/gimpdynamicsoptions-gui.c": ("working dynamics", "ref retained for editor and unref at teardown", "editor destruction before dynamics"),
    "app/tools/gimptooloptions-gui.c": ("popup callback state.config", "ref retained in closure state and unref in destroy callback", "closure release on all popup exits"),
    "app/widgets/gimpcontainertreeview-dnd.c": ("drag source viewable", "temporary ref around drag transfer", "drop failure and cancellation"),
    "app/widgets/gimptooleditor.c": ("group/tool item/action/renderer locals", "temporary refs and locals released after operation", "all drag/selection branches"),
    "app/widgets/gimptooloptionstoolbar.c": ("toolbar child widgets", "move ref/unref pair; hide takes extra ref without local unref", "repeat hide/show and finalization"),
    "app/xcf/xcf-load.c": ("replaced layer and filter argument GValues", "sink/unref old layer; value array clear should unset values", "floating layer state and temporary GValue/string cleanup"),
}


def main():
    with (ROOT / "c-reference-added-line-review.tsv").open(encoding="utf-8", newline="") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    paths = {row["legacy_site"].rsplit(":", 1)[0] for row in candidates}
    assert paths == set(CONTRACTS), (paths ^ set(CONTRACTS))
    rows = []
    for row in candidates:
        assert row["line_origin"] == "ADDED"
        path = row["legacy_site"].rsplit(":", 1)[0]
        owner, release, followup = CONTRACTS[path]
        rows.append((row["legacy_site"], row["operation"], owner, release,
                     followup, row["changed_hunk"], "DONE"))
    assert len(rows) == 74
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "owner_group", "release_contract",
                         "migration_followup", "changed_hunk", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} added C references mapped to {len(Counter(row[2] for row in rows))} owner groups")


if __name__ == "__main__":
    main()
