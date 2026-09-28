#!/usr/bin/env python3
"""Classify legacy delegator creation sites and recorded C++ targets."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "delegator-site-review.tsv"
FACTORIES = {
    "app/base/delegators.hpp:92", "app/base/delegators.hpp:99",
    "app/base/delegators.hpp:106", "app/base/glib-cxx-utils.hpp:1162",
}
MEMBER = re.compile(r"(?:\b(?:Delegators|_D|RESTD)::)?delegator\s*\(\s*"
                    r"(?P<owner>this|[A-Za-z_]\w*)\s*,\s*&\s*"
                    r"(?P<class>[A-Za-z_]\w*(?:::\w+)*)::(?P<method>\w+)")


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    sites = [row for row in candidates if row["kind"] == "delegator_candidate"]
    cache: dict[str, list[str]] = {}
    rows = []
    for row in sites:
        path, line = row["source"], int(row["line"])
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        code = cache[path][line - 1]
        assert code.strip() == row["legacy_code"], row
        site = f"{path}:{line}"
        match = MEMBER.search(code)
        if site in FACTORIES:
            role, owner, target, capture = "ADAPTER_FACTORY", "-", "-", "-"
        elif match:
            role = "MEMBER_DELEGATOR"
            owner, target, capture = match["owner"], f"{match['class']}::{match['method']}", "-"
        elif "std::function" in code or "RESTD::delegator([&]" in code:
            role = "LAMBDA_DELEGATOR"
            owner, target = "-", f"lambda@{site}"
            captured = re.search(r"\[(?P<capture>[^]]*)\]", row["boundary_statement"])
            assert captured, row
            capture = captured["capture"]
        elif path == "app/base/glib-cxx-utils.hpp" and line in (1044, 1065, 1109, 1115):
            role, owner, target, capture = "GENERIC_FORWARD", "-", "generic callback parameter", "-"
        elif path == "app/widgets/gimplayertileview.cpp" and line in (790, 1299):
            role, owner, target, capture = "LOCAL_FUNCTION", "-", "local function variable", "-"
        else:
            raise ValueError(f"Unclassified legacy delegator: {row}")
        if role == "LAMBDA_DELEGATOR":
            port_check = "Verify captured values and asynchronous callback lifetime"
        elif role == "MEMBER_DELEGATOR":
            port_check = "Verify owner lifetime and disconnection before method callback"
        else:
            port_check = "Verify delegator release and callback invocation contract"
        rows.append((site, role, owner, target, capture, port_check, "DONE"))

    counts = Counter(row[1] for row in rows)
    assert len(rows) == 107 and counts == {
        "ADAPTER_FACTORY": 4, "MEMBER_DELEGATOR": 90,
        "LAMBDA_DELEGATOR": 7, "GENERIC_FORWARD": 4,
        "LOCAL_FUNCTION": 2,
    }, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "role", "bound_owner_if_visible", "cpp_target",
                         "lambda_capture_if_visible", "port_check", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} delegator candidates classified: {dict(counts)}")


if __name__ == "__main__":
    main()
