# 23.004/native-default-size: dedicated Smudge defaults

This is a limited compatibility fix based on `aec6d72965`, reconstructed from
current source and independently verified against pinned old source/samples.
The original 23.004 transformed-footprint/repositioning task remains open.
No normal CI workflow is changed.

## Defect and scope

Dedicated Painter Smudge intentionally owns its old transform implementation
without setting the generic geometry provenance flag. Two shared providers
incorrectly used that flag as the only legacy gate: empty smooth neutral
curve lookup and native default brush size. The fix recognizes the dedicated
Smudge options type in these two providers only. It does not enable generic
geometry, mutate shared dynamics, or change standard Smudge behavior.

The native tests compare six linear outputs across 1030 inputs with 256
immutable old-runtime curve samples. Of 6180 comparisons, 1986 differ from
modern neutral mapping. Explicit smooth and free curves remain unchanged.
The size test compares 72 generated brush configurations to a source-only
oracle re-expressed from the pinned old generated half-size/default-size code.
It also covers explicit and context-default brush selection, linked radius
updates/unlink, bitmap size, real bitmap/generated unit-scale strokes and
real default-dynamic scale. Both real stroke checks cancel cleanly.

Generated size expectations are not a fresh execution of the old application.
The old source and captured curve provenance are in `old-source-reference.json`
and `migration/fixtures/legacy-brush-geometry/default-dynamics.json`.

## Validation and reproducibility

Use a private Meson build with headless tests enabled and source the dependency
environment. Build `app/tests/gimp-painter-smudge`, `app/tests/painter-smudge-trace`,
`app/tests/painter-brush-geometry`, `app/tests/painter-smudge-ui`,
`menus/image-menu.ui` and `menus/dockable-menu.ui`. Supply private HOME, XDG and
TMPDIR locations. Run these scripts under `tools/run-painter-xvfb.py`:

- `run-controls.py BUILD`: individually restore each pre-fix provider, require
  the corresponding final regression to fail, then restore the fixed bytes and
  require it to pass. Do not run this concurrently with another build.
- `run-native.py BUILD`: execute native Smudge, geometry, oncanvas Smudge UI,
  the two old 48-scene stroke routes and all three old generic Smudge routes.
- `migration/tests/run_smudge_default_sanitizers.py BUILD --report REPORT`:
  focused ASan/UBSan/float-cast-overflow recompilation and exact old routes.
  The report explicitly enumerates instrumented units. Host/dependencies are
  not fully instrumented; LSan is disabled.

The direct runner uses the same registered native tests and comparators. It
keeps the authenticated 1080x1920 display; it does not invoke the standard
Meson wrapper's additional 1280x1024 Xvfb/DBus session. UI tests enable and
assert the real oncanvas configuration/visibility. The screenshot records
pending-stroke Undo cancellation, not transformed-footprint parity.

## Initial failures retained

- The unmodified provider fails both new regressions: neutral bit-exact sample
  inequality and generated default size 17 instead of old 19. The initial
  baseline test snapshot is retained separately from final control inputs.
- The first linked-size fixture omitted native paint-info, so it never installed
  the brush-change handlers. The test setup now supplies the registered Painter
  Smudge paint-info. Its initial assertion is retained, not labeled a product bug.
- The standard Meson launcher could not create the extra DBus UNIX socket;
  affected tests had not started. Three generic-route registrations succeeded.
  Tests are then executed directly on the already authenticated portrait Xvfb.
- The first UI launch lacked generated image-menu.ui; the Meson menu targets
  were built before rerunning unchanged UI tests.
- The first relocated sanitizer binaries had the original relative RUNPATH,
  so they loaded a system libgimpbase lacking Painter symbols. The dedicated
  runner now explicitly selects all matching built libgimp libraries.
- The next focused sanitizer attempt mixed the uninstrumented no-RTTI
  Filter-config owner with instrumented BindingStore vptr checks. The dedicated
  runner includes that registered owner and its archive, matching the existing
  alpha-Smudge runner's boundary. This is not treated as a product defect and
  sanitizer checks are not disabled.

No arbitrary angle/aspect accumulator relocation, complete UI/profile save,
physical tablet, broad platform, or whole-HEAD sanitizer claim is made.

## Final result

All eight direct native groups pass: 28 Smudge cases, 7 geometry cases,
26 oncanvas UI cases, two 48-scene/193-record/2,952,800-byte old raster routes,
and three old generic Smudge routes. Both individual pre-fix gates fail the
final regression source and exact restoration passes. Focused 23-unit
ASan/UBSan/float-cast-overflow passes the 28 native cases and both old raster
routes. WBS validation and `verify.py` pass. Of the 72 generated defaults,
46 differ from modern default size and match the pinned old formula instead.

## Preset integration

The Smudge commit was rebased without conflicts onto preset commit
`23d58519d934ad583bfb96ead38c76c218e68b25`. Its production/test input hashes
are unchanged. The final native/sanitizer/control receipts were rerun after
this integration, including all eight Smudge groups, the 23 instrumented units,
and both individual red/restoration controls. The same combined build also
passes preset catalog31, catalog GUI5, original GUI-entry6, LayerPreset12 and
LayerPreset UI18 (72 cases). These are integration checks, not a rerun of the
preceding task's full GIMP build or its separate preset sanitizer suite.
The prior preset evidence is untouched; `integration.json` identifies the
combined tested source and the additional checked source hashes.
