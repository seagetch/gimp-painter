# Perspective editor through the actual DisplayShell event route

Bounded progress on original26.002/003/004/006, not task closure. All62 inherited
whole-hunk duties, their unchanged acceptance criteria and dependencies remain
open and traceable in audit.json. No new DONE child or production change.

The standard `painter-perspective-events` Meson registration retains its nine
original tests and original60-second timeout, adding four native entry tests:

*72 poses:3zooms(.5/1.25/4),3angles(0/37/90),two resolution aspects,all four flips.
  Fractional pan and subpixel image coordinates survive actual synthetic GDK
  hover/press/motion/release through DisplayShell: add3, reject fourth, move,
  delete middle/compact, delete remaining and clear model.
*1152 entry checks use the unchanged pinned old zoom and circle-handle functions
  as an independent bounded oracle:552hits/600misses.13px means integer radius6,
  strict squared distance<36 after old scaled-coordinate rounding, not6.5px.
*Source-audited nearest policy: exact image-distance tie preserves first index;
  anisotropic nearest-image point outside its circle does not fall back to the
  other point whose display distance is smaller. This two-case source assertion
  is separate from the unchanged single-point old-function oracle.
*Actual right-button cancellation rolls back, clears snap/grab state, then a new
  edit, native Undo/Redo and deletion adopt the current model and clear motion
  history. Undo/Redo is the current enhancement, not claimed old behavior.

Native canvas deliberately filters Escape while Button1 is held unless lazy
Perspective snap is pending. The earlier test expecting active-editor Escape
rollback failed; its diagnostic is retained. The final test explicitly verifies
Escape does not roll back that edit, then uses the native right-button release
cancel policy. The tool's direct Escape callback is NOT claimed reachable here.
Old tool Escape merely returned TRUE; this is not an established old regression.

All events use a synthetic GDK master pointer for event and source device. This
is not physical pen, backend history, platform parity, or original25.012 proof.
Forward placement uses current matrix arithmetic without SCALEX/Y rounding;
this checks real input retention, not an independent old-viewport matrix oracle.

Diagnostics preserve omitted physical modifier-release and pre-press hover
sequences. Correcting the synthetic protocol, not production, resolves these.
The two causal test mutations round incoming device coordinates or change old
oracle radius to7; each fails the appropriate native assertion, then exact
inputs are restored and the full13-case target is rerun in both configurations.
All9 held providers remain committed private baseline bytes and no held
candidate/diagnostic is imported or executed.96 scoped inputs and tested ELF
hashes are sealed; dependencies are uninstrumented and leak detection disabled.

Authenticated Xvfb verifies1080x1920x24. The new editor tests enable oncanvas;
actual1040x1800 screenshots show the three-point guide after real synthetic
add/move at1.25zoom/37degrees. They are current app rendering, not old runtime.
`final-first` preserves preceding13+13 proof without the screenshot branch;
`final` is the authoritative source-bound rerun with that branch.

Final results:13 normal cases PASS in9.13s,13 ordinary ASan/UBSan cases PASS
in16.27s, each as the unfiltered standard Meson target within60s. Both causal
mutations fail as expected;15 evidence-mutation controls pass. The original nine
case identities remain exactly represented. No production fix is claimed.
