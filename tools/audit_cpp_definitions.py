#!/usr/bin/env python3
"""Separate callable legacy C entries from local and disabled definition candidates."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "cpp-definition-review.tsv"


def rows(path):
    with (ROOT / path).open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def disabled_lines(lines):
    """Track literal #if 0 blocks, including nesting and else branches."""
    active = []
    disabled = set()
    for number, line in enumerate(lines, 1):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            action, rest = directive.groups()
            if action in ("if", "ifdef", "ifndef"):
                active.append(action == "if" and bool(re.match(r"\s*0\b", rest)))
            elif action == "else" and active:
                # An outer disabled branch still keeps the line disabled.
                active[-1] = not active[-1]
            elif action == "endif" and active:
                active.pop()
        if any(active):
            disabled.add(number)
    assert not active
    return disabled


def main():
    definitions = [r for r in rows("cpp-call-boundary-candidates.tsv")
                   if r["kind"] == "c_definition_candidate"]
    signatures = {(r["cpp_definition"], r["symbol"]): r["c_header"]
                  for r in rows("c-entry-signatures.tsv")}
    assert len(definitions) == 149 and len(signatures) == 66
    sources = {}
    result = []
    for row in definitions:
        path, line = row["source"], int(row["line"])
        if path not in sources:
            source = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
            lines = source.splitlines()
            sources[path] = (source, lines, disabled_lines(lines))
        source, lines, inactive = sources[path]
        assert row["legacy_code"] == lines[line - 1].strip()[:300], (path, line)
        symbol = re.search(r"\b(gimp_[A-Za-z0-9_]+)\s*\(", row["legacy_code"])[1]
        site = f"{path}:{line}"
        declaration = signatures.get((site, symbol), "-")
        previous = lines[line - 2].strip() if line > 1 else ""
        if line in inactive:
            assert path == "app/paint/gimpmypaintcore.cpp" and declaration == "-"
            classification = "DISABLED_IF_0"
            direction = "No compiled entry; retain only as historical reference"
        elif symbol == "gimp_tool_options_button_with_popup" and (
                path.endswith("gimptooloptions-gui-cxx.hpp") or
                path.endswith("gimptooloptions-gui-cxx.cpp")):
            assert symbol == "gimp_tool_options_button_with_popup"
            assert declaration != "-", site
            classification = "SEPARATE_CPP_OVERLOAD"
            direction = "C++ helper; separate C implementation in gimptooloptions-gui.c:201"
        elif declaration != "-":
            classification = "C_HEADER_ENTRY"
            direction = "C header declaration to C++ definition; compare return type and owner"
        elif re.search(r"\bstatic\b", previous):
            classification = "FILE_LOCAL_STATIC"
            direction = "File-local function; inspect callback registration or C++ caller"
        elif previous.startswith("template<"):
            classification = "CPP_TEMPLATE_HELPER"
            direction = "C++ template helper; no C caller found in app"
        else:
            assert symbol in ("gimp_image_generator_options_get_type",
                              "gimp_perspective_guide_options_get_type"), site
            # These declarations live inside a local extern C block; their
            # out-of-block definitions inherit C linkage in this translation unit.
            declaration_line = next((i for i, value in enumerate(lines[:line - 1], 1)
                                     if re.search(r"\b" + re.escape(symbol) +
                                                  r"\s*\(\s*void\s*\)\s*;", value)), None)
            assert declaration_line is not None, site
            declaration = f"{path}:{declaration_line}"
            classification = "LOCAL_C_DECLARED_GET_TYPE"
            direction = "C linkage via local extern C declaration; verify GType registration"
        result.append((symbol, site, declaration, classification, previous,
                       direction, "DONE"))

    counts = Counter(row[3] for row in result)
    assert counts == {"C_HEADER_ENTRY": 64, "SEPARATE_CPP_OVERLOAD": 2,
                      "FILE_LOCAL_STATIC": 71, "CPP_TEMPLATE_HELPER": 2,
                      "DISABLED_IF_0": 8, "LOCAL_C_DECLARED_GET_TYPE": 2}, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_site", "declaration_site", "classification",
                         "preceding_source", "port_action", "status"))
        writer.writerows(result)
    print(f"{len(result)} legacy C++ definition candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
