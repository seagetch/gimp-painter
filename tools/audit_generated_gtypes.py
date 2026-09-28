#!/usr/bin/env python3
"""Find legacy GType C entries emitted by macros in C++ translation units."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION, without_comments


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "generated-gtype-review.tsv"


def git(*args):
    return subprocess.check_output(["git", *args], text=True, encoding="utf-8")


def main():
    candidates = list(csv.DictReader((ROOT / "cpp-call-boundary-candidates.tsv").open(
        newline="", encoding="utf-8"), delimiter="\t"))
    explicit = {re.search(r"\b(gimp_\w+)\s*\(", r["legacy_code"])[1]
                for r in candidates if r["kind"] == "c_definition_candidate"}
    paths = [p for p in git("ls-tree", "-r", "--name-only", REVISION, "app").splitlines()
             if p.endswith(".cpp")]
    pattern = re.compile(r"\b(G_DEFINE_TYPE(?:_WITH_CODE)?)\s*\(\s*"
                         r"(\w+)\s*,\s*(gimp_\w+)\s*,", re.S)
    found = []
    for path in paths:
        clean = without_comments(git("show", f"{REVISION}:{path}"))
        for match in pattern.finditer(clean):
            name = match[3] + "_get_type"
            assert name not in explicit, (path, name)
            found.append((name, path, clean.count("\n", 0, match.start()) + 1,
                          match[1], match[2]))

    assert len(found) == 6, found
    results = []
    for name, path, line, macro, instance in found:
        references = git("grep", "-n", "-F", "-e", name, REVISION, "--", "app")
        declarations = []
        for value in references.splitlines():
            _, header, number, code = value.split(":", 3)
            if header.endswith(".h") and re.search(
                    r"\bGType\s+" + re.escape(name) + r"\s*\(\s*void\s*\)", code):
                declarations.append(f"{header}:{number}")
        assert len(declarations) == 1, (name, declarations)
        results.append((name, f"{path}:{line}", declarations[0], macro,
                        instance, "MACRO_GENERATED_C_ENTRY", "DONE"))

    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "macro_site", "header_site", "macro",
                         "instance_type", "classification", "status"))
        writer.writerows(results)
    print(f"{len(results)} macro-generated legacy GType entries paired with C headers")


if __name__ == "__main__":
    main()
