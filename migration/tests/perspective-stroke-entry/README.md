# Perspective direction/constraint/reset through native canvas events

Bounded progress on original26.007–009:87 inherited whole-hunk duties and original
dependencies remain open/unchanged. No DONE child or production fix is claimed.
The new standard `painter-perspective-stroke-events` target reuses the committed
editor event harness by renaming its main at compile time, but registers only
the two new cases. Original13 registration/source bytes are unchanged and rerun
separately, without duplicate execution. Each target keeps60 seconds.

216 poses combine three zooms(.5/1.25/4),three angles(0/37/90),two aspects,all four
flips and1–3vanishing points, each with two independent consecutive strokes.
The432 strokes use fractional origins/pan and real synthetic GDK events:
31.999 scaled pixels remain pending;32.001 starts exactly once at the origin and
drops the crossing motion sample; subsequent coordinates match unchanged old
lazy projection; release clears snap/grab/queued history before the next stroke.
A separate exact unrotated32px case covers the closed boundary, a45degree tie
preserves first horizontal priority, and a short third stroke cannot inherit the
previous locked direction. The constraint is old dominant-axis line arithmetic,
not an invented orthogonal projection.

Both old snap-angle and lazy-projection functions are unchanged pinned source.
The oracle consumes current native input-boundary coordinates on purpose: this
isolates old arithmetic/entry ordering, not old viewport-matrix parity. Model
accessors use current default-angle-zero data, excluding the documented old
angle/id accessor bug. No old application, physical pen/backend history,
paintbrush raster stroke, smoothing/symmetry or platform parity is claimed.
Original26.013 compatibility remains separate/open.

Three causal test-oracle mutations (31px threshold,last-tie priority,reversed
projection) must each fail native acceptance before exact source restoration.
The final complete normal/ordinary ASan+UBSan runs follow restoration. All9 held
providers remain committed private baseline bytes; no held diagnostic is run.
104 scoped inputs/archive and tested ELF hashes are sealed. Dependencies remain
uninstrumented and leak detection is disabled. No raw environment JSON is saved.
Authenticated Xvfb verifies1080x1920x24 and each new case enables oncanvas UI.
The existing committed editor portrait shows this unchanged display configuration;
this recording-tool input test does not claim a new screenshot of painted ink.

Final restored-source results: normal13+2 PASS (7.70s/2.68s), ordinary sanitizer
13+2 PASS (15.76s/4.54s). Three causal controls fail as intended; seventeen
semantic evidence controls pass. No new native failure or production fix.
Run `check-stroke-entry.py`, `test-stroke-evidence.py`, and the old fixture's
`verify-source.py` for the independent documentary/source checks.
