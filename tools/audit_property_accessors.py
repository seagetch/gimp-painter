#!/usr/bin/env python3
"""Find legacy NewGClass property accessor bindings, including disabled code."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


OUTPUT = Path("migration/inventory/property-accessor-review.tsv")
ACCESSOR = re.compile(r"\b(?P<macro>_getter|_setter)\s*\(\s*(?P<property>\w+)\s*\)")
TARGET = re.compile(r"\.bind\s*<\s*&\s*(?P<impl>\w+)\s*::\s*"
                    r"(?P<method>get_|set_)##method\s*>\s*\(")


def disabled_lines(source: list[str]) -> set[int]:
    """Track literal #if 0 branches; other conditional expressions are unresolved."""
    stack: list[bool] = []
    result = set()
    for line, code in enumerate(source, 1):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", code)
        if directive:
            kind, condition = directive.groups()
            if kind in ("if", "ifdef", "ifndef"):
                stack.append(kind == "if" and condition.strip() == "0")
            elif kind == "else" and stack:
                # Flip the literal #if 0 branch; unrelated conditions remain unresolved.
                if any(stack):
                    stack[-1] = not stack[-1]
            elif kind == "endif" and stack:
                stack.pop()
        if any(stack):
            result.add(line)
    return result


def main() -> None:
    paths = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", REVISION, "app"],
        text=True, encoding="utf-8"
    ).splitlines()
    rows = []
    for path in paths:
        if not path.endswith((".cpp", ".hpp")):
            continue
        source = subprocess.check_output(
            ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
        ).splitlines()
        inactive = disabled_lines(source)
        definitions: dict[str, tuple[int, str]] = {}
        for line, code in enumerate(source, 1):
            matches = list(ACCESSOR.finditer(code))
            if not matches:
                continue
            is_definition = code.lstrip().startswith("#define")
            if is_definition:
                assert len(matches) == 1, (path, line)
                target = TARGET.search(code)
                assert target, (path, line, code)
                match = matches[0]
                assert target["method"] == ("get_" if match["macro"] == "_getter" else "set_")
                definitions[match["macro"]] = (line, target["impl"])
                rows.append((f"{path}:{line}", match["macro"], "-", "MACRO_DEFINITION",
                             f"{path}:{line}", target["impl"], "-", "-", "DONE"))
                continue
            for match in matches:
                macro, property_name = match.group("macro", "property")
                assert macro in definitions, (path, line, macro)
                macro_line, impl = definitions[macro]
                method = ("get_" if macro == "_getter" else "set_") + property_name
                role = "DISABLED_BY_IF_0" if line in inactive else "ACTIVE_BINDING"
                sites = [f"{path}:{i}" for i, text in enumerate(source, 1)
                         if re.search(r"\b" + re.escape(method) + r"\s*\(", text)]
                if role == "ACTIVE_BINDING":
                    assert sites, (path, line, method)
                rows.append((f"{path}:{line}", macro, property_name, role,
                             f"{path}:{macro_line}", impl, method,
                             "; ".join(sites) or "-", "DONE"))

    counts = Counter(row[3] for row in rows)
    assert len(rows) == 44 and counts == {
        "MACRO_DEFINITION": 18, "ACTIVE_BINDING": 18,
        "DISABLED_BY_IF_0": 8,
    }, counts
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "macro", "property_token", "role",
                         "macro_definition", "impl_class", "cpp_method",
                         "method_sites", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} property accessor occurrences classified: {dict(counts)}")


if __name__ == "__main__":
    main()
