#!/usr/bin/env python3
"""Cross-check each explicit legacy C++ GObject ref/unref with its owner."""

import csv
from pathlib import Path

SOURCE = Path("migration/inventory/cpp-ref-hold-review.tsv")
OUTPUT = Path("migration/inventory/cpp-explicit-ref-contracts.tsv")

# Site key is the line in the immutable legacy tree afa43fae.
CONTRACTS = {
    "app/base/glib-cxx-utils.hpp:860": ("Object<T> GObject pointer", "copy/additional owner", "Object<T>::decref and ScopedPointer destructor", "06.020", "TRACED"),
    "app/base/glib-cxx-utils.hpp:861": ("Object<T> GObject pointer", "Object<T> owned reference", "g_object_unref on release; no later dereference", "06.020", "TRACED"),
    "app/base/json-cxx-utils.hpp:276": ("IBuilder constructed from Builder", "new reference for borrowed builder pointer", "Builder ScopedPointer destructor unrefs", "06.020", "TRACED"),
    "app/base/json-cxx-utils.hpp:277": ("IBuilder copied from IBuilder", "new reference for copied builder pointer", "Builder ScopedPointer destructor unrefs", "06.020", "TRACED"),
    "app/display/gimpdisplayshell-overlays.cpp:162": ("overlay widget", "temporary ref across gtk_container_remove", "line 170 after add_child", "29.001", "TRACED"),
    "app/display/gimpdisplayshell-overlays.cpp:170": ("overlay widget", "line 162 temporary ref", "remove/add handoff ends temporary ref", "29.001", "TRACED"),
    "app/httpd/navigation-guide.cpp:190": ("SoupLogger from soup_logger_new", "session feature gets its own reference on add_feature", "local ref released after add_feature", "31.008", "TRACED"),
    "app/httpd/navigation-guide.cpp:201": ("SoupMessage from soup_message_new", "local owner of request", "local ref released after synchronous send", "31.008", "TRACED"),
    "app/paint/gimpmypaintcore-surface.cpp:415": ("Surface.brushmark", "set_brushmark line 443 acquired reference and begin_use", "destructor unrefs but omits end_use", "21.002/resource-owner", "BUG"),
    "app/paint/gimpmypaintcore-surface.cpp:418": ("Surface.texture", "set_texture line 460 acquired reference", "destructor unrefs", "21.002/resource-owner", "TRACED"),
    "app/paint/gimpmypaintcore-surface.cpp:436": ("Surface.brushmark", "previous setter reference", "end_use and unref before assigning new pointer; self assignment can destroy source", "21.002/resource-owner", "BUG"),
    "app/paint/gimpmypaintcore-surface.cpp:443": ("Surface.brushmark", "borrowed input becomes retained Surface owner", "line 436 on replacement or 415 on destruction", "21.002/resource-owner", "TRACED"),
    "app/paint/gimpmypaintcore-surface.cpp:454": ("Surface.texture", "previous setter reference", "unref before assigning new pointer; self assignment can destroy source", "21.002/resource-owner", "BUG"),
    "app/paint/gimpmypaintcore-surface.cpp:460": ("Surface.texture", "borrowed input becomes retained Surface owner", "line 454 on replacement or 418 on destruction", "21.002/resource-owner", "TRACED"),
    "app/paint/gimpmypaintoptions-history.cpp:93": ("history.brushes list element", "dirty options brush transferred by push_brush line 396", "history destructor free_brush unrefs; singleton teardown absent", "08.008", "TRACED"),
    "app/paint/gimpmypaintoptions.cpp:224": ("options.brush", "owned duplicate stored by brush changed callback", "options finalize unrefs retained brush", "08.006", "TRACED"),
    "app/paint/gimpmypaintoptions.cpp:398": ("options.brush", "clean duplicate from prior callback", "brush changed drops clean brush; dirty brush transfers to history", "08.006", "TRACED"),
    "app/tools/gimpmypaintbrushoptions-gui.cpp:208": ("popup.container", "borrowed factory container retained at line 244", "destroy unrefs; ensure create/destroy each called once", "08.008", "TRACED"),
    "app/tools/gimpmypaintbrushoptions-gui.cpp:244": ("popup.container", "factory borrowed pointer", "line 208 destroy unrefs", "08.008", "TRACED"),
    "app/tools/gimpmypaintoptions-gui.cpp:226": ("detail popup.container", "borrowed factory container retained at line 266", "destroy unrefs; ensure create/destroy each called once", "08.008", "TRACED"),
    "app/tools/gimpmypaintoptions-gui.cpp:266": ("detail popup.container", "factory borrowed pointer", "line 226 destroy unrefs", "08.008", "TRACED"),
    "app/widgets/gimpcellrendererpopup.cpp:92": ("CellRendererPopup.pixbuf", "gtk_widget_render_icon result", "destructor unrefs; constructor omits NULL initialization", "08.016/pixbuf-owner", "BUG"),
    "app/widgets/gimpcellrendererpopup.cpp:217": ("CellRendererPopup.pixbuf", "previous render_icon result", "create_pixbuf unrefs before replacement; constructor omits NULL initialization", "08.016/pixbuf-owner", "BUG"),
    "app/widgets/gimplayertileview.cpp:150": ("layer tile local pixbuf", "render_icon or scale_simple new pixbuf", "scale branch releases old before replacing", "29.002", "TRACED"),
    "app/widgets/gimplayertileview.cpp:155": ("layer tile local pixbuf", "render_icon may return NULL", "unconditional unref after build_cairo_surface(NULL)", "29.008/icon-null", "BUG"),
    "app/widgets/gimptooltileview.cpp:134": ("tool tile local pixbuf", "render_icon or scale_simple new pixbuf", "scale branch releases old before replacing", "29.008", "TRACED"),
    "app/widgets/gimptooltileview.cpp:139": ("tool tile local pixbuf", "render_icon may return NULL", "unconditional unref after build_cairo_surface(NULL)", "29.008/icon-null", "BUG"),
    "app/widgets/popupper.cpp:468": ("model renderer local pointer", "gtk_tree_model_get object column adds reference", "unref after extracting borrowed renderer->viewable pointer", "29.006", "TRACED"),
}


def main():
    with SOURCE.open(encoding="utf-8", newline="") as stream:
        rows = [row for row in csv.DictReader(stream, delimiter="\t") if row["kind"] == "REF_UNREF"]
    sites = [row["legacy_site"] for row in rows]
    assert len(sites) == len(set(sites)) == 28, len(sites)
    assert set(sites) == set(CONTRACTS), f"missing={set(sites)-set(CONTRACTS)} extra={set(CONTRACTS)-set(sites)}"
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "syntax_role", "owner", "acquisition", "release_or_risk", "followup", "finding"))
        writer.writerows((row["legacy_site"], row["syntax_role"], *CONTRACTS[row["legacy_site"]]) for row in rows)
    print(f"{len(rows)} explicit GObject references traced; {sum(v[-1] == 'BUG' for v in CONTRACTS.values())} risky sites")


if __name__ == "__main__":
    main()
