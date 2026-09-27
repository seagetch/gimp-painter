#!/usr/bin/env python3
"""Classify first-pass C and header references to legacy C++ entry names."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION, without_comments


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-reference-review.tsv"
C_ONLY = "gimp_tool_options_button_with_popup"


def read_table(filename: str) -> list[dict[str, str]]:
    with (ROOT / filename).open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def main() -> None:
    references = read_table("c-call-reference-candidates.tsv")
    signatures = read_table("c-entry-signatures.tsv")
    cpp_symbols = {pair["symbol"] for pair in signatures if pair["symbol"] != C_ONLY}
    source_cache: dict[str, list[str]] = {}
    results = []
    for ref in references:
        path = ref["source"]
        if path not in source_cache:
            source_cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        line = int(ref["line"])
        code = source_cache[path][line - 1]
        assert code.strip() == ref["legacy_code"], ref
        clean = without_comments(code).strip()
        name = ref["symbol"]
        target = "C_IMPLEMENTATION" if name == C_ONLY else "CPP_ENTRY"
        if ref["kind"] == "header":
            if not clean:
                role = "COMMENTED_OUT"
            elif clean.startswith("#"):
                role = "TYPE_MACRO_REFERENCE"
            elif "template<>" in clean:
                role = "CPP_TEMPLATE_REFERENCE"
            else:
                assert name in cpp_symbols or name == C_ONLY, ref
                assert re.search(r"\b" + re.escape(name) + r"\s*\(", clean), ref
                role = "DECLARATION"
        elif ref["kind"] == "c_source":
            if not clean:
                role = "COMMENTED_OUT"
            elif path == "app/tools/gimptooloptions-gui.c" and line == 201:
                assert name == C_ONLY and re.search(r"\b" + name + r"\s*\(", clean)
                role = "C_DEFINITION"
            elif (re.search(r"\b" + re.escape(name) + r"\s*,", clean)
                  or (path == "app/tools/gimp-tools.c" and clean == name)):
                role = "FUNCTION_POINTER_REGISTRATION"
            else:
                assert re.search(r"\b" + re.escape(name) + r"\s*\(", clean), ref
                role = "DIRECT_CALL"
        else:
            raise ValueError(f"Unexpected reference kind: {ref}")
        results.append((name, f"{path}:{line}", ref["kind"], role, target,
                        ref["legacy_code"], "DONE"))

    counts = Counter(row[3] for row in results)
    assert len(results) == 130 and counts == {
        "DECLARATION": 65, "TYPE_MACRO_REFERENCE": 11,
        "CPP_TEMPLATE_REFERENCE": 3, "COMMENTED_OUT": 3,
        "C_DEFINITION": 1, "FUNCTION_POINTER_REGISTRATION": 6,
        "DIRECT_CALL": 41,
    }, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "legacy_site", "source_kind", "reference_role",
                         "target_kind", "legacy_code", "status"))
        writer.writerows(results)
    print(f"{len(results)} candidate references classified: {dict(counts)}")


if __name__ == "__main__":
    main()
