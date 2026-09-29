#!/usr/bin/env python3
"""Classify explicit hold/ref/unref sites in the legacy C++ inventory."""

import csv
from collections import Counter
from pathlib import Path

INPUT = Path("migration/inventory/cpp-reference-candidates.tsv")
OUTPUT = Path("migration/inventory/cpp-ref-hold-review.tsv")


def classify(row):
    site, kind, line = row["legacy_site"], row["kind"], row["source_line"]
    if kind == "HOLD":
        if "inline auto hold(" in line:
            return "ADOPTING_HELPER", "Review caller transfer before wrapping"
        if site == "app/widgets/gimplayerpopup.cpp:927":
            return "BORROWED_SOURCE_RISK", "gtk_ui_manager_get_widget result is wrapped by adopting hold; verify ownership or take explicit ref"
        if "g_array_new(" in line or "gimp_channel_new_mask" in line or "gimp_image_contiguous_region_by_seed_full" in line:
            return "NEW_OBJECT_ADOPTION", "Verify new allocation and release on exit"
        return "TRANSFER_CONTRACT_REVIEW", "Verify returned ownership and wrapper cleanup"
    if site.startswith("app/base/"):
        return "REFERENCE_HELPER", "Inspect wrapper copy move and unref behavior"
    if "g_object_ref(" in line or "g_object_ref (" in line:
        return "RETAIN", "Find corresponding unref and owner"
    if "g_object_unref(" in line or "g_object_unref (" in line:
        return "RELEASE", "Find matching ownership source and post-release aliases"
    raise AssertionError(row)


def main():
    with INPUT.open(encoding="utf-8", newline="") as file:
        candidates = [row for row in csv.DictReader(file, delimiter="\t")
                      if row["kind"] in ("HOLD", "REF_UNREF")]
    rows = []
    for candidate in candidates:
        category, followup = classify(candidate)
        rows.append((candidate["legacy_site"], candidate["kind"], category,
                     candidate["source_line"], followup, "DONE"))
    counts = Counter(row[2] for row in rows)
    assert len(rows) == 52, len(rows)
    assert sum(1 for row in rows if row[2] == "BORROWED_SOURCE_RISK") == 1
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "kind", "syntax_role", "source_line",
                         "ownership_followup", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} explicit hold/ref candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
