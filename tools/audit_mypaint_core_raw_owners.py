#!/usr/bin/env python3
"""Attach ownership and activity to every legacy MyPaint paint core raw site."""

import csv
from pathlib import Path

ROOT = Path("migration/inventory")
SOURCE = "app/paint/gimpmypaintcore.cpp"
CONTRACTS = {
    86: ("option_changed_handler", "cleanup deletes Connection and disconnects signal"),
    93: ("stroke", "cleanup deletes remaining stroke after split"),
    97: ("brush", "cleanup deletes Brush"),
    101: ("surface", "cleanup deletes Surface"),
    116: ("option_changed_handler", "options change deletes previous Connection"),
    129: ("none", "log string mentioning new; no allocation"),
    133: ("none", "log string mentioning delete; no release"),
    134: ("surface", "replace drawable surface then assign new factory result"),
    135: ("none", "log string mentioning new; no allocation"),
    141: ("stroke", "new Stroke deleted in split_stroke or cleanup"),
    219: ("stroke", "split_stroke deletes and nulls stroke"),
    223: ("none", "commented out delete; no release"),
    239: ("brush", "disabled code branch; no active release"),
    245: ("brush", "new Brush deleted in cleanup"),
}


def main():
    with (ROOT / "cpp-raw-lifetime-review.tsv").open(newline="", encoding="utf-8") as file:
        rows = [row for row in csv.DictReader(file, delimiter="\t")
                if row["legacy_site"].startswith(SOURCE + ":")]
    assert len(rows) == len(CONTRACTS) == 14
    assert {int(row["legacy_site"].rsplit(":", 1)[1]) for row in rows} == set(CONTRACTS)
    with (ROOT / "mypaint-core-raw-owners.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "syntax_role", "owner", "release_contract", "status"))
        for row in rows:
            line = int(row["legacy_site"].rsplit(":", 1)[1])
            writer.writerow((row["legacy_site"], row["kind"], row["syntax_role"], *CONTRACTS[line], "DONE"))
    print("14 legacy MyPaint core raw sites mapped")


if __name__ == "__main__":
    main()
