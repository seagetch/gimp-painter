#!/usr/bin/env python3
"""Distinguish active C++ allocation syntax from comments and strings."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION

INPUT = Path("migration/inventory/cpp-raw-lifetime-candidates.tsv")
OUTPUT = Path("migration/inventory/cpp-raw-lifetime-review.tsv")
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def mask_comments_and_strings(source):
    source = re.sub(r"/\*.*?\*/", lambda match: re.sub(r"[^\n]", " ", match[0]),
                    source, flags=re.S)
    source = re.sub(r"//[^\n]*", lambda match: " " * len(match[0]), source)
    return TOKEN.sub(lambda match: " " * len(match[0]), source)


def classify(kind, text):
    if kind == "NEW":
        return "ARRAY_ALLOCATION" if re.search(r"\bnew\s+\w+\s*\[", text) else "CXX_ALLOCATION"
    if kind == "DELETE":
        return "CXX_RELEASE"
    if kind == "G_FREE":
        return "C_ALLOCATOR_RELEASE"
    return "WEAK_POINTER_REGISTRATION"


def main():
    with INPUT.open(encoding="utf-8", newline="") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    paths = {row["legacy_site"].rsplit(":", 1)[0] for row in candidates}
    masked = {}
    for path in paths:
        source = subprocess.check_output(["git", "show", f"{REVISION}:{path}"],
                                         text=True, errors="replace")
        masked[path] = mask_comments_and_strings(source).splitlines()
    rows = []
    for row in candidates:
        path, number = row["legacy_site"].rsplit(":", 1)
        line = masked[path][int(number) - 1]
        kind = row["kind"]
        token = {"NEW": r"\bnew\s+\w+", "DELETE": r"\bdelete\s+\w+",
                 "G_FREE": r"\bg_free\s*\(",
                 "WEAK_POINTER": r"\bg_object_add_weak_pointer\s*\("}[kind]
        role = classify(kind, line) if re.search(token, line) else "INACTIVE_OR_TEXT"
        rows.append((row["legacy_site"], kind, role, row["source_line"], "DONE"))
    counts = Counter(row[2] for row in rows)
    assert len(rows) == 168
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "kind", "syntax_role", "source_line", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} raw C++ candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
