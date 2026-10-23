# Native path miter damage — bounded original25.009 repair

The unchanged native source at f54373d6e0 uses geometric path bounds with1.5px
padding even for PATH/OUTLINE miter joins. A closed acute PATH triangle leaves
nine actual GDK-window pixels after hiding. The exact failed test, production
source and all declared test inputs remain under `expected-red.json`/`red-source`.
The earlier broad curved-path detector found five leftover pixels in a different
scene; that preliminary observation remains under navigation-item-geometry and
is not substituted for the focused nine-pixel red control here.

Only get_extents changes: unfilled PATH/OUTLINE use Cairo stroke extents with
the same3px/1px widths as drawing, retaining conservative1.5px outward padding.
DEFAULT, filled and every draw/style byte remain unchanged. Existing Cairo
miter geometry is not changed into round joins to conceal the defect.

Final evidence:10 normal +10 ordinary Meson ASan/UBSan registrations, each with
the unchanged60s budget,216 recording-surface bounds and216 actual hide scenes
per configuration. Three path topologies (curved closed, acute closed, open),
three styles, filled/unfilled,0/37/90°, both independent flips are covered.
Initial shown frame is fully exposed. Hide must use production invalidation;
no test full repaint or gtk_widget_draw repairs the final capture. Capture and
comparison use the real server-backed canvas central470px ROI. Actual saved
root PNGs are1080×1920; oncanvas UI is enabled. Each fixture checks image pixels
unchanged. External dependencies are not instrumented; LSan is off.

Private native build remains scoped: ce40ac4cffa5b6dba48cd9100303c3c94653a5bc plus
the source manifests, pinned locked dependencies and gimp-data a31e5dea. O2,
no DWARF, assertions enabled. It is not a fresh full-current-checkout build.
`replay-corner-checkpoint.py` validates the immutable preceding5+5 evidence,
not a rerun. Original25.009 and all196 original duties remain open; no historical
application runtime, physical-input, release, platform or whole-port claim.

Initial normal10 and sanitizer2 passes are preserved separately in
`initial-layout-run/`. The next sanitizer window failed because asynchronous
initial oncanvas allocation changed963→968 pixels between captures, not a
residual-ink comparison. `layout-red-test.inc.txt` and the allowlisted failed
receipt are retained. Final tests wait for five consecutive100ms stable canvas
allocations before interaction (bounded3s); any later change still fails the
unchanged dimension assertion. No final capture is resized or repainted.
