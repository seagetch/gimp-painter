# Independent real old RGB-u8 paint sessions

Pinned behavior source: `afa43fae3e920210146abed514f136fd49f671b5`.
The old capture links the existing old application archives and calls actual
`GimpMypaintCore`, native drawable storage and Undo/Redo. No port evaluator or
Surface supplies the reference. Its behavior-source hashes and linked archive
hashes are in `old-capture-report.json`; no archive changed during capture.

The 96 explicitly synthetic scenes cross five bits (nonincremental, ordinary
bitmap shape, paper, offset selection, smudge) with three states (normal, eraser
0.65, actual layer alpha lock). Image dimensions are 150x110 and drawable
dimensions 130x96, offset 7,3. The RGB initial image uses deterministic patterns;
the source contains the exact coordinates, stationary pressure change, times,
foreground/background values and settings. Three full RGB snapshots per scene
record finish, actual Undo and actual Redo. The corresponding Undo records
assert restoration and depth, not just a painted image hash.

The 16 nonincremental + full layer alpha-lock scenes (65,67,...,95) are visibly
unchanged in the old reference, although the real old transaction still creates
one Undo entry. Its initially transparent floating buffer cannot gain alpha in
that mode. The fixture verifies this observed no-op instead of assuming that
every active stroke changes visible pixels. The port's existing finite
zero-alpha lock treatment preserves these pixels without reproducing the old
undefined NaN-to-byte conversion.

There are 385 records, 21,575,701 bytes after decompression, with SHA256
`64cdb3add5ad92af01fe93f079a0378b4dc7a7caefe56406fd29c67747ee00d7`.
`session-values.tsv.gz` retains every complete record once. The old and private
new runtime log files retain non-record stdout diagnostics only, avoiding three
copies of the same full images. Their stderr is retained separately. The
original full raw logs remain in the scratch capture directory recorded in the
commands. This is a source-built Linux reference, not physical tablet evidence.

As with the separately sealed RGBA oracle, the old nonincremental controller
requires a prior valid drawable feature. Each scene first performs a real
incremental warm-up, splits, Undoes and verifies the original pixels, clears
Undo, resets the old brush, then sets the intended nonincremental flag and runs
the recorded stroke. This does not prove old cold-start behavior: the separate
RGBA cold probe documents the old invalid-item setup failure. The new controller
can run these same meaningful strokes from cold state.

The first RGB capture set brush lock-alpha alone. Both old and new core startup
replace that base value from the layer lock flag, so the third group did not
actually exercise locking. Its report and source are retained as
`initial-base-lock-ignored.*`, with full raw output preserved in scratch. The
corrected capture sets the real layer alpha-lock flag in both old and new
stimuli. Only that corrected capture is the golden reference.

`private-comparison.json` records the first exact match from a separately
compiled normal Surface object and comparator linked through private thin
archives against the new application. Production objects were unchanged during
that experiment. Production registration and its repeated comparison are
separate evidence. This fixture is independent of, and does not replace, the
old 32-scene RGBA oracle. Gray/indexed/high precision and arbitrary ICC are not
established by these unprofiled nonlinear RGB-u8 scenes.

Known old runtime parameter-spec and Script-Fu shutdown diagnostics are kept in
stderr. Both corrected capture and private comparator exited zero.
