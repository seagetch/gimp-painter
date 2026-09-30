#!/usr/bin/env python3
"""Parse the exact arguments of newly added legacy C signal connections."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

from audit_cpp_object_data_sites import split_args
from inventory_call_boundaries import REVISION

ROOT = Path("migration/inventory")
OUTPUT = ROOT / "c-added-signal-contracts.tsv"


def read(name):
    with (ROOT / name).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def main():
    rows = read("c-signal-added-line-review.tsv")
    assert len(rows) == 57 and all(r["line_origin"] == "ADDED" for r in rows)
    cache = {}
    result = []
    teardown = {r["path_basename"]: r for r in read("signal-teardown-by-file.tsv")}
    for row in rows:
        site, op = row["legacy_site"], row["operation"]
        path, number = site.rsplit(":", 1)
        if path not in cache:
            cache[path] = subprocess.check_output(["git", "show", f"{REVISION}:{path}"], text=True, errors="replace")
        source = cache[path]
        start = sum(len(line) for line in source.splitlines(keepends=True)[:int(number) - 1])
        match = re.search(r"\b" + re.escape(op) + r"\s*\(", source[start:])
        assert match and source[:start + match.start()].count("\n") + 1 == int(number), site
        args = split_args(source, start + match.end())
        assert len(args) == (5 if op == "g_signal_connect_object" else 4), (site, op, args)
        receiver, signal, callback, data = args[:4]
        if op == "g_signal_connect_object":
            release = "GObject watched callback data; disconnect when receiver or data object finalizes"
            data = args[3]
        elif op == "g_signal_connect_closure":
            release = "closure owned by emitter; verify closure notifier and callback user data"
            data = "closure=" + args[2]
        else:
            release = "emitter finalization or explicit handler disconnect; verify data owner"
        result.append((site, op, receiver, signal, callback, data, release,
                       "synchronous signal emission", "Synchronous emission: callback may switch/destroy owner; guard executing state", "TRACED_STATIC",
                       teardown[Path(path).name]["teardown_evidence"], teardown[Path(path).name]["followup"]))
    assert len(result) == len({r[0] for r in result}) == 57
    with OUTPUT.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "receiver", "signal", "callback", "user_data",
                         "release_contract", "priority", "reentrancy", "status", "teardown_evidence", "followup"))
        writer.writerows(result)
    print(f"{len(result)} added C signal argument contracts parsed: {dict(Counter(r[1] for r in result))}")


if __name__ == "__main__":
    main()
