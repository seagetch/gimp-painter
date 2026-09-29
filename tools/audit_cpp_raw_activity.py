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


def mask_literal_if_zero(source):
    """Mask literal #if 0 arms while preserving source line numbers."""
    frames = []
    active = True
    output = []
    for line in source.splitlines(keepends=True):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            name, arg = directive.groups()
            if name in ("if", "ifdef", "ifndef"):
                enabled = name != "if" or not re.fullmatch(r"0\s*(?://.*)?", arg.strip())
                frames.append([active, enabled, enabled])
                active = active and enabled
            elif name == "elif" and frames:
                parent, _, seen = frames[-1]
                enabled = not seen and not re.fullmatch(r"0\s*(?://.*)?", arg.strip())
                frames[-1][1:] = [enabled, seen or enabled]
                active = parent and enabled
            elif name == "else" and frames:
                parent, _, seen = frames[-1]
                frames[-1][1:] = [not seen, True]
                active = parent and not seen
            elif name == "endif" and frames:
                active = frames.pop()[0]
        output.append(line if active else re.sub(r"[^\n]", " ", line))
    return "".join(output)


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
        masked[path] = mask_comments_and_strings(mask_literal_if_zero(source)).splitlines()
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
