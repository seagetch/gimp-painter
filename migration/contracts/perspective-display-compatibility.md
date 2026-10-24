# Original26.005: Perspective display compatibility checkpoint

Original criterion `zoom・pan・回転・反転に追従` and dependency26.004 remain
unchanged and open. This is a bounded progress checkpoint, not a new DONE child.
All193 original source duties and their other owners are retained in
`migration/tests/perspective-display-compatibility/audit.json`; none is closed.
The176 shared-transform mappings reference immutable25.009 evidence without
silently discharging this task's original assignments.

## Proven valid-path differences

Pinned source afa43fae, CanvasPerspectiveGuide::draw, whole-hunk01.002/000532
(work legacy-cf0e7373680579646933), constructs paths in floating image coordinates.
The prior port rounded centers through SCALEX/Y and started separate circular
subpaths. The old valid path instead includes one radial segment for one point,
one radial plus one connecting segment for two points, and three radial segments
for three points. An unchanged-function standalone Cairo adapter proves those
path counts. Native expected-red independently records34 mismatched pixels at
integer zoom1 and242 at fractional zoom. These are not old-runtime reruns.

The narrow correction restores image-coordinate Cairo translation/scaling and
count-specific path ordering. The canvas group's current shared outer matrix,
native stroke, ownership, callback lifetime and extent policy remain unchanged.
The native old-function adapter deliberately supplies the same outer matrix and
native stroke to both rasters: it proves old path construction, not old viewport
or old styling/runtime parity. No physical input or other-platform claim follows.

## Acceptance boundaries

The evidence package records old-path raster comparison, every-ink containment,
and actual incremental redraw after add/move/remove/replacement/clear with
oncanvas UI and authenticated portrait1080x1920x24 Xvfb. Original core39/UI24/
event9 regressions remain registered. Event9 retains its historical setup and
is not retroactively labeled oncanvas-enabled; the actual new rendering/UI tests
explicitly enable oncanvas. Original dependencies, device gates and other owners
remain open. No whole-port aggregate or held-provider candidate is admitted.
