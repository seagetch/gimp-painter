#!/usr/bin/env python3
"""Verify every first-pass C/C++ boundary candidate has a DONE review row.

This checks coverage of the candidate inventories, not completeness of the
underlying pattern matcher or compatibility with GIMP 3 signatures.
"""

import csv
from collections import Counter
from pathlib import Path


ROOT = Path("migration/inventory")
CPP_TABLES = {
    "extern_c": ("extern-c-block-review.tsv", lambda row: f"{row['source']}:{row['opening_line']}", 69),
    "function_pointer": ("function-pointer-review.tsv", lambda row: row["legacy_site"], 34),
    "delegator_candidate": ("delegator-site-review.tsv", lambda row: row["legacy_site"], 107),
    "callback_registration": ("callback-registration-review.tsv", lambda row: row["legacy_site"], 94),
    "vfunc_candidate": ("vfunc-assignment-review.tsv", lambda row: row["assignment_site"], 63),
    "vfunc_binding_candidate": ("binder-site-review.tsv", lambda row: row["legacy_site"], 92),
    "c_entry_candidate": ("c-entry-candidate-review.tsv", lambda row: row["candidate_site"], 28),
    "c_definition_candidate": ("cpp-definition-review.tsv", lambda row: row["cpp_site"], 149),
    "signal_wrapper_candidate": ("signal-wrapper-review.tsv", lambda row: row["legacy_site"], 47),
}


def load(name):
    with (ROOT / name).open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file, delimiter="\t"))


def unique_sites(rows, field, title):
    sites = [row[field] for row in rows]
    assert len(sites) == len(set(sites)), f"duplicate {title} sites: {Counter(sites)}"
    return set(sites)


def main():
    candidates = load("cpp-call-boundary-candidates.tsv")
    assert len(candidates) == 683, f"candidate count changed: {len(candidates)}"
    assert all(row["status"] == "REVIEW" for row in candidates)
    assert set(row["kind"] for row in candidates) == set(CPP_TABLES)
    candidate_sites = unique_sites(
        [{"key": (row["kind"], row["source"], row["line"])} for row in candidates],
        "key", "C++ candidate")
    covered = set()
    for kind, (filename, key, expected_count) in CPP_TABLES.items():
        reviewed = load(filename)
        assert len(reviewed) == expected_count, (filename, len(reviewed))
        assert all(row["status"] == "DONE" for row in reviewed), filename
        reviewed_sites = [key(row) for row in reviewed]
        assert len(reviewed_sites) == len(set(reviewed_sites)), f"duplicate {filename} site"
        kinds = {(row["source"], row["line"])
                 for row in candidates if row["kind"] == kind}
        assert set(reviewed_sites) == {f"{source}:{line}" for source, line in kinds}, (
            kind, "missing", sorted({f"{source}:{line}" for source, line in kinds}
                                    - set(reviewed_sites)),
            "unexpected", sorted(set(reviewed_sites)
                                 - {f"{source}:{line}" for source, line in kinds}))
        covered.update((kind, *site.rsplit(":", 1)) for site in reviewed_sites)
        print(f"{kind}: {len(reviewed)} reviewed ({filename})")
    assert covered == candidate_sites

    references = load("c-call-reference-candidates.tsv")
    assert len(references) == 130
    review = load("c-reference-review.tsv")
    assert len(review) == 130 and all(row["status"] == "DONE" for row in review)
    source_sites = {(row["symbol"], row["source"], row["line"], row["kind"])
                    for row in references}
    review_sites = {(row["symbol"], *row["legacy_site"].rsplit(":", 1),
                     row["source_kind"]) for row in review}
    assert len(source_sites) == len(references) and len(review_sites) == len(review)
    assert review_sites == source_sites, "C/header reference coverage differs"

    direct = [row for row in review if row["reference_role"] == "DIRECT_CALL"]
    activity = load("c-direct-call-activity.tsv")
    assert len(direct) == len(activity) == 41
    assert all(row["status"] == "DONE" for row in activity)
    assert {(row["symbol"], row["legacy_site"]) for row in direct} == {
        (row["symbol"], row["c_site"]) for row in activity}
    active = {(row["symbol"], row["c_site"]) for row in activity
              if row["compile_state"] == "ACTIVE_CPP_ENTRY"}
    assert len(active) == 33, f"unexpected active C++ calls: {len(active)}"
    xcf = load("xcf-c-entry-review.tsv")
    other = load("non-xcf-c-call-review.tsv")
    assert len(xcf) == 9 and len(other) == 24
    assert all(row["status"] == "DONE" for row in xcf + other)
    assert {(row["symbol"], row["c_call_site"]) for row in xcf} | {
        (row["symbol"], row["c_site"]) for row in other} == active
    assert not ({(row["symbol"], row["c_call_site"]) for row in xcf} & {
        (row["symbol"], row["c_site"]) for row in other})

    pointer = [row for row in review
               if row["reference_role"] == "FUNCTION_POINTER_REGISTRATION"]
    pointer_review = load("c-pointer-registration-review.tsv")
    assert len(pointer) == len(pointer_review) == 6
    assert all(row["status"] == "DONE" for row in pointer_review)
    assert {(row["symbol"], row["legacy_site"]) for row in pointer} == {
        (row["symbol"], row["c_registration"]) for row in pointer_review}

    for filename, count in (("generated-gtype-review.tsv", 6),
                            ("property-accessor-review.tsv", 44),
                            ("deferred-source-review.tsv", 2)):
        rows = load(filename)
        assert len(rows) == count and all(row["status"] == "DONE" for row in rows)
    print("C/header references: 130 classified; 33 active C++ direct calls "
          "traced (9 XCF, 24 other); 6 pointer registrations traced")
    print("Additional checks: 6 macro-generated GTypes, 44 property sites, "
          "2 deferred sources")
    print("First-pass boundary candidate coverage: OK")


if __name__ == "__main__":
    main()
