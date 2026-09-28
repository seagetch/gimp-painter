#!/usr/bin/env python3
"""Trace deferred C callbacks and the observable cancellation paths."""

import csv
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "deferred-source-review.tsv"


def main():
    cache = {}

    def line(site, expected):
        path, n = site.rsplit(":", 1)
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True,
                encoding="utf-8").splitlines()
        value = cache[path][int(n) - 1]
        assert expected in value, (site, value)

    line("app/core/gimpfilterlayer.cpp:614", "g_timeout_add(300")
    line("app/core/gimpfilterlayer.cpp:105", "notify_filter_end_callback(FilterLayer* filter)")
    line("app/widgets/gimplayertileview.cpp:1539", "update_idle = new Idle(")
    line("app/widgets/gimplayertileview.cpp:1540", "[this, viewable]")
    line("app/widgets/gimplayertileview.cpp:198", "CXXPointer<Idle>")
    line("app/base/glib-cxx-utils.hpp:1186", "guint id;")
    line("app/base/glib-cxx-utils.hpp:1209", "remove_func")
    line("app/base/glib-cxx-utils.hpp:1218", "~EventSource()")
    line("app/base/glib-cxx-utils.hpp:1225", "g_idle_add_full")

    # Verify the FilterLayer direct registration does not save a source ID
    # on the same line or in a member, whereas the preview's RAII wrapper does.
    layer = "\n".join(cache["app/core/gimpfilterlayer.cpp"])
    assert "g_timeout_add(300, (GSourceFunc)notify_filter_end_callback, this);" in layer
    assert "g_source_remove" not in layer
    rows = [
        ("app/core/gimpfilterlayer.cpp:614", "300ms timeout",
         "notify_filter_end_callback", "raw FilterLayer this", "ID ignored at call",
         "No g_source_remove in FilterLayer source; owner may be destroyed before callback",
         "Cancel on teardown or retain a safe owner until one-shot callback completes", "DONE"),
        ("app/widgets/gimplayertileview.cpp:1539", "default-priority idle",
         "EventSource::callback", "lambda captures this and viewable",
         "Idle ID held in LayerPreview::update_idle; destructor calls g_source_remove",
         "Owner teardown cancels registered source, but captured viewable lifetime is unproven",
         "Tie viewable lifetime to idle source or guard invalidation before dereference", "DONE"),
    ]
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("registration", "source_kind", "callback", "captured_owner",
                         "cancellation", "observed_risk", "port_action", "status"))
        writer.writerows(rows)
    print("2 deferred callback sites traced: direct timeout and RAII idle")


if __name__ == "__main__":
    main()
