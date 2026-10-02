# Executed legacy fixtures and observations

These files came from **real execution of pinned gimp-painter code** on the
recovered Linux reference. Test inputs were constructed by the scripts/harness;
no tablet or manual UI recording is claimed. None of the XCF bytes or expected
pixels were hand-encoded to impersonate an old application run.

Source commit: `afa43fae3e920210146abed514f136fd49f671b5`.
Build lock, explicit modern-toolchain adaptations, binary hashes and original
build/test logs: `../../baseline/legacy/`. `manifest.json` locks this package's
files and identifies the exact capture executable/patch for each run.
`expectations.json` records observed outputs, including failures.

## Ordinary drawing and editable file

`ordinary-layer.scm` executed in the installed, uninstrumented old application
with `--no-interface --no-shm`, an isolated GIMP2_DIRECTORY and Script-Fu. It:

1. Created a 96×80 RGB image, a white background, group and 80×64 RGBA child
2. Created a procedural round brush and drew one controlled four-point stroke
3. Applied child offset9,7, Multiply and 75% opacity, added a white layer mask,
   channel, 143×167 resolution and selection rectangle8,10–28,28
4. Saved `ordinary-layers.xcf`, exported `ordinary-layers.png`, reopened the XCF
   with the old reader, checked names/types/counts/geometry/opacity/mask/channel/
   resolution/selection, then exported `ordinary-layers-reopened.png`

Both PNGs have 78 unique colors and the same decoded RGBA hash. They depict a
single colored stroke on white. The old writer quantizes opacity: input75%
becomes integer191 and reopens as 191/255×100 = 74.9019607843%. This is the old
format's measured result; exact75% is not an appropriate old-reader assertion.
The script's original absolute output paths are retained as execution evidence;
change only those paths when running it in another isolated directory and
record the adapted script's hash.

## CloneLayer files and behavior

`clone-normal-in-group.xcf` and `clone-group.xcf` use the real old constructors,
writer and reader. Each reopens a custom GimpCloneLayer (24×20, offset32,30),
resolving respectively to the normal child of a source group or the source group
itself. They are not flattened substitutes.

`capture-instrumentation.patch` extends the old `app/tests/test-xcf.c` with a
separate environment-selected capture entry point. It uses the same core
libraries and records normal application critical-message behavior; it does not
claim the original GLib test suite passed. Build it with the recorded C/C++
flags and test link additions `../gimp-features.o -lstdc++
../presets/libapppresets.a -ljson-glib-1.0`. Run with
GIMP_PAINTER_FIXTURE_DIR pointing at an isolated output directory, and leave
GIMP_PAINTER_CAPTURE_FILTER unset for the successful clone-only capture.

The final `clone-capture.log` records ten pixel/geometry checkpoints:

- Source RGBA204,51,102,128 and opacity0.5 produces clone alpha64
- Applied mask128 reduces it to alpha32
- Show-mask produces RGBA128,128,128,255, without source opacity reduction
- Hidden source and Multiply source retain the same cloned RGBA as normal mode
- A same-size source move0,0→5,7 leaves clone32,30 fixed
- A source resize16×16→20×18, moving source5,7→7,10, moves clone32,30→34,33
- Clone scaling is a no-op in the old implementation
- Pending name resolution can supersede a direct setter when that name later
  resolves; the pending name clears only after successful resolution
- Removing a source with Undo enabled keeps the borrowed source pointer valid
  while the Undo record owns it; Undo reattaches it and updates still propagate

Final destruction after clearing Undo history was **not** exercised.
`clone-dissolve-alpha.bin` is the measured 16×16 row-major unsigned-alpha output
for source RGBA204,51,102,128, opacity0.5, applied mask128, Dissolve, source
position0,0 and clone position32,30. It contains 35 nonzero pixels. Its SHA-256
is `e391af9278f0852484c254d50220eca234a3b257f98ddc489ba402ac9182f9eb`.

## FilterLayer: genuine output, negative reader result

`filter-edge.xcf` was saved by the actual old writer after the old FilterLayer
scheduler ran `plug-in-edge` asynchronously to completion. Its six argument
slots include amount2, wrap1, algorithm0; the cached pixel at12,12 was measured
as RGBA0,0,0,255. The log records pending→completion→idle over about42ms for this
small controlled input. This is an observation, not an input-latency benchmark
or a scheduler stress/convergence guarantee.

**The old reader segfaulted on this file**, both in the harness and the
uninstrumented installed application (exit139). The reader's UNSUPPORTED branch
uses `g_array_append_val(args, value)` where `value` is a pointer, despite the
array containing GValue objects; the image-ID argument is serialized with that
unsupported tag. This is a source-identified candidate cause, not a captured
backtrace. The migration should preserve the saved bytes safely, not emulate
undefined memory behavior.

`capture-instrumentation-filter.patch` is the earlier, exact harness/probe
revision used for that run. It adds read-only pending-state observation and an
environment-enabled completion counter/log to FilterLayer::end. Its executable
hash differs from the later clone Dissolve/Undo harness. The output was captured
before reopening failed; do not label it a successful old FilterLayer roundtrip.
`filter-uninstrumented-reopen.log` independently records the original app's
failure. No INT16/string quirk fixture or composite-preset fixture is claimed.

## Limits and rights

GUI/Xvfb startup is unverified because the environment could not create local
X listening sockets. Startup GValue, old GEGL cache-size, and headless GUI
registration warnings remain in logs. The harness process exit also closes its
Script-Fu child pipe; the resulting pipe warning is retained, not a shutdown
correctness pass. These files do not close MyPaint input capture, rotation,
full layer-operation coverage, scheduler stress or composite preset tasks.

These are newly constructed minimal migration test scenes, with no private
user artwork or third-party image imported. Scripts, instrumentation and test
assets are retained with the repository's licensing and source notices. The
old implementation's own notices remain in the pinned source. Hash verification
protects integrity; it does not independently prove provenance or compatibility.

Run the package checks with:

```sh
python3 -m unittest discover -s migration/tests -p test_legacy_runtime_fixtures.py -v
```

These checks validate captured evidence. They do not rerun the old application
or substitute for migration application integration tests.
