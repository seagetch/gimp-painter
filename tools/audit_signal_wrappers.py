#!/usr/bin/env python3
"""Map legacy C++ signal wrapper connections and callback ownership."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from inventory_call_boundaries import REVISION


ROOT = Path("migration/inventory")
OUTPUT = ROOT / "signal-wrapper-review.tsv"
CONNECT = re.compile(r"\.(?P<method>connect(?:_noret)?)\s*\(\s*\"(?P<signal>[^\"]+)\"")
MEMBER = re.compile(r"(?:_D::)?delegator\s*\(\s*this\s*,\s*&\s*"
                    r"(?P<owner>[A-Za-z_]\w*(?:::\w+)*)::(?P<target>\w+)")


def main() -> None:
    with (ROOT / "cpp-call-boundary-candidates.tsv").open(newline="", encoding="utf-8") as file:
        candidates = list(csv.DictReader(file, delimiter="\t"))
    connections = [row for row in candidates if row["kind"] == "signal_wrapper_candidate"]
    cache: dict[str, list[str]] = {}
    rows = []
    for row in connections:
        path, line = row["source"], int(row["line"])
        if path not in cache:
            cache[path] = subprocess.check_output(
                ["git", "show", f"{REVISION}:{path}"], text=True, encoding="utf-8"
            ).splitlines()
        code = cache[path][line - 1]
        assert code.strip() == row["legacy_code"], row
        match = CONNECT.search(code)
        assert match, row
        prefix = code[:match.start()].strip()
        if "=" in prefix:
            field, receiver = (part.strip() for part in prefix.split("=", 1))
            assert match["method"] == "connect" and field, row
            role = "STORED_CONNECTION"
        else:
            field, receiver = "-", prefix
            assert match["method"] == "connect_noret", row
            role = "UNTRACKED_CONNECTION"
        member = MEMBER.search(code)
        if member:
            target = f"{member['owner']}::{member['target']}"
            kind = "CXX_MEMBER"
            assert any(re.search(r"\b" + re.escape(member["target"]) + r"\s*\(", text)
                       for text in cache[path]), row
        else:
            assert "std::function" in code and re.search(r"\[this(?:,|\])", code), row
            target = f"lambda@{path}:{line}"
            kind = "LAMBDA_CAPTURE_THIS"
        port_check = ("Retain and disconnect the connection before owner destruction"
                      if role == "STORED_CONNECTION" else
                      "No connection handle returned; ensure target lifetime and explicit teardown")
        rows.append((f"{path}:{line}", match["method"], receiver, match["signal"],
                     field, role, kind, target, port_check, "DONE"))

    counts = Counter((row[5], row[6]) for row in rows)
    assert len(rows) == 47 and counts == {
        ("STORED_CONNECTION", "CXX_MEMBER"): 22,
        ("UNTRACKED_CONNECTION", "CXX_MEMBER"): 10,
        ("UNTRACKED_CONNECTION", "LAMBDA_CAPTURE_THIS"): 15,
    }, counts
    with OUTPUT.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "method", "receiver", "signal", "handler_storage",
                         "role", "target_kind", "target", "port_check", "status"))
        writer.writerows(rows)
    print(f"{len(rows)} legacy signal wrapper connections classified: {dict(counts)}")


if __name__ == "__main__":
    main()
