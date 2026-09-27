#!/usr/bin/env python3
"""Review legacy C entry parameter spelling without assuming ABI compatibility."""

import csv
import re
from collections import Counter
from pathlib import Path


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-parameter-review.tsv"
NAME_ONLY = {
    "gimp_mypaint_brush_save":
        ("GimpData*data,GError**error", "GimpData*brush,GError**error"),
    "gimp_mypaint_options_get_mapping_point":
        ("GimpMypaintOptions*options,gchar*prop_name,guint inputs,guint*size,GimpVector2**points",
         "GimpMypaintOptions*options,gchar*name,guint inputs,guint*size,GimpVector2**points"),
}
TYPE_REVIEW = "gimp_perspective_guide_new"


def parameters(signature: str) -> str:
    assert "(" in signature and ")" in signature, signature
    value = signature.split("(", 1)[1].rsplit(")", 1)[0].strip()
    if value == "void":
        value = ""
    return ",".join(re.sub(r"\s*\*\s*", "*", part.strip())
                    for part in value.split(",")) if value else ""


def main() -> None:
    with (ROOT / "c-entry-signatures.tsv").open(newline="", encoding="utf-8") as file:
        pairs = list(csv.DictReader(file, delimiter="\t"))
    rows = []
    for pair in pairs:
        name = pair["symbol"]
        if name == "gimp_tool_options_button_with_popup":
            continue  # Its C and C++ overloads have distinct interfaces.
        cpp = parameters(pair["cpp_parameters"])
        header = parameters(pair["header_parameters"])
        if name in NAME_ONLY:
            assert (cpp, header) == NAME_ONLY[name], (name, cpp, header)
            classification = "PARAMETER_NAME_ONLY"
            action = "Retain parameter types; choose one name for the port"
        elif name == TYPE_REVIEW:
            assert cpp == "guint id" and header == "guint32 guide_ID", (cpp, header)
            classification = "TYPE_SPELLING_REVIEW"
            action = "Choose one public type and check the ABI on target platforms"
        else:
            assert cpp == header, (name, cpp, header)
            classification = "TEXT_MATCH_AFTER_NORMALIZATION"
            action = "Check return type and actual caller contract during port"
        rows.append((name, pair["cpp_definition"], pair["c_header"], cpp,
                     header, classification, action, "DONE"))

    counts = Counter(row[5] for row in rows)
    assert len(rows) == 64 and counts == {
        "TEXT_MATCH_AFTER_NORMALIZATION": 61,
        "PARAMETER_NAME_ONLY": 2,
        "TYPE_SPELLING_REVIEW": 1,
    }, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_site", "header_site", "cpp_parameters",
                         "header_parameters", "classification", "port_action", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} legacy C entry parameter pairs classified: {dict(counts)}")


if __name__ == "__main__":
    main()
