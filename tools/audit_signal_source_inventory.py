#!/usr/bin/env python3
"""Merge legacy C++ signal registrations and deferred sources for WBS 01.008."""

import csv
import re
from pathlib import Path

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "signal-source-inventory.tsv"


def read(name):
    with (ROOT / name).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def direct(row, source):
    site, statement = row["legacy_site"], source["boundary_statement"]
    match = re.search(r"g_signal_connect(?:_delegator(?:_noret)?)?\s*\(\s*(?:G_OBJECT\(([^)]+)\)|([^,]+))\s*,", statement)
    assert match, site
    receiver = (match[1] or match[2]).strip()
    signal = re.search(r',\s*"([^"]+)"\s*,', statement)
    signal_expr = signal[1] if signal else "dynamic signal expression"
    if "delegator(this" in statement or "delegator (this" in statement:
        user_data = "C++ delegator captures this"
        stored = "=" in statement.split("g_signal_connect", 1)[0]
        release = "Connection member disconnects" if stored else "Connection return discarded; no explicit disconnect"
        followup = "07.008" if stored else "07.008/untracked-signals"
    else:
        user_data = "editor" if statement.rstrip().endswith("editor);") else "NULL"
        release = "emitter destruction; no explicit handler id"
        followup = "07.008/untracked-signals" if user_data != "NULL" else "08.006"
    return (site, "SIGNAL", receiver, signal_expr, row["callback_target_if_visible"], user_data,
            release, "synchronous signal emission", "callback may run during emit; teardown order requires verification", followup, "TRACED_STATIC")


def wrapper(row):
    stored = row["role"] == "STORED_CONNECTION"
    release = "stored Connection disconnects" if stored else "connect_noret: emitter/closure teardown, no handle"
    return (row["legacy_site"], "SIGNAL", row["receiver"], row["signal"], row["target"],
            "delegator captures this", release, "synchronous signal emission",
            "lambda/member may reenter owner; confirm emitter and owner destruction order",
            "07.008" if stored else "07.008/untracked-signals", "TRACED_STATIC")


SOURCES = (
    ("app/core/gimpfilterlayer.cpp:614", "TIMEOUT", "FilterLayer", "notify_filter_end_callback", "raw this",
     "source ID ignored; no cancellation", "default priority, 300ms", "callback may run after FilterLayer destruction", "17.026/filter-end-timeout-cancel"),
    ("app/widgets/gimplayertileview.cpp:1539", "IDLE", "LayerPreview", "EventSource::callback", "raw this and viewable",
     "update_idle CXXPointer removes source on replacement/destruction", "G_PRIORITY_DEFAULT", "preview/viewable may be destroyed before callback", "29.020/preview-idle-teardown"),
    ("app/widgets/gimplayertileview.cpp:790", "TIMEOUT", "LayerTileView", "EventSource::callback", "raw this",
     "scroll_timeout_handler removes source on drag end/destruction", "G_PRIORITY_DEFAULT", "callback uses scrolled_window; replace and teardown may reenter", "29.002/add-timeout-owner"),
    ("app/widgets/gimplayertileview.cpp:929", "TIMEOUT", "LayerTileView", "EventSource::callback", "raw this",
     "add_timeout_handler clears itself in callback or on button release", "G_PRIORITY_DEFAULT, 500ms", "callback clears owner of executing EventSource", "29.002/add-timeout-owner"),
)


def main():
    candidates = {r["source"] + ":" + r["line"]: r for r in read("cpp-call-boundary-candidates.tsv")}
    callback = [r for r in read("callback-registration-review.tsv") if r["role"] == "SIGNAL_CONNECTION"]
    wrappers = read("signal-wrapper-review.tsv")
    assert len(callback) == 60 and len(wrappers) == 47
    rows = [direct(r, candidates[r["legacy_site"]]) for r in callback]
    rows.extend(wrapper(r) for r in wrappers)
    rows.extend((site, kind, owner, "-", target, data, release, priority, reentry, followup, "TRACED_STATIC")
                for site, kind, owner, target, data, release, priority, reentry, followup in SOURCES)
    assert len(rows) == len({r[0] for r in rows}) == 111
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "kind", "receiver_or_owner", "signal", "callback", "user_data",
                         "release_path", "priority", "reentrancy", "followup", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C++ signal/source registrations inventoried for owner and reentrancy review")


if __name__ == "__main__":
    main()
