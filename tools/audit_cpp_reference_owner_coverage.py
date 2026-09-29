#!/usr/bin/env python3
"""Require exact coverage of the legacy C++ reference and raw lifetime census."""

import csv
from pathlib import Path

ROOT = Path("migration/inventory")


def read(name):
    with (ROOT / name).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def unique_sites(rows, name):
    sites = [r["legacy_site"] for r in rows]
    assert len(sites) == len(set(sites)), f"duplicate in {name}"
    return set(sites)


def main():
    reference = read("cpp-reference-candidates.tsv")
    hold = read("cpp-hold-owner-contracts.tsv")
    explicit = read("cpp-explicit-ref-contracts.tsv")
    scoped = read("cpp-scoped-owner-contracts.tsv")
    values = read("cpp-gvalue-owner-contracts.tsv")
    raw = read("cpp-raw-owner-coverage.tsv")
    assert len(reference) == 242 and len(hold) == 24 and len(explicit) == 28
    assert len(scoped) == 54 and len(values) == 136 and len(raw) == 168
    inventories = (("HOLD", hold), ("REF_UNREF", explicit), ("CXX_POINTER", scoped), ("G_VALUE", values))
    for kind, rows in inventories:
        expected = {r["legacy_site"] for r in reference if r["kind"] == kind}
        actual = unique_sites(rows, kind)
        assert actual == expected, f"{kind}: missing={expected-actual} extra={actual-expected}"
        release_key = "release_or_risk" if kind == "REF_UNREF" else "release_contract"
        assert all(r.get("followup") and r.get(release_key) for r in rows), kind
    assert len(unique_sites(raw, "raw")) == 168
    assert all(r["coverage"] == "MAPPED" and r["owner"] and r["release_contract"] and r["followup"] for r in raw)
    print("242 reference candidate rows and 168 raw candidate rows have exact owner/follow-up coverage")


if __name__ == "__main__":
    main()
