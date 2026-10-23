# Original 05.014 acceptance

The executable contract is in
[legacy-adapter-removal.md](../../contracts/legacy-adapter-removal.md).
Run `python3 -B tools/check_painter_legacy_adapters.py` and
`python3 -B tools/tests/test_painter_legacy_adapters.py`.

The current source scan covers 2,457 non-test application sources/headers and
generation inputs, including all 78 registered C++ units and optional HTTP code.
It reports no unapproved active legacy syntax or data site. The one approved
syntax finding is the exact modern `Clone ref(...)` declaration. Four unchanged
upstream legacy pixel uses remain under literal `#if 0`, explicitly reported and
pinned; re-enabling them is rejected.

All 681 object-data operations are accounted for: 675 exact upstream matches
against GIMP 3.0.9 commit 95f6410f25c5186686db7a489d79c1e79187cd41 and six narrow
native/common-store exceptions. Mutation-only independent comparison agrees:
334 of 337 match upstream; the other three are common-store publication, native
GFile retention, and native FilterData-list transfer. Borrowed native metadata
and the common-store lookup account for the other three exceptions. Matching
preserves source path, complete normalized arguments/literals and multiplicity.
It does not certify unchanged surrounding semantics.

The 39 Python tests include positive controls and deliberate regressions for
old wrapper/private-placement/pixel names, nested/parenthesized ref acquisition,
aliases, declaration-versus-call exceptions, quoted/raw literals and comments,
line splicing, token pasting, macro headers, unknown configuration branches,
multiline preprocessor comments, dormant re-enabling, changed or duplicate
native sites, invalid provenance, missing/empty scope, new file suffixes and
source symlinks. Review identified four direct false-pass cases during development;
their exact variants are now rejected by the final tests. No production
application code changed in this task, so no new GIMP runtime pass is claimed.

`report.json` seals the final scanner, policy, tests and source-scan archive.
The archive contains the actual current scan, test output, pinned upstream
comparison and reproducible comparison script. The baseline source blobs remain
addressable at the recorded upstream commit; routine scanning does not fetch
them or require a network connection.

Original 05.014 defines a checkable removal condition. It has no directly assigned
source-work ledger row, and this checkpoint does not change the existing
implementation TODOs in the handle/type/call inventories. Runtime replacement
proof and 31.014–31.017 remain open. The scanner's documented lexical limits are
not replaced with a claim of complete C++ semantic or arbitrary macro analysis.
