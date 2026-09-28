#!/usr/bin/env python3
"""Reconcile every GTK/GLib and old custom type binder with pinned headers."""

import csv
import subprocess
from collections import Counter
from pathlib import Path

from audit_gimp3_layer_vfuncs import signature
from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "gimp3-external-binder-review.tsv"
GTK2 = "https://raw.githubusercontent.com/GNOME/gtk/2.24.33/gtk/"
GTK3 = "https://raw.githubusercontent.com/GNOME/gtk/3.24.43/gtk/"
GLIB2 = "https://raw.githubusercontent.com/GNOME/glib/2.32.4/gobject/gobject.h"
GLIB3 = "https://raw.githubusercontent.com/GNOME/glib/2.80.0/gobject/gobject.h"

# Pinned GTK and GLib public C headers inspected at the referenced versions.
# The signatures are canonical C function-pointer types, without parameter names.
SLOTS = {
    "constructed": (GLIB2 + "#L344", "void(GObject*)",
                    GLIB3 + "#L354", "void(GObject*)"),
    "activate": (GTK2 + "gtkcellrenderer.h#L99",
                 "gboolean(GtkCellRenderer*,GdkEvent*,GtkWidget*,const gchar*,GdkRectangle*,GdkRectangle*,GtkCellRendererState)",
                 GTK3 + "gtkcellrenderer.h#L154",
                 "gboolean(GtkCellRenderer*,GdkEvent*,GtkWidget*,const gchar*,const GdkRectangle*,const GdkRectangle*,GtkCellRendererState)"),
    "get_size": (GTK2 + "gtkcellrenderer.h#L85",
                 "void(GtkCellRenderer*,GtkWidget*,GdkRectangle*,gint*,gint*,gint*,gint*)",
                 GTK3 + "gtkcellrenderer.h#L141",
                 "void(GtkCellRenderer*,GtkWidget*,const GdkRectangle*,gint*,gint*,gint*,gint*)"),
    "render": (GTK2 + "gtkcellrenderer.h#L92",
               "void(GtkCellRenderer*,GdkDrawable*,GtkWidget*,GdkRectangle*,GdkRectangle*,GdkRectangle*,GtkCellRendererState)",
               GTK3 + "gtkcellrenderer.h#L148",
               "void(GtkCellRenderer*,cairo_t*,GtkWidget*,const GdkRectangle*,const GdkRectangle*,GtkCellRendererState)"),
    "map": (GTK2 + "gtkwidget.h#L642", "void(GtkWidget*)",
            GTK3 + "gtkwidget.h#L380", "void(GtkWidget*)"),
    "button_press_event": (GTK2 + "gtkwidget.h#L677",
                           "gboolean(GtkWidget*,GdkEventButton*)",
                           GTK3 + "gtkwidget.h#L441",
                           "gboolean(GtkWidget*,GdkEventButton*)"),
    "key_press_event": (GTK2 + "gtkwidget.h#L691",
                        "gboolean(GtkWidget*,GdkEventKey*)",
                        GTK3 + "gtkwidget.h#L453",
                        "gboolean(GtkWidget*,GdkEventKey*)"),
}

# A source-defined class has no new header until its GIMP 3 port creates it.
CUSTOM = {
    ("app/core/gimpperspectiveguide.cpp", "removed"):
        ("app/core/gimpperspectiveguide.h", "08.009/removed-signal"),
    ("app/widgets/popupper.cpp", "cancel"):
        ("app/widgets/popupper.h", "29.006/popover-class-signals"),
    ("app/widgets/popupper.cpp", "confirm"):
        ("app/widgets/popupper.h", "29.006/popover-class-signals"),
}


def followup(path):
    if path == "app/core/gimpclonelayerundo.cpp":
        return "08.004"
    if path == "app/widgets/gimplayertileview.cpp":
        return "29.002"
    if path == "app/widgets/gimptooltileview.cpp":
        return "29.008"
    if path == "app/widgets/gimpcellrendererpopup.cpp":
        return "29.006/cellrenderer-gtk3"
    return "29.006"


def main():
    with (ROOT / "binder-site-review.tsv").open(encoding="utf-8", newline="") as file:
        inventory = list(csv.DictReader(file, delimiter="\t"))
    accounted = set()
    for name in ("gimp3-layer-vfunc-review.tsv", "gimp3-tool-vfunc-review.tsv",
                 "gimp3-core-binder-review.tsv"):
        with (ROOT / name).open(encoding="utf-8", newline="") as file:
            accounted.update(row["legacy_binding"] for row in csv.DictReader(file, delimiter="\t"))
    remaining = [row for row in inventory if row["role"] == "VFUNC_BINDING"
                 and row["legacy_site"] not in accounted]
    assert len(remaining) == 13 and len(accounted) == 58
    sources = {}
    tasks = Path("tasks.md").read_text(encoding="utf-8")
    rows = []
    for item in remaining:
        site, slot = item["legacy_site"], item["slot"]
        path = site.rsplit(":", 1)[0]
        if (path, slot) in CUSTOM:
            old_header, task = CUSTOM[path, slot]
            if old_header not in sources:
                sources[old_header] = subprocess.check_output(
                    ["git", "show", f"{REVISION}:{old_header}"], text=True,
                    encoding="utf-8")
            old, old_line = signature(sources[old_header], slot)
            assert old != "-", site
            old_ref, new_ref, new = f"{old_header}:{old_line}", "TYPE_NOT_PORTED", "-"
            classification = "CUSTOM_TYPE_PENDING"
        else:
            assert slot in SLOTS, (site, slot)
            old_ref, old, new_ref, new = SLOTS[slot]
            task = followup(path)
            classification = "SAME_SIGNATURE" if old == new else "CHANGED_SIGNATURE"
        assert f"| {task} |" in tasks, task
        rows.append((site, slot, old_ref, old, new_ref, new, classification,
                     task, "DONE"))

    count = Counter(row[6] for row in rows)
    assert count == {"SAME_SIGNATURE": 7, "CHANGED_SIGNATURE": 3,
                     "CUSTOM_TYPE_PENDING": 3}, count
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_binding", "slot", "legacy_header", "legacy_signature",
                         "gimp3_header", "gimp3_signature", "classification",
                         "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} remaining binder sites reconciled: {dict(count)}")


if __name__ == "__main__":
    main()
