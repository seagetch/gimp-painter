# Genuine Max RGB capture tooling

The genuine old-runtime base (218 scenes), large (2 scenes), and projection
history (16 scenes / 48 runs) captures passed on 2026-10-16. Their independently
validated archives are now sealed by the explicit pins in
`tools/check_max_rgb_evidence.py` and
`tools/check_max_rgb_projection_evidence.py`. The receipt is
`migration/tests/max-rgb-capture-evidence.json`.

This is old-runtime evidence, not native-owner, XCF-consumer, or whole-port
acceptance. No modern-port result, generated expected raster, or historical
point-only fixture substitutes for these captured bytes. A first attempt failed
before any case because a relative plug-in symlink was resolved within the
isolated profile; it is rejected, and the fixed absolute-target behavior has a
regression control. Fresh successful captures supersede that attempt. Known old
startup diagnostics are retained: two GValue object assertions and one missing
tag-cache message per capture; no warning-free legacy-runtime claim is made.

## Oracle and safe executable domain

The original source revision is
`afa43fae3e920210146abed514f136fd49f671b5`. Preparation verifies the full commit
and hashes every listed transformation, iterator, context, merge, serialization,
and observation-hook source against `git show` before writing output. Runtime
source and executable hashes are checked again after capture. The original
Max RGB executable, point callback, region iterator, replacement merge, writer,
and parent projection-copy algorithm are not rewritten.

Only private copies of three old core source files receive observation hooks.
The projection supplement adds a pre-copy observer to that private core copy.
These hooks record actual owned pixels, shadow pixels, masks, run boundaries,
merge entry/exit, and parent rectangles. Controlled late-context mutations
happen synchronously inside an actual hook. They do not replace the callback
or generate expected output pixels.

The noninteractive callback unconditionally reads three color bytes. Its safe
capture domain is RGB or RGBA only; Gray is not called even as a rejection
probe. Four actual typed arguments are checked: run-mode, image, drawable,
and signed INT32 max-p. Mode > 0 chooses maximum channels; mode <= 0 chooses
minimum channels. All tied chosen channels survive, alpha is copied, and raw
hidden RGB is transformed before the separate old REPLACE_INTEN merge. The
checker independently validates both stages, including hidden-color retention
by zero-alpha replacement, component masks, fractional selection, and byte
rounding. Source patterns deliberately include pair and three-way ties,
black/white extrema, unequal channels, and zero/fractional/full alpha.

## Finite matrices

- Base: 218 scenes, 94 synchronous PDB and 124 live FilterLayer scenes. Live
  scenes produce 172 final comparisons, including 14 changed-source reruns and
  34 byte-identical lower-source notifications. Fourteen genuine writer saves
  cover RGB/RGBA lower sources and modes INT32_MIN, -2, -1, 0, 1, 3, INT32_MAX.
- Large: 2 live scenes at 1031×1021, with RGB mode 1 and RGBA mode -2, each with
  a byte-identical lower notification. Both require an observed mid-work
  context mutation and produce 4 final comparisons. This captures spill-size
  inputs; native spill execution is independently tested by its consumer.
- Combined base/large: 220 scenes, 126 live scenes, 176 live final comparisons,
  14 changed and 36 unchanged lower notifications; 16 actual merge-entry
  mutations and 18 strictly mid-work mutations.
- Projection history: 16 scenes, RGB/RGBA lower source × top/group × positive/
  negative overflow × fresh/seeded cache. Each uses modes 1, 0, 0; only the owner
  moves on run three. All 48 pre-copy caches, lower source snapshots, parent
  projections, copied rectangles, starts, raw shadows, and final outputs are
  observed. The eight seeded scenes use the original mark-as-loaded API and
  require zero initial jobs. They are not actual XCF reopen observations.

The base includes tiny 1×1, 1×7, 7×1 and 2×2 extents, tiled 67×66 extents,
RGB/RGBA, signed extrema and noncanonical modes, soft/hard/empty/outside
selection, all component masks, own alpha lock, negative/asymmetric offsets,
idle context edits, later selection expansion/clearing/outside transition,
late component/lock edits, and min-mode context controls.

Phase 1 means the actual core merge-entry observer after shadow flush, with
`merge-entry-mutate` immediately preceding `merge-before`. Its progress field
is a zero sentinel, not a claim about plug-in progress. Phase 2 means an actual
PDB progress request strictly between zero and one, immediately followed by
`progress-mid-mutate`. Progress 1, image size, or a missing callback never proves
completion or mid-work. A second invocation whose start selection is wholly
outside the drawable has a start/end pair and no merge; validators and consumers
must retain that distinction.

## Commands

From the Max RGB worktree, set absolute paths to the restored original source
and installed prefix. The environment script must be sourced before native
execution. Use a fresh work and output directory for every attempt, including
retries; output/work must be independent and outside the original source.

```sh
source ../legacy-runtime/env.sh
source_dir=$(realpath ../legacy-gimp)
prefix=$(realpath ../legacy-runtime/prefix)

# Pure-Python controls do not execute GIMP or create oracle evidence.
PYTHONPYCACHEPREFIX=/tmp/max-rgb-python-cache \
  python3 tools/tests/test_max_rgb_capture.py -v

# Omit --capture to prepare and pin-check without compiling or running.
python3 tools/run-painter-xvfb.py python3 tools/capture_legacy_max_rgb.py \
  --capture --legacy-source "$source_dir" --legacy-prefix "$prefix" \
  --work /absolute/fresh/base-work --output /absolute/fresh/base --timeout 600

python3 tools/run-painter-xvfb.py python3 tools/capture_legacy_max_rgb.py \
  --capture --large --legacy-source "$source_dir" --legacy-prefix "$prefix" \
  --work /absolute/fresh/large-work --output /absolute/fresh/large --timeout 600

python3 tools/run-painter-xvfb.py python3 tools/capture_legacy_max_rgb_projection.py \
  --capture --legacy-source "$source_dir" --legacy-prefix "$prefix" \
  --work /absolute/fresh/projection-work --output /absolute/fresh/projection \
  --timeout 600
```

The producer owns the shared build/capture flock. Do not wrap it with the same
lock externally. The Xvfb wrapper verifies authenticated loopback-only access;
set TMPDIR through the replay environment as required. Each producer creates a
fresh temporary profile containing only the unchanged Max RGB plug-in. Relative
prefix arguments are resolved before making the isolated profile symlink.

`--case-pattern` can diagnose a subset in a separate output. A subset cannot
pass full-matrix sealing or activate genuine-corpus native tests. Keep failed
attempts separate and report why they were rejected. Changing the producer,
harness, observation helper, or checker during execution invalidates its seal.

After all actual outputs pass and their artifacts are reviewed:

```sh
python3 tools/seal_legacy_max_rgb_capture.py --kind base \
  --source /absolute/passed/base \
  --archive migration/fixtures/legacy-max-rgb-context.tar.xz
python3 tools/seal_legacy_max_rgb_capture.py --kind large \
  --source /absolute/passed/large \
  --archive migration/fixtures/legacy-max-rgb-large.tar.xz
python3 tools/seal_legacy_max_rgb_capture.py --kind projection \
  --source /absolute/passed/projection \
  --archive migration/fixtures/legacy-max-rgb-projection.tar.xz
```

The sealer independently validates full observed evidence and creates canonical
xz archives and manifests with fixed timestamps, ordering and dictionary sizes.
It round-trips every original byte through the bounded archive reader and prints
the three hashes for explicit inspection and pinning; it never fills pins itself.
After pins are set, run the two standalone validators and extract into empty
separate directories for the native owner/XCF consumers.

## Artifact interface

Base captures.tsv uses Threshold's 30 columns with `mode` replacing `threshold`.
The authoritative column list is recorded in fixtures.json. Each case retains
source/input/output, source2/start2/output2 when rerun, start/final/idle masks,
actual run ROI rows, merge input/shadow/mask/output, status and event tables,
and raw artifacts hashed by the capture report. Source RGB has three channels;
live owner buffers always have four. PDB RGB retains three channels.

Old writer files use `.xcf` and the exact extracted `.definition.raw`. Their
capsule is independently inspected from the original file: property 32, length
68, literal `plug-in-max-rgb` plus NUL, and record tags 2/1/10/2/0. Run mode and
signed operation mode are observed at payload offsets 28 and 56. Saved image
and drawable slots are zero-length old ID records, not modern execution IDs.

Projection captures.tsv retains 14 columns with source_channels 3/4; every
owned seed/before/start/projection/shadow/output buffer is RGBA4. Source and
source1/source2/source3 use source_channels. copies.tsv records 20 measured
fields; options.tsv records the actual mode read from the old layer definition
at each start. Native consumers should compare complete captured bytes and
observed rectangles rather than reconstructing oracle output from these notes.

These finite captures do not establish arbitrary precision, huge actual I/O,
process isolation, full-project aggregate acceptance, installed UI behavior,
other operating systems, or whole-port completion.
