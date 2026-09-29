#!/usr/bin/env python3
"""Classify reference operations near C migration hunks for focused review."""

import csv
from collections import Counter
from pathlib import Path

INPUT = Path("migration/inventory/c-reference-candidates.tsv")
OUTPUT = Path("migration/inventory/c-reference-hunk-review.tsv")
ROLES = {
    "g_object_ref": ("RETAIN", "Pair acquired reference with later unref"),
    "g_object_ref_sink": ("SINK_OR_RETAIN", "Check floating state and later unref or transfer"),
    "g_object_unref": ("RELEASE", "Check owning source and aliases after release"),
    "g_value_init": ("VALUE_INIT", "Pair initialized value with exactly one unset"),
    "g_value_set_object": ("VALUE_OBJECT_REF", "Check GValue unset and object reference transfer"),
}


def main():
    with INPUT.open(encoding="utf-8", newline="") as file:
        candidates = [row for row in csv.DictReader(file, delimiter="\t")
                      if row["changed_hunk"] != "-"]
    rows = []
    for candidate in candidates:
        role, followup = ROLES[candidate["operation"]]
        rows.append((candidate["legacy_site"], candidate["changed_hunk"],
                     candidate["operation"], role, candidate["source_line"], followup,
                     "DONE"))
    counts = Counter(row[3] for row in rows)
    assert len(rows) == 74, len(rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "changed_hunk", "operation", "syntax_role",
                         "source_line", "ownership_followup", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C hunk reference sites classified: {dict(counts)}")


if __name__ == "__main__":
    main()
