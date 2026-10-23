# Actual legacy layer operations (02.011)

Pinned GIMP-Painter: `afa43fae3e920210146abed514f136fd49f671b5`.
These are two headless original-core scenes with synthetic inputs, captured
2026-10-15. They are not physical UI actions, real artwork, or proof of GIMP 3
parity. Both runs and their fresh-profile repeats exited 0.

## Captured coverage

- Clone scene: 30 saved states, 10 observed objects, 268 raw pixel records.
  Same-size source translation; clone translation; source stack reorder;
  direct clone duplication; whole-group duplication with an internal source
  and a shared external source; independent source/copy/external pixel edits;
  source deletion, restoration and further editing; clone deletion; copied
  group deletion. Each structural operation includes ordinary Undo and Redo.
- Filter scene: 17 saved states, 3 observed objects, 61 pixel records.
  Source and FilterLayer translation; FilterLayer duplication; deletion of
  lower source and duplicate filter; ordinary Undo/Redo. Every attached filter
  snapshot has completed original `plug-in-edge` execution and pending=0.
- Every state saves an editable XCF, image projection RGBA, each live watched
  layer's raw pixels, pointer identities, stable capture labels, core item IDs,
  parent/stack position, attachment, geometry, cumulative update counts, clone
  source pointers, full recursive Undo/Redo object/type/item records, and
  FilterLayer procedure plus all six typed arguments. Logs preserve update
  rectangles and property notifications in observed callback order.
- The repeat's 329 pixel hashes exactly match the primary capture. Repeat
  logs and comparisons are included; primary raw bytes are the sealed outputs.
- The unchanged installed legacy reader reopens five clone-scene XCF states.
  These are dimensions/root-count smoke observations, not reconstructed source
  relationship or render equivalence checks. Four FilterLayer XCF states
  reproduce the known old-reader segfault (exit 139), not a successful round trip.

## Measured outcomes and caveats

Direct clone duplication retains the original source pointer. Group duplication
remaps its internal clone to the copied source; its external clone retains the
original external source. Original/copy edits update only the appropriate clone
branch; an external edit updates both external clones. Same-size source movement
does not move the clone. Source delete retains the same pointer while detached;
Undo reattaches that object, Redo detaches it, and editing after restoration
updates both the original clone and its direct duplicate.

The old group duplicate emits `gimp_drawable_project_region`'s attached-item
critical while remapping a not-yet-attached source. It is retained verbatim.
The process still completes; subsequent explicit insertion/rendering yields the
recorded pixels. No legacy implementation was corrected to suppress the warning.

Filter duplication preserves procedure/scalar parameters but gives the new
execution its own GimpDrawableID, recorded against each object's core ID. Its
cache is recomputed from its current lower stack, which now includes the original
filter. Deleting/restoring the lower source changes the filter caches and image
projection; Redo reproduces deletion's recorded pixels. Detached filter objects
remain alive in Undo history with pending=1 at several states. Those are detached
work requests, not claimed successful processing or idle completion. Reattaching
via Undo/Redo processes them again. The full records retain the actual results.

The two startup GValue criticals and Script-Fu shutdown pipe warning also remain
in each complete log. Initial exploratory captures omitted projection sampling
while waiting; they are preserved separately in the isolated workspace and are
not the accepted evidence. The final harness samples projection tiles while
servicing the event loop (2 seconds for filter; 0.1 second for clone). This is an
explicit observer/render step, not a claim about unobserved instantaneous state.
The snapshot is taken only after that bounded interval; pending is reported,
never overwritten or inferred from elapsed time.

## Inputs and source boundaries

The clone source/external are opaque 24x20 rectangles in a 96x80 RGB image,
initially at (8,8)/(60,40), with RGB (204,51,102)/(51,76,204). The group contains
that source, an internal clone at (36,8), and an external clone at (36,36).
The filter input uses the same source rectangle and an original selection/edit
fill of a 10x8 inset at image (14,13), color (26,51,204). The edge arguments are
run noninteractive, amount 2, wrap 1, algorithm 0. Exact construction and ordered
calls are in `capture-layer-operations.c`; `ACTION` lines identify each API.
Simple recolor probes use the old no-Undo fill/update calls; they are not claims
of paint-history coverage. Undo/Redo covers each recorded structural operation.

The complete source overlay includes inherited compiler/GTK compatibility fixes,
original FilterLayer logging/read-only observation, and this test entry point.
CloneLayer, group duplication, layer/item edits, Undo and XCF implementations
remain byte-identical to the pin (see `source-provenance.json`). Read-only signal
observers hold no extra object references. Weak-finalization callbacks guard the
observer table; normal image/Undo ownership controls lifetime. No ptrace, preload,
interposition, runtime injection, or security configuration change was used.

## Reproduce and validate

See [REPRODUCE.md](REPRODUCE.md) for exact staging layout, commands, exporter
dependencies, and the distinction between checking this old seal and exporting
a new measurement. The exact exporter and provenance helper are included.

Use the dependencies/configuration in `migration/baseline/legacy/` and the
20261015 replay. Apply `source-overlay.patch` once to the pinned tree; do not
reapply its constituent overlays. It passes `git apply --check` against clean
pinned files. The original capture reused the previously built compatible
archives in its own copied isolated build; `build.sh` relinks its changed test
entry point. A fresh source build must first build the core/libraries using the
baseline recipe. Paths in scripts record the actual workspace and dependency
prefix, and need relocation on another machine.

```
bash legacy-layer-operations-20261015/build.sh
bash legacy-layer-operations-20261015/run.sh
bash legacy-layer-operations-20261015/run.sh repeat-captures
bash legacy-layer-operations-20261015/independent.sh
python3 -m unittest discover -s migration/tests -p 'test_legacy_layer_operations_fixtures.py' -v
```

`capture-report.json` records binary hashes and process/state scope.
`source-provenance.json`, `source-overlay.patch`, `dependencies.txt`, build logs,
raw logs, XCFs, pixels and `manifest.json` provide replay/integrity provenance.
The installed reader hash remains
`85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1`.

Remaining gaps: physical UI input, arbitrary/nested cross-sibling group graphs,
cycles, final disposal after history eviction, masks/size-changing transforms
(already separately bounded by legacy-clone-undo), broader filters, FilterLayer
successful old-reader round trip, GIMP3 replay equivalence, and detailed timed
execution-order acceptance (02.013). This package does not mark those complete.
