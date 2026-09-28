#!/usr/bin/env python3
"""Classify active and preprocessor-disabled legacy C direct call sites."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from audit_cpp_definitions import disabled_lines
from inventory_call_boundaries import REVISION, without_comments


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-direct-call-activity.tsv"


def guard_site(lines, line):
    """Give the controlling literal #if branch for a disabled candidate."""
    stack = []
    for number, value in enumerate(lines[:line], 1):
        match = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", value)
        if not match:
            continue
        directive, rest = match.groups()
        if directive in ("if", "ifdef", "ifndef"):
            condition = rest.strip() if directive == "if" else "unknown"
            stack.append((condition, condition == "0", number))
        elif directive == "else" and stack:
            condition, _, _ = stack[-1]
            stack[-1] = (condition, condition == "1", number)
        elif directive == "endif" and stack:
            stack.pop()
    disabled = [number for _, inactive, number in stack if inactive]
    return disabled[-1] if disabled else None


def call_expression(code, offset, symbol):
    match = re.search(r"\b" + re.escape(symbol) + r"\s*\(", code[offset:])
    assert match, (symbol, offset)
    begin = offset + match.start()
    depth = 0
    quote = None
    escape = False
    for position in range(begin, len(code)):
        char = code[position]
        if quote:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = None
            continue
        if char in ("'", '"'):
            quote = char
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return " ".join(code[begin:position + 1].split())
    raise ValueError((symbol, offset))


def main():
    with (ROOT / "c-reference-review.tsv").open(newline="", encoding="utf-8") as file:
        candidates = [r for r in csv.DictReader(file, delimiter="\t")
                      if r["source_kind"] == "c_source"
                      and r["reference_role"] == "DIRECT_CALL"]
    assert len(candidates) == 41
    cache = {}
    results = []
    for row in candidates:
        path, number = row["legacy_site"].rsplit(":", 1)
        line = int(number)
        if path not in cache:
            raw = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"],
                text=True, encoding="utf-8")
            cache[path] = (raw, raw.splitlines(), disabled_lines(raw.splitlines()))
        raw, lines, inactive = cache[path]
        assert row["legacy_code"] == lines[line - 1].strip()
        code = without_comments(raw)
        offset = sum(len(s) for s in raw.splitlines(keepends=True)[:line - 1])
        call = call_expression(code, offset, row["symbol"])
        if line in inactive:
            classification = "DISABLED_BY_PREPROCESSOR"
            guard = guard_site(lines, line)
            assert guard is not None, row
            guard = f"{path}:{guard}"
        else:
            classification = ("ACTIVE_C_IMPLEMENTATION" if row["target_kind"] ==
                              "C_IMPLEMENTATION" else "ACTIVE_CPP_ENTRY")
            guard = "-"
        results.append((row["symbol"], row["legacy_site"], row["target_kind"],
                        classification, guard, call, "DONE"))
    counts = Counter(r[3] for r in results)
    assert counts == {"ACTIVE_CPP_ENTRY": 33, "ACTIVE_C_IMPLEMENTATION": 2,
                      "DISABLED_BY_PREPROCESSOR": 6}, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "c_site", "target_kind", "compile_state",
                         "disabled_guard", "call_expression", "status"))
        writer.writerows(results)
    print(f"{len(results)} direct-call candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
