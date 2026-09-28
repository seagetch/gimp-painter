#!/usr/bin/env python3
"""Trace C-held callbacks into legacy C++ implementations and their consumers."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-pointer-registration-review.tsv"

FACTORY = {
    "gimp_mypaint_brush_load": (
        "GimpDataLoadFunc", "app/core/gimpdatafactory.h:30",
        "app/core/gimpdatafactory.c:208", "app/core/gimpdatafactory.c:892",
        "PERSISTENT_FACTORY_POINTER", "Load returns a GList of GimpData objects"),
    "gimp_mypaint_brush_new": (
        "GimpDataNewFunc", "app/core/gimpdatafactory.h:28",
        "app/core/gimpdatafactory.c:211", "app/core/gimpdatafactory.c:515",
        "PERSISTENT_FACTORY_POINTER", "New returns one GimpData object"),
}
TOOL = {
    "gimp_mypaint_tool_register": ("app/tools/gimpmypainttool.cpp:813", "CALLS_PROVIDED_CALLBACK"),
    "gimp_bucket_fill_brush_tool_register": (
        "app/tools/gimpbucketfillbrushtool.cpp:490", "BYPASSES_PROVIDED_CALLBACK"),
    "gimp_perspective_guide_tool_register": (
        "app/tools/gimpperspectiveguidetool.cpp:627", "BYPASSES_PROVIDED_CALLBACK"),
    "gimp_image_generator_tool_register": (
        "app/tools/gimpimagegeneratortool.cpp:437", "BYPASSES_PROVIDED_CALLBACK"),
}


def main():
    with (ROOT / "c-reference-review.tsv").open(newline="", encoding="utf-8") as file:
        references = [r for r in csv.DictReader(file, delimiter="\t")
                      if r["source_kind"] == "c_source"
                      and r["reference_role"] == "FUNCTION_POINTER_REGISTRATION"]
    assert len(references) == 6
    cache = {}

    def lines(path):
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True,
                encoding="utf-8").splitlines()
        return cache[path]

    def anchored(site, token):
        path, number = site.rsplit(":", 1)
        value = lines(path)[int(number) - 1]
        assert token in value, (site, token, value)
        return site

    results = []
    for ref in references:
        symbol, site = ref["symbol"], ref["legacy_site"]
        anchored(site, symbol)
        if symbol in FACTORY:
            kind, typedef, store, invoke, lifetime, contract = FACTORY[symbol]
            anchored(typedef, kind)
            anchored(store, "factory->priv->" + (
                "loader_entries" if symbol.endswith("_load") else "data_new_func"))
            anchored(invoke, "loader->load_func" if symbol.endswith("_load")
                     else "factory->priv->data_new_func")
            if symbol.endswith("_load"):
                # gimp.c declares the table static, and the factory retains
                # its address, not a copy of a stack-local array.
                context = lines("app/core/gimp.c")[int(site.rsplit(":", 1)[1]) - 3]
                assert "static const GimpDataFactoryLoaderEntry" in context
            sig = typedef
            behavior = "FACTORY_DEFERRED_CALL"
            ownership = contract + "; inspect object/list ownership in the port"
        else:
            sig = anchored("app/tools/tools-types.h:66", "GimpToolRegisterFunc")
            anchored("app/tools/gimp-tools.c:220", "register_funcs[i]")
            entry, behavior = TOOL[symbol]
            anchored(entry, symbol)
            body = "\n".join(lines(entry.rsplit(":", 1)[0])[
                int(entry.rsplit(":", 1)[1]) - 1:
                int(entry.rsplit(":", 1)[1]) + 48])
            if behavior == "CALLS_PROVIDED_CALLBACK":
                assert re.search(r"\(\*\s*callback\s*\)\s*\(", body)
                invoke = entry
            else:
                assert "gimp_tools_register (gimp, tool_info)" in body
                assert not re.search(r"\(\*\s*callback\s*\)\s*\(", body)
                invoke = entry
            store = "app/tools/gimp-tools.c:133"
            lifetime = "SYNCHRONOUS_REGISTER_ARRAY"
            ownership = ("Callback receives Gimp as data" if behavior == "CALLS_PROVIDED_CALLBACK"
                         else "Direct registration stores tool info and GUI function pointers")
        results.append((symbol, site, sig, store, invoke, lifetime, behavior,
                        ownership, "DONE"))
    assert {r[0] for r in results} == set(FACTORY) | set(TOOL)
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "c_registration", "callback_typedef",
                         "storage_site", "invocation_or_entry", "storage_lifetime",
                         "registration_behavior", "port_ownership_check", "status"))
        writer.writerows(results)
    print("6 C pointer registrations traced: 2 factory callbacks, "
          "1 tool callback and 3 direct tool registrations")


if __name__ == "__main__":
    main()
