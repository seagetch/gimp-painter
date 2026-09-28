#!/usr/bin/env python3
"""Trace active non-XCF C call sites into legacy C++ entry contracts."""

import csv
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "non-xcf-c-call-review.tsv"

# Classification describes the visible caller/callee relationship. The port
# must still validate target GIMP 3 APIs and ownership with runtime tests.
CONTRACTS = {
    "gimp_clone_layer_new": (
        "CREATE_LAYER", "Returns new layer for insertion above source; caller lacks null guard"),
    "gimp_filter_layer_new": (
        "CREATE_LAYER", "Returns new layer for insertion; caller checks null"),
    "gimp_mypaint_brush_get_standard": (
        "BORROW_STANDARD_RESOURCE", "Returns weak-pointer-backed standard brush; context references selected brush"),
    "gimp_clone_layer_get_source": (
        "LOOKUP_SOURCE", "Borrowed source; getter may resolve saved source name as a side effect"),
    "gimp_clone_layer_set_source": (
        "REMAP_GROUP_CLONE", "Rebinds copied clone to copied source and connects update signals"),
    "gimp_mypaint_brush_editor_new": (
        "CONSTRUCT_EDITOR_WITH_LEGACY_GUARD_DEFECT", "GimpContext parameter is also checked as GimpMypaintOptions; local options remains null"),
    "gimp_display_shell_update_on_canvas_views": (
        "REFRESH_OVERLAYS", "Rebuilds or disposes canvas overlay widgets as image/display changes"),
    "gimp_perspective_guide_get_vanish_point_length": (
        "READ_GUIDE", "Returns point count to the snapping calculation"),
    "gimp_perspective_guide_get_vanish_points": (
        "READ_GUIDE", "Boolean success result ignored while using output coordinates"),
    "gimp_display_shell_update_on_canvas_opacity": (
        "UPDATE_OVERLAY_OPACITY", "Uses borrowed shell widgets and press/release flag"),
    "gimp_display_shell_detach_on_canvas_view": (
        "DETACH_OVERLAY", "Undecorates widget before parent change; caller holds dock reference"),
    "gimp_display_shell_attach_on_canvas_view": (
        "ATTACH_OVERLAY", "Installs decorator and stores widget in shell; caller releases temporary reference"),
    "gimp_cell_renderer_popup_clicked": (
        "FORWARD_POPUP_EVENT", "Synchronous call receives path string freed just after return"),
    "gimp_cell_renderer_popup_new": (
        "CONSTRUCT_RENDERER", "New renderer stored in layer view and packed into tree column"),
}


def main():
    with (ROOT / "c-direct-call-activity.tsv").open(newline="", encoding="utf-8") as file:
        sites = [r for r in csv.DictReader(file, delimiter="\t")
                 if r["compile_state"] == "ACTIVE_CPP_ENTRY"
                 and not r["c_site"].startswith("app/xcf/")]
    assert len(sites) == 24
    with (ROOT / "cpp-definition-review.tsv").open(newline="", encoding="utf-8") as file:
        definitions = {r["symbol"]: r for r in csv.DictReader(file, delimiter="\t")
                       if r["classification"] == "C_HEADER_ENTRY"}
    sources = {}

    def anchored(site, token):
        path, line = site.rsplit(":", 1)
        if path not in sources:
            sources[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"],
                text=True, encoding="utf-8").splitlines()
        value = sources[path][int(line) - 1]
        assert token in value, (site, token, value)

    rows = []
    for site in sites:
        name, location = site["symbol"], site["c_site"]
        assert name in CONTRACTS and name in definitions, (location, name)
        anchored(location, name)
        anchored(definitions[name]["cpp_site"], name)
        effect, contract = CONTRACTS[name]
        rows.append((location, name, definitions[name]["cpp_site"], effect,
                     contract, "DONE"))
    assert len({r[1] for r in rows}) == 14
    counts = Counter(r[3] for r in rows)
    assert counts["BORROW_STANDARD_RESOURCE"] == 4
    assert counts["READ_GUIDE"] == 4
    assert counts["UPDATE_OVERLAY_OPACITY"] == 4

    # Recheck the contract claims against caller/callee source anchors.
    anchored("app/actions/layers-commands.c:439", "gimp_image_add_layer")
    anchored("app/core/gimpcontext.c:3504", "g_object_ref (mypaint_brush)")
    anchored("app/core/gimpmypaintbrush.cpp:366", "static GimpData *standard_mypaint_brush")
    anchored("app/core/gimpclonelayer.cpp:277", "CloneLayer::set_source")
    anchored("app/dialogs/dialogs-constructors.c:813", "gimp_mypaint_brush_editor_new")
    anchored("app/widgets/gimpmypaintbrusheditor.cpp:287", "GIMP_IS_CONTEXT")
    anchored("app/widgets/gimpmypaintbrusheditor.cpp:288", "GIMP_IS_MYPAINT_OPTIONS(context)")
    anchored("app/widgets/gimpmypaintbrusheditor.cpp:290", "options = NULL")
    anchored("app/widgets/gimpmypaintbrusheditor.cpp:299", "gimp_mypaint_options_get_current_brush (options)")
    anchored("app/widgets/gimpcontainertreeview.c:1190", "g_free (path_str)")
    anchored("app/widgets/gimplayertreeview.c:359", "gtk_tree_view_column_pack_start")
    anchored("app/display/gimpdisplayshell-overlays.cpp:674", "shell->docks = widget")
    anchored("app/display/gimpdisplayshell-overlays.cpp:717", "undecorate")

    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("c_site", "symbol", "cpp_entry", "effect",
                         "result_and_owner_contract", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} non-XCF active C-to-C++ calls traced across "
          f"{len(CONTRACTS)} symbols")


if __name__ == "__main__":
    main()
