#!/usr/bin/env python3
"""Classify legacy extern C scopes in C++ sources with source anchors."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION, without_comments


ROOT = Path("migration/inventory")


def without_literals(source: str) -> str:
    code = without_comments(source)
    pattern = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
    return pattern.sub(lambda match: re.sub(r"[^\n]", " ", match[0]), code)


def matching_brace(clean: str, opening: int) -> int:
    depth = 0
    for position in range(opening, len(clean)):
        if clean[position] == "{":
            depth += 1
        elif clean[position] == "}":
            depth -= 1
            if depth == 0:
                return position
    raise ValueError(f"Unclosed extern C block at offset {opening}")


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    blocks = [row for row in candidates if row["kind"] == "extern_c"]
    cache: dict[str, str] = {}
    results = []
    for block in blocks:
        path = block["source"]
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
        source = cache[path]
        clean = without_literals(source)
        line = int(block["line"])
        offset = sum(map(len, source.splitlines(keepends=True)[:line - 1]))
        opening = re.search(r'extern\s+"C"\s*\{', source[offset:])
        assert opening and "\n" not in source[offset:offset + opening.start()], block
        start = offset + opening.end() - 1
        end = matching_brace(clean, start)
        end_line = clean.count("\n", 0, end) + 1
        body = source[start + 1:end]
        headers = re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', body, re.M)
        # This count is only for named function definitions in the scope. GLib
        # registration macros and local declarations do not match this pattern.
        definitions = re.findall(r"\bgimp_[A-Za-z0-9_]+\s*\([^;]*\)\s*\{",
                                 without_comments(body), re.S)
        if headers and definitions:
            role = "INCLUDES_AND_DEFINITIONS"
        elif headers:
            role = "HEADER_INCLUDE_SCOPE"
        elif definitions:
            role = "DEFINITION_SCOPE"
        else:
            role = "LOCAL_DECLARATIONS"
            assert path in ("app/tools/gimpimagegeneratortool.cpp",
                            "app/tools/gimpperspectiveguidetool.cpp"), block
        results.append((path, line, end_line, role, len(headers),
                        len(definitions), "; ".join(headers) or "-", "DONE"))

    counts = Counter(row[3] for row in results)
    assert len(results) == 69 and counts == {
        "HEADER_INCLUDE_SCOPE": 55,
        "DEFINITION_SCOPE": 9,
        "INCLUDES_AND_DEFINITIONS": 3,
        "LOCAL_DECLARATIONS": 2,
    }, counts
    with (ROOT / "extern-c-block-review.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("source", "opening_line", "closing_line", "scope_role",
                         "include_count", "named_definition_count", "included_headers", "status"))
        writer.writerows(results)
    print(f"{len(results)} legacy extern C scopes classified: {dict(counts)}")


if __name__ == "__main__":
    main()
