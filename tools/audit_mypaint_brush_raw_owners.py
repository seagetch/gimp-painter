#!/usr/bin/env python3
"""Map the active raw lifetime sites of the legacy MyPaint brush resource."""

import csv
from pathlib import Path

ROOT = Path("migration/inventory")
SOURCE = "app/core/gimpmypaintbrush.cpp"
CONTRACTS = {
    147: ("GimpMypaintBrush.p", "GObject finalize deletes private", "normal construction"),
    157: ("GimpMypaintBrush.p", "private destructor releases strings, mappings and icon", "normal finalize"),
    357: ("duplicate result.p", "replace constructed private with duplicate result", "duplicate must release former private"),
    375: ("static standard_mypaint_brush slot", "weak pointer nulls static slot when GObject finalizes", "slot survives process"),
    457: ("private.parent_brush_name", "private destructor frees duplicate string", "normal finalize"),
    461: ("private.group", "private destructor frees duplicate string", "normal finalize"),
    469: ("private.text[i]", "private destructor frees each duplicate string", "normal finalize"),
    503: ("private.settings[index].mapping", "deallocate_mapping deletes mapping", "19.006/mapping-value-owner"),
    512: ("private.settings[index].mapping", "delete and reset slot to NULL", "19.006/mapping-value-owner"),
    527: ("private.parent_brush_name", "setter frees previous string then duplicates input", "alias input to old string unsafe"),
    541: ("private.group", "setter frees previous string then duplicates input", "alias input to old string unsafe"),
    567: ("private.text[index]", "setter frees previous string then duplicates input", "alias input to old string unsafe"),
    628: ("preview local Brush", "deleted after surface session", "early exit and exception cleanup"),
    733: ("preview local Brush", "delete after surface session", "normal preview exit"),
    734: ("preview local GimpMypaintSurface", "delete after surface session", "normal preview exit"),
    782: ("duplicated private result", "transferred to result.p then GObject finalize", "mapping copy and icon buffer followups"),
}


def main():
    with (ROOT / "cpp-raw-lifetime-review.tsv").open(newline="", encoding="utf-8") as file:
        source_rows = [row for row in csv.DictReader(file, delimiter="\t")
                       if row["legacy_site"].startswith(SOURCE + ":")]
    assert len(source_rows) == len(CONTRACTS) == 16
    assert {int(row["legacy_site"].rsplit(":", 1)[1]) for row in source_rows} == set(CONTRACTS)
    with (ROOT / "mypaint-brush-raw-owners.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "owner", "release_contract", "migration_followup", "status"))
        for row in source_rows:
            line = int(row["legacy_site"].rsplit(":", 1)[1])
            writer.writerow((row["legacy_site"], row["kind"], *CONTRACTS[line], "DONE"))
    print("16 legacy MyPaint brush raw sites mapped")


if __name__ == "__main__":
    main()
