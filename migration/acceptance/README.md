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

`state-attachment-gaps.json` additionally preserves the known remaining free-form
Painter layer-dialog/XCF attachment sites under31.016. This is an unresolved
architecture obligation, not a signal to drop provenance, unknown payloads or
saved references. Native last-file context keys remain a separate upstream
protocol. The scoped scan does not pretend to cover every uncommitted feature.

The first hierarchy build exposed a standalone runner basename collision:
`test-hierarchy.c` and `test-hierarchy.cpp` overwrote the same object output.
The runner now includes each suffix in its object name. A subsequent test-only
macro comma syntax error was also corrected; both failed build outputs remain
stored separately from successful runtime reports.
