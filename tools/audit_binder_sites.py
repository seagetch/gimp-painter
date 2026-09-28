#!/usr/bin/env python3
"""Expand the named parts of legacy NewGClass vfunc binder macros."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "binder-site-review.tsv"
DEFINE = re.compile(r"^#define\s+(?P<macro>\w+)\s*\(")
MACRO_WRAPPER = re.compile(r"\b(?P<wrapper>\w+)\s*::\s*__\s*\(")
MACRO_TARGET = re.compile(r"\.bind\s*<\s*&\s*(?P<impl>\w+)\s*::\s*method\s*>\s*\(")
OVERRIDE = re.compile(r"^_override\s*\(\s*(?P<slot>\w+)\s*\)\s*;$")
BIND = re.compile(r"^bind_to_class\s*\(\s*(?P<receiver>\w+)\s*,\s*"
                  r"(?P<slot>\w+)\s*,\s*(?P<impl>[A-Za-z_]\w*(?:::\w+)*)\s*\)\s*;$")


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    sites = [row for row in candidates if row["kind"] == "vfunc_binding_candidate"]
    cache: dict[str, list[str]] = {}
    definitions: dict[tuple[str, str], list[tuple[int, str, str]]] = {}
    for row in sites:
        path = row["source"]
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        line = int(row["line"])
        code = cache[path][line - 1].strip()
        assert code == row["legacy_code"], row
        definition = DEFINE.match(code)
        if definition:
            wrapper = MACRO_WRAPPER.search(code)
            assert wrapper, row
            target = MACRO_TARGET.search(code)
            definitions.setdefault((path, definition["macro"]), []).append(
                (line, wrapper["wrapper"], target["impl"] if target else "-"))

    results = []
    for row in sites:
        path, line, code = row["source"], int(row["line"]), row["legacy_code"]
        definition = DEFINE.match(code)
        if definition:
            kind = "MACRO_DEFINITION"
            macro = definition["macro"]
            receiver = slot = impl = "-"
            wrapper = MACRO_WRAPPER.search(code)["wrapper"]
            macro_site = f"{path}:{line}"
            resolved_impl = alias_site = "-"
        else:
            override = OVERRIDE.fullmatch(code)
            binding = BIND.fullmatch(code)
            assert override or binding, row
            macro = "_override" if override else "bind_to_class"
            preceding = [d for d in definitions[(path, macro)] if d[0] < line]
            assert preceding, row
            macro_line, wrapper, macro_impl = preceding[-1]
            macro_site = f"{path}:{macro_line}"
            kind = "VFUNC_BINDING"
            slot = (override or binding)["slot"]
            receiver = "klass" if override else binding["receiver"]
            impl = macro_impl if override else binding["impl"]
            assert impl != "-", row
            resolved_impl = impl
            alias_site = "-"
            if impl == "Impl":
                aliases = []
                for index, source_line in enumerate(cache[path][:line - 1], 1):
                    typedef = re.search(r"\btypedef\s+(\w+(?:::\w+)*)\s+Impl\s*;", source_line)
                    using = re.search(r"\busing\s+Impl\s*=\s*(\w+(?:::\w+)*)\s*;", source_line)
                    if typedef or using:
                        aliases.append((index, (typedef or using)[1]))
                assert aliases, row
                alias_line, resolved_impl = aliases[-1]
                alias_site = f"{path}:{alias_line}"
        results.append((f"{path}:{line}", kind, macro, macro_site, wrapper,
                        receiver, slot, impl, resolved_impl, alias_site, "DONE"))

    counts = Counter(row[1] for row in results)
    assert len(results) == 92 and counts == {"MACRO_DEFINITION": 21,
                                       "VFUNC_BINDING": 71}, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "role", "macro", "macro_definition",
                         "bridge_type_alias", "class_pointer", "slot", "impl_alias",
                         "resolved_impl", "impl_alias_site", "status"))
        writer.writerows(results)
    print(f"{len(results)} legacy binder sites classified: {dict(counts)}")


if __name__ == "__main__":
    main()
