#!/usr/bin/env python3
"""Verify every legacy hold call whose return transfer needs source tracing."""

import csv
from pathlib import Path

SOURCE = Path("migration/inventory/cpp-ref-hold-review.tsv")
OUTPUT = Path("migration/inventory/cpp-hold-transfer-contracts.tsv")

DICT_SITES = {277, 279, 345, 367, 551, 553, 555}


def contract(site):
    if site.startswith("app/core/gimpmypaintbrush-load.cpp:"):
        assert int(site.rsplit(":", 1)[1]) in DICT_SITES, site
        return ("getter adds a GHashTable reference for each caller",
                "GLib::HashTable hold adopts that reference and unrefs at scope exit",
                "01.007/dict-transfer,08.007/dict-owner", "TRACED")
    if site == "app/presets/layer-preset.cpp:98":
        return ("JSON::INode::keys returns json_object_get_members allocated GList",
                "GLib::List hold frees list nodes; member strings remain borrowed",
                "28.003", "TRACED")
    if site == "app/presets/layer-preset.cpp:450":
        return ("create_replacement_layer returns NULL; its result is never appended",
                "GLib::List hold sees NULL; created layers are handed to the image",
                "28.005", "TRACED")
    if site == "app/presets/layer-preset.cpp:468":
        return ("get_args allocates GArray with GValue clear function; IArray borrow adds a temporary ref",
                "IArray scope drops its ref; returned ref passes to GLib::Array hold; caller unrefs after set_procedure copies values",
                "28.007,15.001/gvalue-deep-copy", "TRACED")
    if site == "app/widgets/gimplayertileview.cpp:1282":
        return ("ILayerPresetApplier::new_instance returns new LayerPresetApplier C++ instance",
                "generic hold selects GLib::Object and g_object_unref, not C++ delete; use CXXPointer or unique_ptr",
                "28.014/applier-owner", "BUG")
    if site == "app/widgets/gimplayerpopup.cpp:927":
        return ("gtk_ui_manager_get_widget returns borrowed GtkWidget menu",
                "generic hold adopts without g_object_ref and unrefs borrowed widget; use nonowning ref or explicit ref",
                "29.006/menu-borrowed-reference", "BUG")
    raise AssertionError(f"untraced transfer: {site}")


def main():
    with SOURCE.open(encoding="utf-8", newline="") as stream:
        candidates = [row for row in csv.DictReader(stream, delimiter="\t")
                      if row["syntax_role"] in ("TRANSFER_CONTRACT_REVIEW", "BORROWED_SOURCE_RISK")]
    assert len(candidates) == 12, len(candidates)
    rows = [(row["legacy_site"], row["syntax_role"], *contract(row["legacy_site"]))
            for row in candidates]
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "syntax_role", "source_contract", "release_contract",
                         "followup", "finding"))
        writer.writerows(rows)
    print(f"{len(rows)} hold transfer contracts traced; {sum(r[-1] == 'BUG' for r in rows)} bugs")


if __name__ == "__main__":
    main()
