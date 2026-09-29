#!/usr/bin/env python3
"""Cover legacy C++ GValue syntax with owner and migration follow-up."""

import csv
from pathlib import Path

SOURCE = Path("migration/inventory/cpp-gvalue-review.tsv")
OUTPUT = Path("migration/inventory/cpp-gvalue-owner-contracts.tsv")


def contract(row):
    site, role = row["legacy_site"], row["syntax_role"]
    path = site.rsplit(":", 1)[0]
    if role == "VALUE_WRAPPER_OR_TRAIT":
        if "CopyValue" in row["source_line"]:
            return ("CopyValue heap GValue", "owning Value destructor unsets then frees; copy/move assignment must release old value", "06.020/value-assignment", "WRAPPER")
        return ("GValue wrapper or trait caller", "trait get/set borrows its argument; Value/ValueRef ownership depends on IsOwner and IsManager", "06.020", "WRAPPER")
    if role == "ARRAY_BORROW":
        owner = "popup proc_args Array" if path.endswith("gimplayerpopup.cpp") else "caller GArray"
        follow = "29.006/proc-args-owner" if path.endswith("gimplayerpopup.cpp") else "15.001/gvalue-deep-copy" if path.endswith("gimpfilterlayer.cpp") else "28.007"
        return (owner, "IArray<GValue> adds a temporary GArray ref and drops it on scope exit; element values remain owned by array", follow, "ARRAY_BORROW")
    if role == "VALUE_INITIALIZATION":
        if path.endswith("pdb-cxx-utils.hpp"):
            return ("PDB stack GValue", "g_value_init requires g_value_unset before return; old temporary string value is not unset", "15.001/pdb-temporary-gvalue", "RISK")
        return ("preset stack GValue", "G_VALUE_INIT entries are copied as bytes into GArray with g_value_unset clear function", "28.007", "INITIALIZATION")
    if role == "BORROWED_VALUE_POINTER":
        follow = "15.001/pdb-temporary-gvalue" if path.endswith("pdb-cxx-utils.hpp") else "29.006/proc-args-owner" if path.endswith("gimplayerpopup.cpp") else "06.020"
        return ("callback, param spec or local wrapper supplies GValue", "pointer is borrowed for callback or expression; do not unset externally owned value", follow, "BORROWED")
    assert role == "VALUE_COPY_OR_SIGNATURE", site
    if site == "app/core/gimpfilterlayer.cpp:672":
        return ("FilterLayer returned arguments", "g_array_append_vals shallow-copies GValueArray entries; old source array is not freed", "15.001/gvalue-deep-copy", "RISK")
    if site == "app/widgets/gimplayerpopup.cpp:242":
        return ("popup proc_args Array", "g_array_append_vals shallow-copies GValueArray entries then g_value_array_free invalidates owned payloads", "29.006/proc-args-owner", "RISK")
    if path.endswith("pdb-cxx-utils.hpp"):
        return ("PDB runner args or returned GValue", "by-value GValue is borrowed/shallow; local conversion must be unset and returned values require explicit contract", "15.001/pdb-temporary-gvalue", "SIGNATURE")
    if path.endswith("gimpfilterlayer.cpp"):
        return ("FilterLayer argument or getter", "by-value GValue may alias payload; getter must return defined ownership, setter copies/transform values", "15.001/gvalue-deep-copy", "SIGNATURE")
    if path.endswith("layer-preset.cpp"):
        return ("preset argument array", "new GArray owns values with g_value_unset clear function; append must establish each element's ownership", "28.007", "SIGNATURE")
    return ("GObject property callback argument", "GValue pointer belongs to caller and is valid only during callback", "08.020", "SIGNATURE")


def main():
    with SOURCE.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    assert len(rows) == 136 and len({r["legacy_site"] for r in rows}) == 136
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "syntax_role", "owner", "release_contract", "followup", "finding"))
        writer.writerows((row["legacy_site"], row["syntax_role"], *contract(row)) for row in rows)
    print(f"{len(rows)} GValue sites associated with owner and migration follow-up")


if __name__ == "__main__":
    main()
