#!/usr/bin/env python3
"""Classify GValue reference and ownership syntax in the legacy C++ tree."""

import csv
from collections import Counter
from pathlib import Path

INPUT = Path("migration/inventory/cpp-reference-candidates.tsv")
OUTPUT = Path("migration/inventory/cpp-gvalue-review.tsv")


def classify(site, line):
    if site.startswith("app/base/glib-cxx-utils.hpp:"):
        return "VALUE_WRAPPER_OR_TRAIT", "Value wrapper ownership depends on IsOwner and IsManager"
    if "g_value_init" in line or "G_VALUE_INIT" in line:
        return "VALUE_INITIALIZATION", "Unset initialized owned contents exactly once"
    if "ref<GValue>" in line:
        return "ARRAY_BORROW", "Check backing array and element clear function"
    if "g_value_unset" in line:
        return "VALUE_RELEASE", "Check corresponding initialized value"
    if "const GValue" in line or "GValue*" in line or "GValue *" in line:
        return "BORROWED_VALUE_POINTER", "Callback or accessor pointer has external lifetime"
    return "VALUE_COPY_OR_SIGNATURE", "Check by-value shallow copy and ownership transfer"


def main():
    with INPUT.open(encoding="utf-8", newline="") as file:
        candidates = [row for row in csv.DictReader(file, delimiter="\t")
                      if row["kind"] == "G_VALUE"]
    rows = []
    for candidate in candidates:
        role, followup = classify(candidate["legacy_site"], candidate["source_line"])
        rows.append((candidate["legacy_site"], role, candidate["source_line"], followup, "DONE"))
    counts = Counter(row[1] for row in rows)
    assert len(rows) == 136
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "syntax_role", "source_line", "ownership_followup", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} GValue candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
