#!/usr/bin/env python3
"""Classify syntactic C entry candidates using the definition review."""

import csv
import re
from collections import Counter
from pathlib import Path


ROOT = Path("migration/inventory")


def read(path):
    with (ROOT / path).open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def main():
    candidates = [r for r in read("cpp-call-boundary-candidates.tsv")
                  if r["kind"] == "c_entry_candidate"]
    definitions = {r["cpp_site"]: r for r in read("cpp-definition-review.tsv")}
    rows = []
    for row in candidates:
        site = f"{row['source']}:{row['line']}"
        name = re.search(r"\b(gimp_\w+)\s*\(", row["legacy_code"])[1]
        target = definitions.get(site)
        if target:
            assert target["symbol"] == name
            kind = target["classification"]
            assert kind in ("C_HEADER_ENTRY", "LOCAL_C_DECLARED_GET_TYPE")
            actual = site
            note = "Compiled definition; see cpp-definition-review.tsv"
        elif row["source"].endswith("gimpmypaintcore.hpp"):
            assert name == "gimp_mypaint_core_round_line"
            kind = "HEADER_DECLARATION_OF_DISABLED_DEFINITION"
            actual = "app/paint/gimpmypaintcore.cpp:451"
            assert definitions[actual]["classification"] == "DISABLED_IF_0"
            note = "C++ header declaration only; definition is inside #if 0"
        else:
            assert name in ("gimp_image_generator_options_get_type",
                            "gimp_perspective_guide_options_get_type")
            kind = "TRANSLATION_UNIT_C_DECLARATION"
            actual = next((site for site, item in definitions.items()
                           if item["symbol"] == name), None)
            assert actual and definitions[actual]["classification"] == "LOCAL_C_DECLARED_GET_TYPE"
            note = "Local extern C declaration before compiled definition"
        rows.append((name, site, actual, kind, note, "DONE"))
    counts = Counter(r[3] for r in rows)
    assert counts == {"C_HEADER_ENTRY": 23, "LOCAL_C_DECLARED_GET_TYPE": 2,
                      "TRANSLATION_UNIT_C_DECLARATION": 2,
                      "HEADER_DECLARATION_OF_DISABLED_DEFINITION": 1}, counts
    with (ROOT / "c-entry-candidate-review.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "candidate_site", "definition_site",
                         "classification", "note", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} C entry syntax candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
