# Actual old full-session comparison

The independent old-side capture is sealed in
`migration/fixtures/legacy-mypaint-session/`. It calls real old
GimpMypaintCore/options/loader, drawable tile renderer, ordinary resources and
Undo/Redo, linked from pinned application archives without editing those feature
sources. Explicitly synthetic 32-scene input yields 96 complete 130×96 RGBA
snapshots plus Undo results and completion marker: 129 records, 9,587,537 bytes.

The new `app/tests/painter-mypaint-session-trace.cpp` calls the production
PaintCore/GimpResources/Engine/GeglSurface and current native GimpPaintCore Undo
path. Meson `painter-mypaint-legacy-session` compares every record exactly. Four
fixture tests check provenance, values, actual before/finish/Undo/Redo semantics,
the separate cold failure, and retained failed color-stimulus evidence.

## What is established

Incremental/nonincremental, ellipse/asymmetric bitmap, paper, offset selection,
ordinary paint with opacity1 and smudge with opacity.37, stationary pressure and
movement across old 64-pixel tile boundaries all agree for the recorded nonlinear
RGBA-u8/unprofiled stimulus. Native images start cold and run safely. Old source
hashes, archive and executable fingerprints, compile/link output, full stdout and
stderr and all exact harness source are sealed. Existing old-reference executions
were not repeated while building the new comparator.

## What is not established

The old side is explicitly warmed with an incremental stroke/Undo/reset sequence
before each scenario. Cold old nonincremental initialization instead exited139;
a separate diagnostic-only signal handler shows get_height inside Surface
begin_session. The original tool source does not perform a preceding explicit
Surface refresh. These are separate negative findings, not passing cold parity.
The warmed bytes are not proof for arbitrary ICC profiles, RGB/gray or higher
precision targets, brush pipes, all177 brush renderings, GUI/tablets or platforms.

The first new harness compilation failed on a missing declaration header. After
that was corrected, a color-stimulus mismatch affected RGB but not alpha: GEGL
rgb() string parsing did not encode the intended old nonlinear GimpRGB values.
Setting explicit nonlinear double pixels reproduced the input and made all
records match without changing production painting code. Failures and successful
results are stored separately; old startup diagnostics are not suppressed.

The optional `capture_mypaint_sessions.py` reproduction runner verifies old feature
hashes, substitutes only the input file path in a temporary harness copy and
reuses archives. It does not run the known cold crash probes. It was added after
this sealed capture and has not itself been executed; the recorded source, link
and runtime outputs remain the evidence for the original execution.
