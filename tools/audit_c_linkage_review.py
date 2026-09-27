#!/usr/bin/env python3
"""Record how legacy C++ sources used declarations in unguarded C headers."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
INCLUDES = {
    "app/core/gimpmypaintbrush-load.cpp": 59,
    "app/core/gimpmypaintbrush-save.cpp": 59,
    "app/core/gimpmypaintbrush.cpp": 39,
    "app/display/gimpdisplayshell-overlays.cpp": 87,
    "app/paint/gimpmypaintoptions.cpp": 44,
    "app/tools/gimpmypaintoptions-gui.cpp": 75,
    "app/tools/gimpmypainttool.cpp": 56,
    "app/widgets/gimpmypaintbrusheditor.cpp": 41,
}
EXPLICIT_DEFINITION = "app/tools/gimpmypaintbrushoptions-gui.cpp"
CPP_OVERLOADS = {
    "app/tools/gimptooloptions-gui-cxx.cpp",
    "app/tools/gimptooloptions-gui-cxx.hpp",
}


def read_table(filename: str) -> list[dict[str, str]]:
    with (ROOT / filename).open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def legacy_source(path: str) -> list[str]:
    return subprocess.check_output(
        ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
    ).splitlines()


def main() -> None:
    pairs = [row for row in read_table("c-entry-signatures.tsv")
             if row["g_begin_decls_guard"] == "review"]
    refs = read_table("c-call-reference-candidates.tsv")
    cache: dict[str, list[str]] = {}

    def source(path: str) -> list[str]:
        if path not in cache:
            cache[path] = legacy_source(path)
        return cache[path]

    rows = []
    for pair in pairs:
        path, line_text = pair["cpp_definition"].rsplit(":", 1)
        line = int(line_text)
        name = pair["symbol"]
        lines = source(path)
        assert re.search(r"\b" + re.escape(name) + r"\s*\(", lines[line - 1]), pair
        if path in INCLUDES:
            include_line = INCLUDES[path]
            header = pair["c_header"].rsplit(":", 1)[0].split("/")[-1]
            assert f'#include "{header}"' in lines[include_line - 1], pair
            openings = [i for i, text in enumerate(lines[:include_line], 1)
                        if re.search(r'extern\s+"C"\s*\{', text)]
            assert openings, pair
            opening = openings[-1]
            assert not any(re.match(r"^\s*}\s*;?\s*(?://.*)?$", text)
                           for text in lines[opening:include_line - 1]), pair
            kind = "INCLUDED_UNDER_EXTERN_C"
            evidence = f"{path}:{opening},{include_line}"
            port_action = "Give the C declaration an explicit linkage guard"
        elif path == EXPLICIT_DEFINITION:
            assert re.search(r'extern\s+"C"\s*\{', lines[321]), pair
            assert line == 325, pair
            kind = "DEFINITION_UNDER_EXTERN_C"
            evidence = f"{path}:322,325"
            port_action = "Give the header declaration an explicit linkage guard"
        elif path in CPP_OVERLOADS:
            assert "PopupCreateViewDelegator*" in pair["cpp_parameters"], pair
            assert "GimpPopupCreateViewCallbackExt" in pair["header_parameters"], pair
            kind = "DISTINCT_CPP_OVERLOAD"
            evidence = "app/tools/gimptooloptions-gui.c:201; " + pair["cpp_definition"]
            port_action = "Keep the C helper and C++ overload signatures distinct"
        else:
            raise ValueError(f"Unclassified unguarded pair: {pair}")
        callers = sorted({f"{ref['source']}:{ref['line']}" for ref in refs
                          if ref["symbol"] == name and ref["source"].endswith(".c")})
        rows.append((name, pair["cpp_definition"], pair["c_header"], kind,
                     evidence, "; ".join(callers) or "-", port_action, "DONE"))

    assert len(rows) == 28, len(rows)
    with (ROOT / "c-linkage-review.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_site", "unguarded_header", "classification",
                         "legacy_evidence", "c_source_references", "port_action", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} legacy linkage pairs classified")


if __name__ == "__main__":
    main()
