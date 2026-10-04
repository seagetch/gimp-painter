# Filter execution inventory, point mappings and native precision

This checkpoint extends the independent FilterLayer. It does not complete the
108-row general procedure inventory, external plug-in isolation
or the latency acceptance gates in tasks 15–18.

Generation-scoped native/isolated progress and the GTK owner surface are covered
by the separate `filter-progress.md` checkpoint.

## What the original selector actually offered

`tools/inventory_filter_routes.py` extracts procedure names, menu paths and PDB
type identifiers from the genuine pinned reference's compiled plug-in registry.
It implements `gimplayerpopup.cpp:is_allowed_proc_path()` and the image/drawable
argument requirements literally. It emits public metadata, never the registry,
profile, inherited environment or session files. Its source hashes pin the old
selection rule and every matched bundled registration source/header.

The result is 94 compiled popup procedures,9 source-eligible Script-Fu registrations
and 5 additional direct-definition entries (four Gaussian aliases and Threshold
Alpha):108 rows. The nine scripts are source evidence, not a runtime registration
proof. Dynamically installed third-party extensions are unbounded; an unknown
saved definition cannot be classified as safe merely because the PDB resolves it.
`filter-definition-callers.json` records the app/preset definition paths.

`filter-procedure-routes.tsv` distinguishes current bundled source mentions from
GEGL thematic counterparts and from an executable compatibility route. Source
presence is not proof that a plug-in was built/installed. A thematic counterpart
is not a tested parameter, radius, alpha, channel, context or pixel mapping.
Every inventoried name has original bundled source. Their unimplemented routes
are missing port code, not mislabeled missing external dependencies. Multi-output,
image/palette-mutating, secondary-drawable and resource-context operations need
individual adapters; calling an arbitrary script/file/eval/PDB name from an XCF
is not authorized by this inventory.

Nine literal names now have independent transformation mappings: Edge; canonical
Gaussian and its four aliases; Value Invert; Maximum/Minimum RGB; Threshold Alpha.
The original name, ordered argument types/values, context descriptors and raw
bytes remain distinct from the execution options. No name is rewritten to a
GEGL operation and no executor is launched from GEGL evaluation.

## New exact-byte mappings

- `plug-in-vinvert`: three original context slots, no extra arguments; RGB only.
  Integer HSV-value shortcut, tied maxima and old alpha-zero hidden RGB retained
- `plug-in-max-rgb`: three context slots plus INT32 flag; RGB only. Positive
  selects the maximum, zero/negative the minimum; ties remain set. Noncanonical
  signed flags are not normalized in the saved definition
- `plug-in-threshold-alpha`: three context slots plus INT32 threshold; RGB/Gray.
  Strict `threshold < alpha` comparison, including out-of-range integer values

`legacy-filter-points` contains 40 outputs produced by the actual pinned old PDB,
with input PNG load/export equality, executable/source hashes and retained logs.
RGB without alpha, RGBA, Gray-alpha, 1×1 and 67×66 cross-tile images exercise low and
zero alpha and noncanonical arguments. All 40match both independent packed/raster
paths and actual GIMP FilterLayer transfer. These add to the existing 457 genuine
old byte buffers; this does not assert 497 separate native projection oracles.

## Explicit modern precision extension

GIMP 2.8 supplies no high-precision or linear/perceptual storage oracle. The new
route therefore has its own conversion and arithmetic contract:

1. Nonlinear U8 keeps the existing byte executors and byte rounding unchanged.
   Other RGB/Gray component types/TRCs are read as straight RGBA/YA doubles in
   the drawable's own Babl space and TRC. Gray replicates/extracts its native Y
   channel; it is not converted to RGB luminance. No forced U8 conversion occurs
2. The independently owned worker representation is 32 bytes per pixel. Byte
   vectors are transferred by memcpy to/from aligned double objects. Workers
   receive no GObject, GEGL node, drawable, UI callback or borrowed slot
3. Inputs must be finite, with alpha in [0,1]. Edge uses the same masks, wrap
   modes and amount placement, without byte truncation, saturating output
   channels to [0,1]. The old alpha-zero hidden-color rule remains
4. Gaussian preserves radius-to-sigma, vertical-before-horizontal order,
   disabled/negative-axis region rules and method selection/small-radius
   fallback. It premultiplies/separates alpha in double precision, without byte
   rounding. The IIR approximation is explicitly normalized to unity DC gain;
   without that normalization a constant image brightens slightly at high
   precision. This normalization never applies to the byte route. RLE retains
   integer Gaussian weights and its historical encoded/nonencoded endpoint
   choice, but removes integer sample rounding. Per-pass channels saturate
   to [0,1]. Extended-range/HDR artistic behavior is not a new compatibility claim
5. Point operations extend their normalized equations without byte rounding.
   Threshold Alpha compares against threshold/255. Native output is converted
   once into the drawable's original precision, so that format's own rounding
   remains. Host lower-stack GEGL operations can themselves use float; a double
   transfer is not proof of bit-exact double precision throughout GIMP
6. Indexed/palette semantics remain unsupported for reruns. Complete loaded
   caches remain usable; unknown names, malformed arguments, nonfinite inputs
   and execution failures keep the definition/cache and stop same-generation
   retry loops

Analytical tests cover point equations, a Sobel ramp, hidden channels, all Edge
masks/borders, constant Gaussian, independent finite RLE impulse weights,
nonfinite rejection, cancellation and a 32-byte transpose roundtrip. Actual GIMP
checks 34 additional component/TRC/base combinations (all six component types,
three TRCs, RGB/Gray, excluding the two existing nonlinear-U8 cases), plus RGB
and Gray 513×257spill-backed Gaussian for both methods and completed-cache rerun.
These are modern extension tests, not fabricated old-runtime evidence.

## Shared scheduler/resource boundary

The existing scheduler/spool gains a checked immutable request stride in 1..32,
with 4 as the backward-compatible default. Chunk offsets/counts remain pixels;
geometry and every transfer size use that request's stride. Invalid stride
fails before reading or launching. Generation changes, cancellation completion,
admission FIFO, priority 150 dispatcher and completed-cache-only publication are
unchanged. Close never joins an independent worker.

The live route spills above 4 MiB of prepared packed samples: one Mi pixels for
RGBA8,131072pixels for RGBA double. Double Edge/point/identity reserves 64 bytes
per pixel for input+result; vertical Gaussian reserves 96 including transpose
scratch. The same trusted config-owned spill pool accounts for both formats.
No XCF argument can choose the path or resource budget. Worker-only temporary
files enforce their declared logical extent and advisory available-space query.
Double transpose uses 256-square tiles (2MiB). Conservative real working-memory
admission includes 16 MiB of bounded transport/transpose/coefficient storage and
128 bytes per maximum-axis sample; small vectors also reserve all raster storage.
GEGL/OS cache, destruction cost and other applications remain outside this pool.

## Editing and active persistence

The common-store GTK editor directly edits all four Gaussian alias forms. The
single-radius forms expose original INT32 axis flags, including negative/truthy
values and both-disabled identity. Two-radius forms retain five slots and fixed
method. Existing FLOAT/DOUBLE types, original context slots and raw bytes remain
intact. Merely opening/accepting does not create a new definition. Explicitly
choosing another entry replaces the definition. Negative radii within the editor
range remain representable; unsupported/out-of-range shapes stay on Keep.

The three point mappings also have editor entries. A copied selection string
and repeated current-selection checks prevent a reentrant stack/sensitivity
notification from applying an obsolete choice to a newer editor state. The
single DialogSlot, weak hooks and factory lifetime leases remain unchanged.

Real RUNNING/IMPORTING Save→close→reopen cases are in
`app/tests/test-painter-xcf-active.inc`, integrated and owned by the XCF workstream.
They must be accepted with that workstream's latest exact-source reports; this
kernel contract alone does not certify native persistence.

## Acceptance and unresolved gates

Normal independent targets and focused native runs are recorded separately.
One full native attempt exposed a Gaussian route variable-shadowing defect;
that failed log is retained, the converter was renamed, and subsequent byte
integration passed. A later combined 83-case run received SIGKILL during the
existing 8193²case, without an assertion or a confirmed OOM diagnosis. 82 other
native cases passed in a separate process, with that one test explicitly skipped;
the 8193²case then passed alone. Do not call the interrupted single process a pass.

The isolated large observation was 18.30 seconds of filter work, 1,363,596 KiB peak
process RSS, 278.6 ms maximum owner quantum, heartbeat p99 5.17 ms/max 1.209 seconds,
and 114.5 ms final image unref. These shared-host numbers leave the latency gate open.
They are not evidence of a universal time bound or whole-application memory cap.

Focused ASan/UBSan passes the six new independent kernel cases (also
float-cast-overflow), 37 scheduler cases and 82 native app cases with 20 instrumented
and 34 RTTI-only source units. Leak detection is disabled. The large ASan case
receives SIGKILL both in the combined run and alone, without a sanitizer
diagnostic or confirmed OOM event. It is an unpassed check. The latest GTK suite
passes 24 normal cases; its previous 21-case sanitizer result is historical and
has not been refreshed for these editor changes at this WIP checkpoint.
`filter-native-foundation-normal.json` and `filter-native-other82-sanitizers.json`
carry the current source hashes and the two retained-handle/image-close tests
used by foundation acceptance entries 06.028 and 07.012.

The full procedure inventory still has 99 rows without an accepted execution route.
External process launch/termination/crash/unresponsive handling and native
GimpProgress remain required, implementable follow-on work. The finite mappings
in this checkpoint cannot be presented as a complete generic FilterLayer port.
