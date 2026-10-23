# Positive original FilterLayer XCF fixture

This is an additional real old-writer/old-reader fixture from pinned gimp-painter
`afa43fae3e920210146abed514f136fd49f671b5`. It uses the original bundled PDB
procedure `gimp-brightness-contrast`; no custom adapter, replacement renderer,
repaired XCF implementation, or hand-encoded XCF was used.

The original `legacy-runtime/filter-edge.xcf` remains a negative reader fixture.
The same uninstrumented rebuilt app still exits139 when reading it. This new
fixture does not repair or replace that case.

## Measured acceptance

- Original FilterLayer invocation: drawable context, brightness25, contrast−10
- Real original writer saved a 64×64 image with custom FilterLayer, 20% opacity,
  Difference mode, and lower 24×20 RGBA layer at8,7
- Original core reader recovered custom type, procedure, both numeric argument
  values, layer order/count/names, geometry, mode and opacity
- Every cached RGBA byte matches both the original PDB procedure applied to an
  ordinary control layer and the reopened FilterLayer cache
- Cache pixel12,12: RGBA200,78,118,255; full cache SHA-256:
  `24280d0fab549abf2d546880814467c1e96054b193cca5715ca6c49ac960b331`
- Every lower-layer RGBA byte also matches after reopening
- Independently, the installed uninstrumented original application reopened
  the file, checked the layer count/name, printed the explicit pass marker and
  exited0. Its binary hash is in the manifest and separate replay report

The drawable is serialized through the original DRAWABLE placeholder tag.
The original reader skips context arguments when restoring the procedure;
there is no image-ID/UNSUPPORTED slot in this three-argument procedure.

## Important scheduler limit

The original LUT implementation performs the pixel operation synchronously and
emits no progress-end callback. The original runner remains pending=1 and the
observer sees zero completion callbacks. The first harness attempt incorrectly
required an end callback and aborted; its log is retained. The successful
capture instead independently verifies all output pixels against the original
PDB control and explicitly records pending=1. It neither forces idle nor treats
this as a scheduler, responsiveness, convergence or later-edit pass.

This is acceptance of the bounded fixture save/reopen task only. A fixture
that also proves scheduler completion remains a different requirement.

## Provenance and reproduction

`../../baseline/legacy/replay-20261015/report.json` records a successful pristine
replay of the existing pinned build recipe. The uninstrumented app was installed
and hashed before the separate capture instrumentation was built. The original
XCF reader/writer and ProcedureRunner files were never modified.

`capture-instrumentation.patch` applies to the pinned old source. It adds the
capture entry point, a read-only pending observer and completion logging;
it does not change decisions in the scheduler. Build only the test/core capture
libraries after installing the reference app, and do not reinstall the
instrumented build over that reference. The test link additions are:
`test_xcf_LDADD=$(LDADD) ../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0`.
Run with an isolated `GIMP2_DIRECTORY`, `GIMP_PAINTER_FIXTURE_DIR` output directory
and `GIMP_PAINTER_CAPTURE_FILTER=1`. Source the rebuilt environment first.

The `*-record.sh` files preserve the exact local orchestration used; their local
staging paths are records, not portable turnkey entry points. The patch is the
portable capture source. The independent `.scm` files preserve executed paths;
when moving the experiment, adapt only those paths and record the new hashes.
The uninstrumented app uses `--no-interface --no-shm
--batch-interpreter=plug-in-script-fu-eval -b '(load ".../reopen.scm")'`.

All inputs are newly constructed minimal test scenes, without user artwork.
Repository licensing and upstream source notices apply. Startup GValue,
obsolete GEGL-cache, headless plug-in registration, font-cache and harness
Script-Fu shutdown warnings remain in their logs. No GUI/tablet test is claimed.
Manifest tests verify captured evidence; they do not rerun the application.
