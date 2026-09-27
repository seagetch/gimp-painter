#!/usr/bin/env python3
"""Classify first-pass function-pointer syntax in legacy C++ files."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
DECLARATIONS = {
    "app/base/delegators.hpp:106", "app/base/delegators.hpp:228",
    "app/base/glib-cxx-impl.hpp:345", "app/base/glib-cxx-impl.hpp:397",
    "app/base/glib-cxx-impl.hpp:449", "app/base/glib-cxx-impl.hpp:467",
    "app/base/glib-cxx-impl.hpp:474", "app/base/glib-cxx-impl.hpp:493",
    "app/base/glib-cxx-types.hpp:53", "app/base/scopeguard.hpp:10",
    "app/gimp-features.cpp:48", "app/widgets/popupper.cpp:563",
    "app/widgets/popupper.cpp:568",
}


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    pointers = [row for row in candidates if row["kind"] == "function_pointer"]
    cache: dict[str, list[str]] = {}
    rows = []
    for candidate in pointers:
        path = candidate["source"]
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        line = int(candidate["line"])
        code = cache[path][line - 1]
        assert code.strip() == candidate["legacy_code"], candidate
        match = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(", code)
        assert match, candidate
        site = f"{path}:{line}"
        role = "POINTER_DECLARATION" if site in DECLARATIONS else "INDIRECT_CALL"
        rows.append((site, match[1], role, candidate["legacy_code"], "DONE"))

    counts = Counter(row[2] for row in rows)
    assert len(rows) == 34 and counts == {"POINTER_DECLARATION": 13,
                                       "INDIRECT_CALL": 21}, counts
    with (ROOT / "function-pointer-review.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "pointer_name", "syntax_role", "legacy_code", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} function-pointer sites classified: {dict(counts)}")


if __name__ == "__main__":
    main()
