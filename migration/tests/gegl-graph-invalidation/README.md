# 09.013/gegl-graph-invalidation — actual native graph damage

Original criterion (unchanged): **変更 ROI と座標を source node・投影へ通知し画面更新と再評価範囲が一致**.

This is a bounded verification of existing native production routes, not a new
painting implementation or a whole-current-checkout certification. Six model
cases and three real rendered application cases pass normally and in the existing
ordinary native ASan/UBSan build. Leak detection is disabled. Two compiled
production mutations fail; the restored six-case model suite passes.

## What is measured

- Actual `GeglBuffer::changed` (using GEGL's signal-enabling API), drawable source
  node `invalidated`, and native projection `update` rectangles.
- Actual projection graph `gegl_node_blit` / `gegl_node_blit_buffer` calls, observed
  by forwarding linker wrappers. No replacement tile-handler vfunc or fake graph.
- A 7×9 local edit at (70,65), item offset (17,23), gives image damage
  (64,64,32,64), independently computed from the documented 32-pixel projection
  update grid. Isolated group projection quantizes locally first, then translates
  by its offset and quantizes again: (64,64,64,64). Negative item offsets and a
  retained hot tile are separate cases. Validation stays inside that damage;
  reading a different clean tile does not evaluate, nor does a second clean read.
- Clone at offset (201,101) receives the exact local 7×9 copy and image damage
  (256,160,32,32). After source detach, edits to the old source produce no Clone
  buffer/source/projection damage or evaluation; the committed cache is retained.
- Gaussian radius1 (never Edge) commits a **new** complete result buffer. The old
  retained buffer remains unchanged and emits no mutation signal; the source
  node receives full-result invalidation and projection receives the independently
  quantized complete extent. Querying the projection does not start another run;
  an upper-layer update does not run the Filter. This is full-cache invalidation,
  not a claim that Gaussian updates only the incoming pixel ROI.

The first outside-pixel diagnostic shared a 128×128 tile with the dirty ROI:
reading it legitimately validates that tile's dirty portion. It was corrected to
use a genuinely different tile; no global no-outside-pixel-evaluation claim is
made. Group and Filter diagnostics preserve respectively the discovered nested
quantization and atomic replacement behavior. The X-only mutation fell within
the same grid cell and was ineffective; it is preserved and **not counted**.
Dropping the Y translation is a meaningful failing origin control. Omitting the
previously committed hot-tile flush yields stale pixels and fails separately.

## Real UI evidence

`graph-{0,1,2}-{before,after}.png` are unedited root-window captures from an
actual app under authenticated loopback Xvfb, **1080×1920**, with oncanvas UI
explicitly enabled and asserted. 0=ordinary, 1=Clone, 2=Gaussian. Each raw canvas
sample changes blue→red and its projection update covers the sample.

Input is disclosed as **direct native buffer edit → drawable_update → image_flush**,
not a synthetic mouse gesture. Only event-loop processing (and awaiting the real
Gaussian scheduler) occurs before the after screenshot. Pickable/display forcing
is used solely for initial baseline readiness, never after the edit. Original
09.013's separate real MyPaint-tool stroke/cancel proof remains unchanged.

## Source and obligation limits

The native dependency snapshot is ce40ac4 plus committed navigation changes and
exact committed `gimpprojection.c` hot-cache correction. `tested-sources.tar.gz`
contains75 explicitly hashed relevant sources. Twelve files intentionally use
immutable ce40ac4 dependencies, listed in `dependency-checkpoints.json`, including
all existing held production paths; the held Bucket header remains absent. No
held candidate source/test was imported or executed. Newer features/Filter scalar
routes, Painter blend modes, and current whole-app integration are not certified.
No production file is changed by this task.

All **19** original origin-aware update hunks are preserved verbatim in
`original-duty-snapshot.json`; their audit status and implementation TODO are not
conflated. They retain their existing other17/18-series implementation owners.
Global colormap/component/crop/view-policy behavior and whole old signal ABI are
not closed by local ROI tests. The inherited complete429-line DrawableFeature
obligation remains TODO, copied unchanged in `inherited-drawable-duty.json` with
its other successor owners. This bounded graph criterion does not close any
entire original hunk, Filter/Clone parent, paint-composition, or31.017 obligation.

Run `python3 migration/tests/gegl-graph-invalidation/verify.py --check-owned-source`
and `python3 -m unittest tools.tests.test_gegl_graph_invalidation_proof`.
Strict `--check-source` deliberately rejects the12 immutable dependency/main
mismatches, rather than falsely claiming whole-current-source execution.
