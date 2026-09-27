#!/usr/bin/env python3
"""Pair legacy C++ definitions with C header declarations for review."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION, without_comments


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-entry-signatures.tsv"


def load_rows(name: str) -> list[dict[str, str]]:
    with (ROOT / name).open(encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def signature(source: str, line: int, symbol: str) -> str | None:
    clean = without_comments(source)
    start = sum(len(part) for part in clean.splitlines(keepends=True)[:line - 1])
    match = re.search(r"\b" + re.escape(symbol) + r"\s*\(", clean[start:])
    if not match or "\n" in clean[start:start + match.start()]:
        return None
    begin = start + match.start()
    position = start + match.end()
    depth = 1
    while position < len(clean) and depth:
        if clean[position] == "(":
            depth += 1
        elif clean[position] == ")":
            depth -= 1
        position += 1
    if depth:
        return None
    return " ".join(clean[begin:position].split())


def c_linkage_guard(source: str, line: int) -> bool:
    lines = source.splitlines()
    before = "\n".join(lines[:line - 1])
    after = "\n".join(lines[line:])
    if "G_BEGIN_DECLS" in before and "G_END_DECLS" in after:
        return True
    opening = list(re.finditer(r'extern\s+"C"\s*\{', before))
    if not opening:
        return False
    closing = list(re.finditer(r"#\s*ifdef\s+__cplusplus\s*\n\s*\}", before))
    if closing and closing[-1].start() > opening[-1].start():
        return False
    return bool(re.search(r"#\s*ifdef\s+__cplusplus\s*\n\s*\}", after))


def main() -> None:
    candidates = load_rows("cpp-call-boundary-candidates.tsv")
    references = load_rows("c-call-reference-candidates.tsv")
    definitions = [row for row in candidates if row["kind"] == "c_definition_candidate"]
    headers = [row for row in references if row["kind"] == "header"
               and not row["legacy_code"].lstrip().startswith("#")
               and "template<>" not in row["legacy_code"]
               and "return " not in row["legacy_code"]]
    cache: dict[str, str] = {}

    def source(path: str) -> str:
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8")
        return cache[path]

    results = []
    for definition in definitions:
        name = re.search(r"\b(gimp_[A-Za-z0-9_]+)\s*\(", definition["legacy_code"])
        if not name:
            continue
        symbol = name[1]
        cpp_location = f"{definition['source']}:{definition['line']}"
        cpp_signature = signature(source(definition["source"]), int(definition["line"]), symbol)
        for header in (h for h in headers if h["symbol"] == symbol):
            header_location = f"{header['source']}:{header['line']}"
            declaration = signature(source(header["source"]), int(header["line"]), symbol)
            if declaration is None:
                continue
            linkage = c_linkage_guard(source(header["source"]), int(header["line"]))
            results.append((symbol, cpp_location, header_location, cpp_signature or "",
                            declaration, "yes" if linkage else "review", "REVIEW"))

    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_definition", "c_header", "cpp_parameters",
                         "header_parameters", "g_begin_decls_guard", "status"))
        writer.writerows(results)
    print(f"{len(results)} definition/declaration pairs; "
          f"{len({row[0] for row in results})} distinct C entry names")


if __name__ == "__main__":
    main()
