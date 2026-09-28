#!/usr/bin/env python3
"""Check that every old Binder and direct class callback has one contract row."""

import csv
from collections import Counter
from pathlib import Path


ROOT = Path("migration/inventory")
BINDER_REPORTS = (
    "gimp3-layer-vfunc-review.tsv",
    "gimp3-tool-vfunc-review.tsv",
    "gimp3-core-binder-review.tsv",
    "gimp3-external-binder-review.tsv",
)
DIRECT_REPORTS = (
    "gimp3-brush-callback-review.tsv",
    "gimp3-tool-callback-review.tsv",
    "gimp3-remaining-callback-review.tsv",
)


def read(name):
    with (ROOT / name).open(encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def check_reports(reports, key, expected, count):
    rows = [entry for name in reports for entry in read(name)]
    occurrences = Counter(row[key] for row in rows)
    assert len(rows) == count and set(occurrences) == expected, (
        len(rows), len(expected), sorted(expected - occurrences.keys()),
        sorted(occurrences.keys() - expected))
    assert all(value == 1 for value in occurrences.values())
    assert all(row["status"] == "DONE" and row["legacy_signature"]
               and row["followup_task"] for row in rows)
    for row in rows:
        if (row["gimp3_header"] == "TYPE_NOT_PORTED" or
                row["gimp3_header"].endswith(":NO_SLOT")):
            assert row["classification"] in {"CUSTOM_TYPE_PENDING", "REMOVED_SLOT"}
            assert row["gimp3_signature"] == "-"
        else:
            assert row["gimp3_signature"] != "-"
    return rows


def main():
    expected_binders = {row["legacy_site"] for row in read("binder-site-review.tsv")
                        if row["role"] == "VFUNC_BINDING"}
    expected_direct = {row["assignment_site"] for row in read("vfunc-assignment-review.tsv")
                       if row["role"] != "CLASS_METADATA"}
    assert len(expected_binders) == 71 and len(expected_direct) == 40
    binders = check_reports(BINDER_REPORTS, "legacy_binding", expected_binders, 71)
    direct = check_reports(DIRECT_REPORTS, "legacy_assignment", expected_direct, 40)
    assert all(row["definition_signature"] == row["legacy_signature"] for row in direct)
    tasks = Path("tasks.md").read_text(encoding="utf-8")
    assert all(f"| {row['followup_task']} |" in tasks for row in binders + direct)
    print("GIMP 3 vfunc contract coverage: 71 Binder and 40 direct callback rows OK")


if __name__ == "__main__":
    main()
