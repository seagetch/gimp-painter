#!/usr/bin/env python3
"""Map every legacy NewGClass declaration to its C and C++ types."""

import csv
import re
import subprocess
from pathlib import Path


REVISION = "afa43fae3e920210146abed514f136fd49f671b5"


def git(*arguments: str) -> str:
    return subprocess.check_output(["git", *arguments], text=True, errors="surrogateescape")


def arguments(text: str, start: int) -> list[str]:
    depth = 1
    pieces = []
    begin = start
    for position in range(start, len(text)):
        char = text[position]
        if char == "<":
            depth += 1
        elif char == ">":
            depth -= 1
            if depth == 0:
                pieces.append(text[begin:position].strip())
                return pieces
        elif char == "," and depth == 1:
            pieces.append(text[begin:position].strip())
            begin = position + 1
    raise ValueError("unterminated template argument list")


def parse_structs(argument: str, source: str) -> tuple[str, str, str]:
    actual = argument.replace("GLib::", "").strip()
    if actual == "CStructs":
        alias = re.search(r"(?:typedef\s+)(UseCStructs<[^;]+>)\s+CStructs\s*;", source)
        if not alias:
            raise ValueError("CStructs alias without definition")
        actual = alias[1]
    match = re.fullmatch(r"UseCStructs<\s*(\w+)\s*,\s*(\w+)\s*>", actual)
    if match:
        return "UseCStructs", match[1], match[2]
    match = re.fullmatch(r"DerivedFrom<\s*(\w+)\s*>", actual)
    if match:
        return "DerivedFrom", match[1], "generated instance"
    raise ValueError(f"unknown C struct strategy: {actual}")


def main() -> None:
    paths = git("ls-tree", "-r", "--name-only", REVISION, "app").splitlines()
    rows = []
    for path in paths:
        if not path.endswith((".cpp", ".hpp")) or path.startswith("app/base/"):
            continue
        source = git("show", f"{REVISION}:{path}")
        for match in re.finditer(r"NewGClass<", source):
            if not re.search(r"\b(?:using|typedef)\b", source[max(0, match.start() - 45):match.start()]):
                continue
            name, strategy, implementation, *interfaces = arguments(source, match.end())
            method, parent, instance = parse_structs(strategy, source)
            runtime = re.search(r"(?:(?:extern\s+)?const|constexpr)\s+char\s+" + re.escape(name)
                                + r"\[\]\s*=\s*\"([^\"]+)\"", source)
            if not runtime:
                raise ValueError(f"no runtime GType name for {path}:{name}")
            expected = name.removesuffix("_name") + "_get_type" if name.endswith("_name") else ""
            getter = expected if expected and re.search(r"\b" + expected + r"\s*\(", source) else "through Class::Traits::get_type"
            line = source.count("\n", 0, match.start()) + 1
            rows.append((f"01.004/{len(rows)+1:03d}", "DONE", path, line,
                         runtime[1], name, method, parent, instance,
                         implementation, ", ".join(interfaces), getter))
    if len(rows) != 17:
        raise ValueError(f"expected 17 type declarations, got {len(rows)}")
    output = Path("migration/inventory/cpp-types.tsv")
    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("child_id", "status", "source", "line", "runtime_gtype",
                         "name_symbol", "struct_strategy", "c_parent", "c_instance",
                         "cpp_impl", "ginterface", "gtype_getter"))
        writer.writerows(rows)
    print(f"{len(rows)} NewGClass declarations mapped")


if __name__ == "__main__":
    main()
