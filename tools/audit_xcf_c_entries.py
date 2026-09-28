#!/usr/bin/env python3
"""Trace legacy XCF C calls into the C++ layer entry points and owners."""

import csv
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "xcf-c-entry-review.tsv"

# The call-site and implementation anchors are intentionally explicit. They
# document behavior that a mere symbol or return-type match would miss.
SPECS = {
    ("app/xcf/xcf-load.c:1019", "gimp_filter_layer_new"):
        ("REPLACE_LAYER", "app/core/gimpfilterlayer.cpp:1175",
         "Build a distinct filter layer, copy old layer properties, then replace *layer"),
    ("app/xcf/xcf-load.c:1020", "gimp_filter_layer_mark_as_loaded"):
        ("SET_LOADED_FLAG", "app/core/gimpfilterlayer.cpp:1206",
         "Set FilterLayer::loaded before replaying the saved procedure"),
    ("app/xcf/xcf-load.c:1048", "gimp_filter_layer_set_procedure"):
        ("COPY_VALUES_BEFORE_ARRAY_RELEASE", "app/core/gimpfilterlayer.cpp:1198",
         "Caller unrefs proc_args; setter transforms values into runner arguments without retaining GArray"),
    ("app/xcf/xcf-load.c:1082", "gimp_clone_layer_new"):
        ("REPLACE_LAYER_WITH_NULL_SOURCE", "app/core/gimpclonelayer.cpp:669",
         "Construct clone without a source; copy old layer properties and replace *layer"),
    ("app/xcf/xcf-load.c:1106", "gimp_clone_layer_set_source_by_name"):
        ("DEFER_SOURCE_LOOKUP", "app/core/gimpclonelayer.cpp:696",
         "Setter duplicates saved name; source resolution is deferred"),
    ("app/xcf/xcf-load.c:2528", "gimp_clone_layer_get_source"):
        ("IGNORE_RETURN_FOR_LAZY_RESOLUTION", "app/core/gimpclonelayer.cpp:688",
         "Post-load walk calls getter for its lazy name resolution side effect"),
    ("app/xcf/xcf-save.c:660", "gimp_filter_layer_get_procedure"):
        ("BORROWED_PROCEDURE_NAME", "app/core/gimpfilterlayer.cpp:1186",
         "Returned string comes from runner or empty literal; serialize before runner changes"),
    ("app/xcf/xcf-save.c:661", "gimp_filter_layer_get_procedure_args"):
        ("ALLOCATED_ARGUMENT_ARRAY", "app/core/gimpfilterlayer.cpp:1192",
         "Getter shallow-appends GValues from copied GValueArray; legacy writer frees neither allocation"),
    ("app/xcf/xcf-save.c:666", "gimp_clone_layer_get_source"):
        ("BORROWED_NULLABLE_SOURCE", "app/core/gimpclonelayer.cpp:688",
         "Getter resolves name and returns source pointer; writer dereferences it without null guard"),
}


def main():
    with (ROOT / "c-reference-review.tsv").open(newline="", encoding="utf-8") as file:
        references = {(r["legacy_site"], r["symbol"]): r
                      for r in csv.DictReader(file, delimiter="\t")
                      if r["reference_role"] == "DIRECT_CALL"
                      and r["legacy_site"].startswith("app/xcf/")}
    assert set(references) == set(SPECS), (set(references) - set(SPECS),
                                           set(SPECS) - set(references))
    with (ROOT / "cpp-definition-review.tsv").open(newline="", encoding="utf-8") as file:
        definitions = {r["cpp_site"]: r for r in csv.DictReader(file, delimiter="\t")}
    sources = {}

    def anchored(site, token):
        path, number = site.rsplit(":", 1)
        if path not in sources:
            sources[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"],
                text=True, encoding="utf-8").splitlines()
        value = sources[path][int(number) - 1]
        assert token in value, (site, token, value)
        return value

    results = []
    for (call, name), (effect, definition, contract) in SPECS.items():
        assert references[(call, name)]["target_kind"] == "CPP_ENTRY"
        anchored(call, name)
        assert definitions[definition]["symbol"] == name
        assert definitions[definition]["classification"] == "C_HEADER_ENTRY"
        anchored(definition, name)
        results.append((call, name, definition, effect, contract, "DONE"))

    # Ownership and side-effect checks beyond the single-line call anchors.
    anchored("app/xcf/xcf-load.c:1049", "g_array_unref(proc_args)")
    anchored("app/core/gimpfilterlayer.cpp:630", "FilterLayer::set_procedure")
    anchored("app/core/gimpfilterlayer.cpp:667", "FilterLayer::get_procedure_args")
    anchored("app/core/gimpfilterlayer.cpp:674", "g_array_append_vals")
    anchored("app/pdb/pdb-cxx-utils.hpp:270", "get_args()")
    anchored("app/pdb/pdb-cxx-utils.hpp:272", "g_value_array_copy(args)")
    anchored("app/core/gimpclonelayer.cpp:303", "get_source()")
    anchored("app/core/gimpclonelayer.cpp:336", "set_source_by_name")
    anchored("app/core/gimpclonelayer.cpp:339", "g_strdup(name)")
    anchored("app/xcf/xcf-save.c:667", "gimp_object_get_name (target_layer)")
    anchored("app/xcf/xcf-load.c:2527", "GIMP_IS_CLONE_LAYER")
    assert "g_array_unref" not in "\n".join(sources["app/xcf/xcf-save.c"][659:668])

    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("c_call_site", "symbol", "cpp_entry", "observed_effect",
                         "ownership_and_port_contract", "status"))
        writer.writerows(results)
    print("9 XCF-to-C++ calls traced: 6 load and 3 save")


if __name__ == "__main__":
    main()
