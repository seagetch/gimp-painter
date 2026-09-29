#!/usr/bin/env python3
"""Filter legacy C data calls to modified hunks and annotate state contracts."""

import csv
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-object-data-delta-review.tsv"
CONTRACT = {
    "app/tools/gimp-tools.c": ("tool options or tool info", "toolbar widget ref is sunk and released by g_object_unref; metadata is borrowed", "toolbar teardown and function-pointer API"),
    "app/tools/gimpbrushoptions-gui.c": ("brush options widgets", "literal property names are borrowed; gimp-item-data is borrowed", "widget and brush option callback lifetime"),
    "app/tools/gimptooloptions-gui.c": ("popup button label", "GINT_TO_POINTER flags are borrowed scalar values", "config notify disconnection and label lifetime"),
    "app/widgets/gimptooloptionstoolbar.c": ("tool options toolbar and tool options", "toolbar widget is borrowed from tool options data", "tool change and toolbar widget destruction order"),
}


def main():
    hunks = defaultdict(list)
    with (ROOT / "changed-hunks.tsv").open(encoding="utf-8", newline="") as file:
        for row in csv.DictReader(file, delimiter="\t"):
            if row["path"] not in CONTRACT:
                continue
            match = re.search(r"-> (\d+)(?:,(\d+))?", row["old_new_lines"])
            if match:
                hunks[row["path"]].append((int(match[1]), int(match[2] or 1), row["child_id"]))
    rows = []
    with (ROOT / "c-object-data-candidates.tsv").open(encoding="utf-8", newline="") as file:
        for row in csv.DictReader(file, delimiter="\t"):
            path, number = row["legacy_site"].rsplit(":", 1)
            for start, count, child_id in hunks[path]:
                if start <= int(number) < start + count:
                    owner, destruction, risk = CONTRACT[path]
                    rows.append((row["legacy_site"], child_id, row["operation"],
                                 row["owner_expr"], row["key_expr"], row["value_expr"],
                                 row["destroy_expr"], owner, destruction, risk, "DONE"))
                    break
    assert len(rows) == 19, len(rows)
    assert len({row[0] for row in rows}) == 19
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "changed_hunk", "operation", "owner_expr",
                         "key_expr", "value_expr", "destroy_expr", "owner_type",
                         "destruction_contract", "followup_risk", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C state calls within changed hunks")


if __name__ == "__main__":
    main()
