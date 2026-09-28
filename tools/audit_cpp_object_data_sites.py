#!/usr/bin/env python3
"""Inventory legacy C++ GObject data/qdata calls with source anchors."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION

OUTPUT = Path("migration/inventory/cpp-object-data-sites.tsv")
CALL = re.compile(r"\b(?P<op>g_object_(?:set|get|steal|replace)_(?:qdata|data)(?:_full)?|"
                  r"g_object_set_cxx_object)\s*\(")
SUFFIXES = (".cpp", ".hpp", ".cc", ".hh", ".cxx")


def split_args(source, start):
    depth = 1
    quoted = None
    parts = []
    field_start = start
    for index in range(start, len(source)):
        char = source[index]
        if quoted:
            if char == quoted and (index == 0 or source[index - 1] != "\\"):
                quoted = None
        elif char in "\"'":
            quoted = char
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                parts.append(source[field_start:index].strip())
                return parts
        elif char == "," and depth == 1:
            parts.append(source[field_start:index].strip())
            field_start = index + 1
    raise AssertionError("unclosed call")


def role(path, line, operation):
    if path == "app/base/delegators.hpp":
        return "HELPER_DEFINITION" if line in {195, 201} else "HELPER_FORWARD"
    if "steal" in operation:
        return "TRANSFER_READ"
    if "get" in operation:
        return "BORROWED_READ"
    if operation == "g_object_set_cxx_object" or operation.endswith("_full"):
        return "OWNING_ATTACHMENT"
    return "BORROWED_ATTACHMENT"


def main():
    paths = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", REVISION, "app"], text=True
    ).splitlines()
    sources = [path for path in paths if path.endswith(SUFFIXES)]
    assert len(sources) == 71, len(sources)
    rows = []
    for path in sources:
        source = subprocess.check_output(
            ["git", "show", f"{REVISION}:{path}"], text=True, errors="replace"
        )
        lines = source.splitlines()
        for match in CALL.finditer(source):
            line = source.count("\n", 0, match.start()) + 1
            if lines[line - 1].strip().startswith(("//", "*", "#")):
                continue
            args = split_args(source, match.end())
            operation = match["op"]
            expected = (3 if path == "app/base/delegators.hpp" and line in {195, 201}
                        else 4 if operation.endswith("_full")
                        else 3 if "set" in operation else 2)
            assert len(args) == expected, (path, line, operation, args)
            rows.append((f"{path}:{line}", operation, args[0], args[1],
                         args[2] if len(args) > 2 else "-",
                         args[3] if len(args) > 3 else "-", role(path, line, operation),
                         "DONE"))
    counts = Counter(row[6] for row in rows)
    assert len(rows) == 30, len(rows)
    assert counts == {"HELPER_DEFINITION": 2, "HELPER_FORWARD": 2,
                      "OWNING_ATTACHMENT": 10, "BORROWED_ATTACHMENT": 9,
                      "BORROWED_READ": 6, "TRANSFER_READ": 1}, counts
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "owner_expr", "key_expr",
                         "value_expr", "destroy_expr", "syntax_role", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} legacy C++ GObject data/qdata sites: {dict(counts)}")


if __name__ == "__main__":
    main()
