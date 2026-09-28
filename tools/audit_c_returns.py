#!/usr/bin/env python3
"""Compare return type spelling at legacy C++ definitions and C headers."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION, without_comments


ROOT = Path("migration/inventory")


def return_type(lines, site, symbol):
    line = int(site.rsplit(":", 1)[1]) - 1
    code = without_comments("\n".join(lines)).splitlines()
    assert symbol in code[line], (site, symbol)
    prefix = code[line].split(symbol, 1)[0].strip()
    if not prefix:
        prefix = code[line - 1].strip()
    assert re.fullmatch(r"(?:const\s+)?[A-Za-z_]\w*(?:\s*\*)?", prefix), (site, prefix)
    return prefix


def normalized(value):
    return re.sub(r"\s*\*\s*", "*", " ".join(value.split()))


def main():
    with (ROOT / "c-entry-signatures.tsv").open(newline="", encoding="utf-8") as file:
        pairs = list(csv.DictReader(file, delimiter="\t"))
    cache = {}
    results = []
    for pair in pairs:
        symbol = pair["symbol"]
        if symbol == "gimp_tool_options_button_with_popup":
            continue  # The C++ overload is a different function from the C header entry.
        types = []
        for site in (pair["cpp_definition"], pair["c_header"]):
            path = site.rsplit(":", 1)[0]
            if path not in cache:
                cache[path] = subprocess.check_output(
                    ["git", "show", f"{REVISION}:{path}"], text=True,
                    encoding="utf-8").splitlines()
            types.append(return_type(cache[path], site, symbol))
        assert normalized(types[0]) == normalized(types[1]), (symbol, types)
        results.append((symbol, pair["cpp_definition"], pair["c_header"],
                        *types, "RETURN_SPELLING_MATCH", "DONE"))
    assert len(results) == 64
    with (ROOT / "c-return-review.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_site", "header_site", "cpp_return",
                         "header_return", "classification", "status"))
        writer.writerows(results)
    print(f"{len(results)} legacy C entry return spellings matched")


if __name__ == "__main__":
    main()
