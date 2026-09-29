#!/usr/bin/env python3
"""Finish the legacy hold helper and new allocation transfer census."""

import csv
from pathlib import Path

SOURCE = Path("migration/inventory/cpp-ref-hold-review.tsv")
TRANSFERS = Path("migration/inventory/cpp-hold-transfer-contracts.tsv")
OUTPUT = Path("migration/inventory/cpp-hold-owner-contracts.tsv")

HELPERS = {
    "app/base/glib-cxx-utils.hpp:163": ("GList nodes", "List destructor g_list_free; elements remain borrowed", "06.021"),
    "app/base/glib-cxx-utils.hpp:332": ("GArray reference", "Array destructor g_array_unref; element clear function belongs to creator", "06.021/array-reassignment"),
    "app/base/glib-cxx-utils.hpp:416": ("GHashTable reference", "HashTable destructor g_hash_table_unref", "08.007/dict-owner"),
    "app/base/glib-cxx-utils.hpp:837": ("heap GValue pointer", "HoldValue destructor g_value_unset then g_free; caller must pass heap allocation", "06.020"),
    "app/base/glib-cxx-utils.hpp:838": ("GValue reference", "HoldValue also g_free on destruction; only safe if reference aliases owned heap allocation", "06.020"),
    "app/base/glib-cxx-utils.hpp:875": ("owned GObject T*", "Object<T> destructor g_object_unref; non-GObject T is invalid", "28.014/applier-owner"),
    "app/base/json-cxx-utils.hpp:41": ("owned JsonNode reference", "Node destructor json_node_unref", "06.020"),
}

NEW = {
    "app/base/glib-cxx-impl.hpp:161": ("new GArray signals cache", "static Array adopts initial ref and unrefs on destruction", "05.008"),
    "app/base/glib-cxx-impl.hpp:169": ("new GArray properties cache", "static Array adopts initial ref and unrefs on destruction", "05.008"),
    "app/presets/preset-factory-gui.cpp:129": ("new GArray action entries", "local Array adopts initial ref; IArray view adds/drops temporary ref", "28.014/preset-dialog-actions"),
    "app/tools/gimpbucketfillbrushtool.cpp:410": ("new GimpChannel mask", "local Object<GimpChannel> unrefs initial GObject ref at scope end", "27.004"),
    "app/tools/gimpbucketfillbrushtool.cpp:433": ("new GimpChannel contiguous region", "local Object<GimpChannel> unrefs initial GObject ref at scope end", "27.004"),
}


def main():
    with SOURCE.open(encoding="utf-8", newline="") as stream:
        rows = [r for r in csv.DictReader(stream, delimiter="\t") if r["kind"] == "HOLD"]
    with TRANSFERS.open(encoding="utf-8", newline="") as stream:
        transfers = {r["legacy_site"]: r for r in csv.DictReader(stream, delimiter="\t")}
    assert len(rows) == 24 and len(transfers) == 12
    assert len({r["legacy_site"] for r in rows}) == 24
    output = []
    for row in rows:
        site = row["legacy_site"]
        if site in transfers:
            transfer = transfers[site]
            owner, release, followup = transfer["source_contract"], transfer["release_contract"], transfer["followup"]
            finding = transfer["finding"]
        else:
            source = HELPERS if row["syntax_role"] == "ADOPTING_HELPER" else NEW
            owner, release, followup = source[site]
            finding = "HELPER" if source is HELPERS else "NEW_OBJECT"
        output.append((site, row["syntax_role"], owner, release, followup, finding))
    assert {r[0] for r in output} == set(transfers) | set(HELPERS) | set(NEW)
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "syntax_role", "owner_or_source", "release_contract", "followup", "finding"))
        writer.writerows(output)
    print(f"{len(output)} hold sites accounted for: {len(transfers)} transfers, {len(HELPERS)} helpers, {len(NEW)} new objects")


if __name__ == "__main__":
    main()
