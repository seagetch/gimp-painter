# Native Perspective Undo, non-persistence, and image lifecycle

Bounded progress on original26.010–012;45 inherited whole-hunk duties and original
criteria/dependencies stay open. No new DONE child, production fix, old-runtime
or physical-device claim. A single extra standard target reuses the frozen
editor harness but registers only three lifecycle cases. Original editor13 and
stroke2 targets are rerun separately; all retain60-second budgets.

1. Actual synthetic DisplayShell add/move/middle-delete/last-delete edits restore
   through native Undo/Redo, preserve ordering and clear stale snap state. One
   change creates one Undo entry; max3 no-op creates none. XCF/export clean flags
   remain unchanged. Export cleanliness uses explicit known-export metadata
   setup; it does not claim that a PNG export was performed.
2. Actual memory-stream XCF Save/Open omits the guide, preserving image dimensions
   and layer count; live3-point model identity/coordinates remain unchanged after
   Save/Open. A controlled second test temporarily removes/restores the guide:
   first/control/without-guide writer streams must be BYTE-IDENTICAL. No field
   masks or tolerance. Image duplication then omits the guide as old source does.
3. Actual active-edit image switch ignores stale motion/release and old model
   signals. The new image's model/extents stay unchanged. Closing an active editor
   clears control/draw/display/status references. Only committed held providers
   are used; no held diagnostic or candidate is imported.

The complete pinned old XCF tree and image duplicate have no guide references;
image private/model/tool sources give ownership context.21 source blobs and the
entire old XCF tree hash are independently verified. This is SOURCE evidence,
not an old Save/Open runtime recording. Current146 scoped inputs are sealed
before build with ELF hashes for all three targets; all9 held providers match
committed private baselines. Ordinary ASan/UBSan covers app units, not external
dependencies; leak detection is disabled.

Preserved diagnostics:
* Cleaning XCF does not clean the separate export counter. Clearing export on an
  unnamed never-exported image triggers the native invalid-context warning.
  The test now establishes a valid explicit exported-file metadata context.
* An initial writer comparison incorrectly duplicated the image between saves.
  Exactly one byte differed: image PROP_TATTOO(20), value2→3, offset66. The captured
  original streams and tested source are retained. Native tattoo counter output
  confirms duplication advances2→3; ordinary item allocation consumes tattoos.
  Moving that unrelated operation AFTER the comparison yields exact identical
  first/control/without-guide streams. No production defect or skipped byte.

Three causal test mutations omit Undo, add a persistent guide-named parasite,
or omit image switch. Each must fail the appropriate native invariant before
exact restoration and the final full normal/sanitizer rerun.

Authenticated portrait Xvfb is1080x1920x24. Actual1040x1800 oncanvas screenshots
show three points before deletion, two after deletion, and three restored by
Undo. They are current app images from synthetic pointer editing, not old runtime
or physical pen results. Prerequisite and broad platform/device tasks stay open.

Final exact-restoration results: normal13+2+3 PASS (8.97s/3.08s/6.10s), ordinary
sanitizer13+2+3 PASS (18.75s/4.69s/7.23s), zero skipped cases/timeouts. Three causal
controls fail as required; eighteen evidence-mutation controls pass. Native
production files are unchanged. Check `check-lifecycle-entry.py`,
`test-lifecycle-evidence.py`, and the pinned old fixture's `verify-source.py`.

## Explicit source successor (2026-10-21)

The smoothing checkpoint reuses this target with one additive case/include and a
non-threaded painting test setting. All three lifecycle case bodies and this
checkpoint's original runtime archive/receipts remain unchanged. The checker
admits only the exact additive transformation in
`../perspective-smoothing-entry/admit-lifecycle-successor.py`; new runtime claims
belong to that successor, not retroactively to this receipt.
