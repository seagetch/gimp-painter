#!/usr/bin/env python3
"""Map legacy CXXPointer types to their delete and callback lifetime contracts."""

import csv
import re
from collections import Counter
from pathlib import Path

INPUT = Path("migration/inventory/cpp-reference-candidates.tsv")
OUTPUT = Path("migration/inventory/cpp-scoped-pointer-review.tsv")


def role(line):
    if "CXXPointer<" not in line:
        raise AssertionError(line)
    if "guard(T* ptr)" in line:
        return "HELPER_DEFINITION", "delete T through CXXPointer on scope exit"
    if "Connection" in line:
        return "SIGNAL_CONNECTION", "delete Connection; destructor disconnects target signal"
    if re.search(r"\b(?:Idle|Timeout)\s*>", line):
        return "SOURCE_OWNER", "delete source guard; verify source cancellation"
    if "Delegator<" in line or "<Delegator>" in line or "<rule_delegator>" in line:
        return "DELEGATOR_OWNER", "delete delegated callback; verify closure lifetime"
    return "CXX_INSTANCE", "delete T on replacement or scope exit; verify allocation and aliases"


def main():
    with INPUT.open(encoding="utf-8", newline="") as file:
        candidates = [row for row in csv.DictReader(file, delimiter="\t")
                      if row["kind"] == "CXX_POINTER"]
    rows = []
    for candidate in candidates:
        kind, destruction = role(candidate["source_line"])
        rows.append((candidate["legacy_site"], kind, candidate["source_line"],
                     destruction, "DONE"))
    counts = Counter(row[1] for row in rows)
    assert len(rows) == 54, len(rows)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "owner_role", "source_line", "destruction_contract",
                         "status"))
        writer.writerows(rows)
    print(f"{len(rows)} CXXPointer sites: {dict(counts)}")


if __name__ == "__main__":
    main()
