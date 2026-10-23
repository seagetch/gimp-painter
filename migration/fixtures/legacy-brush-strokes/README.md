# Original brush-engine stroke evidence (02.010)

Three actual executions of pinned gimp-painter `afa43fae3e920210146abed514f136fd49f671b5`:

- `pen.json`: `classic/pen.myb`, pressure/speed-sensitive analytic ink
- `textured_ink.json`: `classic/textured_ink.myb`, randomized dab radius/offset
- `watercolor_expressive.json`: `deevad/watercolor_expressive.myb`, smudge and randomized ellipse

Each has 18 caller-generated events on the same two-color 96×80 RGBA drawable.
The inputs are synthetic, explicitly supplied at the post-transform engine
boundary. The brush equations, surface drawing, sampling, native XCF writer,
and output bytes ran in the old implementation. These are **not physical tablet
or original UI event/coordinate-transform measurements**, and are not the
schema-only synthetic examples in `test_stroke_records.py`.

## What is measured

`gimpmypaintcore.cpp` observes each call after options and `surface->set_coords`,
immediately before the existing `Brush::stroke_to`. The raw `boundary.jsonl`
retains all eight GimpCoords values and the original binary64 dtime (17 decimal
digits), complete 30-slot binary32 state bit patterns, reset flag, both elapsed
counters, every allocated engine setting/mapping, and actual surface resources.
Names/order are checked against the bundled pinned enum header. The public
record contains the initial engine state; every subsequent snapshot remains in
the raw sidecar.

`seeds.jsonl` is written at the actual `g_rand_set_seed` site. The original
implicit float-to-uint32 conversion is assigned to a named uint32 local and
that exact value is passed to GRand and logged. No harness seed is set, and no
state is replaced. Initial zero states/seed and later nonzero seeds were
observed. A synthetic zero-time stationary pressure change and the pressure-zero
release are retained. All three first calls return an engine reset/split; the
other calls do not. The external begin/end phase is not split at that boundary.

Monotonic timestamps are measured with GLib's microsecond clock and expressed
as nanoseconds (`microseconds * 1000`); they do not claim nanosecond resolution.
The published record subtracts the first raw timestamp without changing any
delta. Recorder filesystem work affects wall-clock capture spacing, while the
actual supplied dtime arguments remain unchanged. Phase and modifier state are
explicit harness input metadata (no modifiers), not device measurements.

Each resource manifest includes exact source .myb identity matched against
`../legacy-source/assets.tsv`, the loader's icon resource (not used for stroke
geometry), all switch/text values, foreground/background, opacity, alpha lock,
nonincremental mode, layer offset, selection bytes and initial XCF/pixels.
All three loaded brushes actually have every switch false and both text slots
null. Thus there is no external brushmark, texture or layer mask. Empty selection
means no effective mask. This is observed state, not a guessed property default.
The analytic dab transform comes from original per-dab brush settings; no UI
rotation/zoom transform was applied. Sampling uses current drawable pixels,
including earlier dabs. Initial pixels have two colors split at x=40, produced
by the original rectangle-selection/edit-fill API. RGB is untagged legacy
numeric RGB8; no calibrated display/color-profile result is claimed.

## Checkpoints and verification

Final RGBA and XCF are taken after event 17 and the normal `split_stroke()`
session finalization. Pixels are uncompressed row-major top-to-bottom,
left-to-right RGBA8, straight alpha, 384-byte rows, with no padding.

For all three captures:

- capture process exit 0, with `STROKE_CAPTURE_COMPLETE`
- original reader in a fresh isolated harness process reopens the XCF and dumps
  RGBA byte-identical to the final capture
- the preserved installed old binary independently reopens the XCF and checks
  dimensions, layer count and name; this separate check does **not** dump pixels
- a second original-engine execution produces identical input/state/seed traces
  (except recorder timestamps) and byte-identical output RGBA
- three records pass `validate_stroke_record.py --verify-files`
- nine measured-evidence tests and all eleven format-only tests pass

The preserved installed binary SHA-256 remains
`85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1`.
The different capture executable, linked dependencies, actual compiler flags,
baseline build configuration and package lock are identified in the manifest.
The capture observed GLib 2.84.4 and num-processors=9. This is a rebuilt
source-faithful reference, not a historical binary-identical environment.

Warnings remain visible in full logs: startup GValue assertions, obsolete
plug-in registration, loader `unknown key 'colorize'`, undo-type registration
assertions, thumbnail-directory failures, and Script-Fu shutdown pipe warnings.
Successful capture/reopen does not establish a warning-free runtime.

## Reproducing the recorded run

The recorded build used an isolated copy of the already built original tree,
`legacy-strokes-20261015/gimp`, sharing only read-only dependency/install paths
with `legacy-filter-positive-20261015`. It never installed over the preserved
binary or changed earlier fixtures. `capture-source.patch` is the complete
tracked-source delta plus the new harness against the pinned original commit;
its application was checked against the original file contents. It includes
previous build compatibility edits and a dormant FilterLayer observer, which
this harness does not enable. Brush equations/surface/reader/writer were not
replaced. `stroke-capture.cpp` is included as a new file in that patch.

The original constructor leaves two pointers uninitialized. The harness uses
placement-new over zeroed allocation to make its options pointer defined,
without changing the constructor or brush state. This harness choice is part
of the baseline contract.

The exact packaging implementation is supplied as `package.py` (Python 3
standard library only); there is no additional packaging helper. Stage this
file and the supplied `.sh` scripts in `legacy-strokes-20261015/`, alongside its
patched `gimp/` tree. The packaging script reads `captures/`, `repeat-captures/`,
recorded build logs and the sibling maintained source/baseline directories at
the explicitly recorded workspace layout; it does not recreate the baseline
build itself. It emits `deliverables/migration/fixtures/legacy-brush-strokes/`.

From the workspace containing the existing pinned baseline dependencies/build:

```sh
# For the existing isolated, patched capture tree:
bash legacy-strokes-20261015/build.sh
# Archive existing captures/repeat-captures before another run: recorders append.
bash legacy-strokes-20261015/run.sh
bash legacy-strokes-20261015/reopen.sh
bash legacy-strokes-20261015/independent.sh
bash legacy-strokes-20261015/repeat.sh
python3 legacy-strokes-20261015/package.py
```

The supplied scripts are exact, path-specific records. For another workspace,
relocate their explicit workspace/dependency paths together with the baseline
build environment. For a clean source reconstruction, checkout the pinned
commit, apply the complete source patch once, and configure/build using the
recorded baseline build options/dependency lock; do not apply the same
compatibility patches twice. Existing compiled trees must be isolated before
patching. No ptrace, preload, injected code, interposition or security-setting
changes were used.

`build.sh` intentionally touches `test-xcf.c` after compiling the separately
linked C++ harness: the historical make target does not track that added object
as a prerequisite. Without this forced relink it can reuse a stale executable.
An exploratory stale-link attempt was rejected and archived outside this
package; final captures were regenerated and checked for their runtime marker
and two-color initial pixels. Only final records are deliverables.

After copying this fixture and the test into the maintained source tree:

```sh
for brush in pen textured_ink watercolor_expressive; do
  python3 tools/validate_stroke_record.py migration/fixtures/legacy-brush-strokes/$brush.json --verify-files
done
python3 -m unittest discover -s migration/tests -p test_stroke_records.py -v
python3 -m unittest discover -s migration/tests -p test_legacy_brush_strokes.py -v
```

## Boundaries of acceptance

This package establishes representative old-engine input/output fixtures only.
No physical tablet/UI event path, imported-event replay adapter, GIMP 3 replay,
cross-machine determinism, brushmark/pattern-enabled variant, nonempty
selection/layer mask, alpha-lock, nonincremental or resumed existing-state case
is established. It must not be used to claim any such result. All-sidecar
integrity and schema success alone are not authenticity proof; review the
instrumentation, build provenance and logs together.

Source notices are included. Classic brush provenance follows the original
LICENSE/COPYING register; the Deevad set includes its CC0/public-domain notice.
The input canvas/path is newly constructed fixture data, not a private artwork.
