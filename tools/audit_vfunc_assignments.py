#!/usr/bin/env python3
"""Locate legacy vtable assignments and their callback definitions."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION, cpp_function_definitions, without_comments


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "vfunc-assignment-review.tsv"
BRIDGE_DEFINITIONS = {
    "GClassWrapper::set_property": "app/base/glib-cxx-impl.hpp:185",
    "GClassWrapper::get_property": "app/base/glib-cxx-impl.hpp:216",
    "instance_finalize": "app/base/glib-cxx-impl.hpp:413",
}
EXTERNAL_DEFINITIONS = {
    "gimp_mypaint_brush_save": "app/core/gimpmypaintbrush-save.cpp:101",
}
ASSIGNMENT = re.compile(
    r"^\s*(?P<receiver>(?:\w+|\w+\([^)]*\)))"
    r"->(?P<slot>\w+)\s*=\s*(?P<value>.+?)\s*;\s*$"
)


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    assignments = [row for row in candidates if row["kind"] == "vfunc_candidate"]
    source_cache: dict[str, list[str]] = {}
    definition_cache: dict[str, dict[str, list[int]]] = {}

    def lines(path: str) -> list[str]:
        if path not in source_cache:
            source_cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        return source_cache[path]

    def definitions(path: str) -> dict[str, list[int]]:
        if path not in definition_cache:
            source = "\n".join(lines(path)) + "\n"
            result: dict[str, list[int]] = {}
            for name, line in cpp_function_definitions(without_comments(source)):
                result.setdefault(name, []).append(line)
            definition_cache[path] = result
        return definition_cache[path]

    results = []
    for row in assignments:
        path = row["source"]
        line = int(row["line"])
        code = lines(path)[line - 1]
        assert code.strip() == row["legacy_code"], row
        match = ASSIGNMENT.fullmatch(code)
        assert match, row
        receiver, slot, value = match.group("receiver", "slot", "value")
        if value in BRIDGE_DEFINITIONS:
            role = "BRIDGE_CALLBACK"
            target = BRIDGE_DEFINITIONS[value]
        elif value in EXTERNAL_DEFINITIONS:
            role = "CROSS_FILE_CALLBACK"
            target = EXTERNAL_DEFINITIONS[value]
        elif re.fullmatch(r"gimp_[A-Za-z0-9_]+", value):
            locations = definitions(path).get(value, [])
            assert len(locations) == 1, (path, value, locations)
            role = "LOCAL_CALLBACK"
            target = f"{path}:{locations[0]}"
        else:
            assert (value == "TRUE" or value.startswith(('"', '_(', 'C_('))), row
            role = "CLASS_METADATA"
            target = "-"
        if role == "CLASS_METADATA":
            port_check = "Preserve class metadata, not a callback ABI"
        elif value in ("GClassWrapper::set_property", "GClassWrapper::get_property"):
            port_check = "Replace catch-all process exit with a defined C ABI error path"
        elif slot in ("dispose", "finalize", "constructed"):
            port_check = "Verify lifecycle ordering, parent chaining and exception containment"
        else:
            port_check = "Verify GIMP 3 slot signature, owner and exception containment"
        results.append((f"{path}:{line}", receiver, slot, value, role, target,
                        port_check, "DONE"))

    counts = Counter(row[4] for row in results)
    assert len(results) == 63 and counts == {
        "LOCAL_CALLBACK": 36,
        "BRIDGE_CALLBACK": 3,
        "CROSS_FILE_CALLBACK": 1,
        "CLASS_METADATA": 23,
    }, counts
    for _, _, _, value, role, location, _, _ in results:
        if role != "CLASS_METADATA":
            path, line = location.rsplit(":", 1)
            assert 1 <= int(line) <= len(lines(path)), location
            if role != "LOCAL_CALLBACK":
                assert re.search(r"\b" + re.escape(value.split("::")[-1]) + r"\s*\(",
                                 lines(path)[int(line) - 1]), location
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("assignment_site", "receiver", "slot", "assigned_value",
                         "role", "target_definition", "port_check", "status"))
        writer.writerows(results)
    print(f"{len(results)} vtable candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
