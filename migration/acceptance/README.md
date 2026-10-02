# Implementation acceptance, separate from source inventory

The original inventory and 90-type/76-entry contract registry describe what must
be migrated. Their historical TODO fields are intentionally not rewritten into
runtime claims. These acceptance files connect a WBS obligation to actual current
implementation sources, named passed tests and stored normal/sanitizer evidence.

`foundation.json` covers all 50 rows of sections06 and07. It distinguishes:

- COMPONENT_VERIFIED: the stated common component scope is implemented and has
  exact source-hash and named normal/sanitizer test evidence
- PARTIAL: some implementation exists, but an explicitly listed call-site or
  feature-wide obligation remains
- OPEN: the required evidence does not exist

This is not a replacement completion counter. Parent WBS dependencies stay intact;
component evidence cannot turn a Linux test into a Windows/macOS result, close an
unreviewed GTK replacement or certify every legacy callback. No parent box is
marked done merely because a related representative feature test passes.

The first matrix has46 verified component obligations,3 partial obligations
(full GTK DSL replacement, operation-level lookup audit, and all untracked signal
call sites), and the unrun cross-platform linking gate. The34-case common suite
adds real base/derived GTypes with two distinct slots of one Impl type, inherited
and own properties, a separately registered GInterface, C-to-C++ error returns,
and reentrant parent dispose. These are genuine additional missing checks, not
inferences from the previous single-class fixture.

The standalone runner preserves C and C++ compilation separately, even when both
files share a basename. It records only compiled sources and used common/test
headers, not every unrelated file later added under app/painter. The original
31-case reports stay unchanged. ASan/UBSan does not include LeakSanitizer here.

Run `python tools/check_painter_acceptance.py` to require complete WBS coverage,
unchanged dependency snapshots, existing sources, named successful tests and exact
source hashes. `python tools/test_painter_acceptance.py` checks missing/duplicate
rows, invented test names, unrelated source claims and removed dependencies.
Later work extends the matrix to the90 legacy handle contracts,76 C entries and
remaining feature families. Unresolved mappings must remain visible rather than
being dropped or filled with generic success language.

`state-attachment-gaps.json` preserves the 17 historical free-form Painter
layer-dialog/XCF attachment sites under31.016. Their replacements now have typed
common-store ownership, native lifecycle/copy tests and focused sanitizer proof.
The parent architecture gate and all-feature acceptance remain open. Native
last-file context keys remain a separate upstream protocol; this replacement
does not discard provenance, unknown payloads or saved references.

`clone.json` covers all26 WBS14 rows:21 have verified component evidence and5
retain explicit fixture or integration gaps. It joins the37-case Clone core
report, the earlier42-case provenance/XCF checkpoint and21-case dialog checkpoint.
The latter two are **historical compiled snapshots**, not tests of the currently
evolving combined XCF/GUI tree. Their sealed source archives make that distinction
checkable. The Clone core implementation/test hashes still match the measured
source. `check_clone_acceptance.py` checks source/report seals, complete WBS
coverage, unchanged dependency/acceptance text, and named normal/sanitizer tests;
`test_clone_acceptance.py` has8 positive/negative checks. This is evidence
reconciliation, not a newly executed application run. The remaining component
visibility, positive same-name persistence checkpoint, indexed/compound and
non-RGB cross-image fixtures stay explicit, along with aggregate/platform gates.

The first hierarchy build exposed a standalone runner basename collision:
`test-hierarchy.c` and `test-hierarchy.cpp` overwrote the same object output.
The runner now includes each suffix in its object name. A subsequent test-only
macro comma syntax error was also corrected; both failed build outputs remain
stored separately from successful runtime reports.
