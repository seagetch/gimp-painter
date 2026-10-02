# Stroke record v1 (WBS 02.009)

`stroke-record.schema.json` specifies a JSON document for one complete stroke.
It preserves source inputs, engine state, assets and expected output separately.
The format and validator are ready; no legacy capture or replay adapter has run.
The unit tests construct explicitly synthetic format examples in memory only.

## Capture point and units

Capture in pinned `app/paint/gimpmypaintcore.cpp:198–203`, immediately before
`Brush::stroke_to`, after options and `Surface::set_coords` have been applied.
Keep one event per invocation, including stationary pressure changes and release
samples. Do not merge samples, resample, smooth again, clamp input values or
replace elapsed times. The schema fixes this capture boundary; raw device/UI
traces need a separate format rather than pretending to be post-transform input.

- `sequence`: contiguous zero-based call order, including equal timestamps
- `monotonic_ns`: recorder's monotonic nanoseconds from capture origin; may be
  equal but must not decrease
- `dtime_s`: exact original elapsed-seconds argument, signed to preserve a clock
  glitch. It is deliberately independent of recorder time
- `coords`: all eight pinned `GimpCoords` members (`app/core/core-types.h:231–241`)
  as finite doubles: x/y in drawable coordinates, pressure, x/y tilt, wheel,
  velocity and direction. Preserve the caller's values without normalization;
  record the device/transform conventions in the capture log
- `phase`: begin/motion/end; marks one complete externally initiated stroke
- `modifier_state`: original GDK modifier bitmask, decoded in the capture log
- `engine_split_after`: actual return from `Brush::stroke_to`. Internal splits
  do not manufacture a new device press/release or discard engine state

Store enough decimal digits to round-trip the original binary64 inputs (for
example Python `json.dumps` or C `%.17g`). JSON NaN/Infinity are rejected. A
future recorder that needs non-finite failure inputs must encode explicit bit
patterns in a separately versioned failure format.

## Engine state and reproducibility

The old `Stroke` class records only `dtime` and `GimpCoords`; its `start()` has
an unimplemented settings/state-recording TODO (`mypaintbrush-stroke.cpp:46–68`).
Using its in-memory recording alone is therefore insufficient for a baseline.

Capture every `Brush::states[]` slot in enum order, its matching state name and
IEEE-754 binary32 bits as eight lowercase hex digits (numeric bit pattern,
independent of host byte order). Capture `reset_requested`, both elapsed stroke
counters and the effective uint32 seed used by `g_rand_set_seed`. A recording
adapter must verify the exact `STATE_COUNT` and name order against this pinned
source; the format validator only checks equal lengths and unique names.

The engine stores `STATE_RNG_SEED` in a float, reseeds GRand on every call at
`mypaintbrush-brush.hpp:764`, and writes `g_rand_int` back to that float at line
885. Retaining only an intended seed, or seeding a different RNG once at start,
is not equivalent. Capture both the stored float bits and actual converted
uint32 value. `new_stroke()` clears time counters, not every state slot
(lines 188–192); don't silently replace the captured snapshot with all zeros.
Initial `reset_requested` and time-gap reset behavior also matter.

`brush.effective_settings` references a complete settings/switch/text snapshot
at capture time, not merely the saved `.myb`: options and alpha-lock can change
values before the call. `surface.resource_manifest` records brush shape, texture,
transform/phase, layer offset, selection/masks, foreground/background colors,
non-incremental mode, stroke opacity, alpha lock and every referenced resource
hash. Record absent resources explicitly. Capture the initial source pixels
(including the exact sampling source), dimensions, format, alpha convention and
color space. These sidecars are part of runtime review; their contents are not
silently synthesized by the format validator.

## Provenance and expected results

`evidence_kind` must be `synthetic-format-test` for hand-authored tests.
`legacy-runtime-capture` requires output checkpoints and the full pinned commit,
build manifest, recording patch and capture log sidecars. Sidecars use relative
paths within the record directory and SHA-256 of their exact bytes. Build
manifests include binary and dependency hashes, architecture, compiler options
and GLib version. Record changes to the reference as patches, never overwrite
the pristine reference checkout without preserving the diff.

Each result identifies the event after which pixels and an editable document
were captured. Pixel sidecars specify row order and any stride/padding in the
capture log; hash canonical, uncompressed data as well as any encoded image.
Hash equality is byte equality, not visual/perceptual equivalence. No tolerance
or rendered success is inferred from a valid record. Match source resources
against `../legacy-source/assets.tsv` when using bundled brushes.

## Validation

Requires Python 3 and `jsonschema` (tested with 4.26.0 / draft 2020-12):

```sh
python3 tools/validate_stroke_record.py path/to/stroke.json --verify-files
python3 -m unittest discover -s migration/tests -p test_stroke_records.py -v
```

The validator checks schema, finite numbers, call/time ordering, event phases,
state lengths, result indices, portable paths and optionally sidecar existence
and hashes. It rejects silent extension fields; add a new version for a changed
contract. Passing this validator cannot establish authenticity or demonstrate
that a replay engine, application writer, or UI event path executed correctly.
