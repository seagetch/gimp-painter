# Standard paint, dynamics, context and bucket audit (01.016)

`standard-paint-hunk-review.tsv` contains **306 exact hunk checks in 36 paths**.
It includes ordinary brush-core/options, independent Smudge, dynamics/model and
editor, paintbrush/Smudge tool registration, context fields/enums, mouse and pen
input, and the shared contiguous-region/bucket path. Each check includes the
complete changed-line evidence, old source blob/range, target commit/blob (or
an explicit new-component requirement), concrete implementation tasks and
verification tasks. None is marked behaviorally equivalent.

## Preserved responsibilities

- Standard paintbrush paper: a pattern selected from context, `set-texture`
  signal, owned cached mask, image/brush-mask phase, and application *after*
  subsample/solidify/pressure-mask selection. The `ignore_scale` additions in
  brush core belong to **Smudge accumulator sizing**, not paper rendering
- Dynamics adds `blending-output`, creates/gets/sets/releases an aggregate
  `GimpDynamicsOutput` and adds its enum/name. This is the independent Smudge
  color-blending extension; it must not be relabeled as paper or assumed equal
  to modern Flow/Rate
- Smudge allocates separate accumulation and optional color-blending buffers,
  uses foreground color with dynamic blending-output, controls brush-scale
  evaluation, and temporarily substitutes canvas subwindows. Preserve their
  order, sizes, coordinates and ownership under 23.001–23.006
- Paint options add a circular history queue separately from `use-texture`.
  Ink keeps queue cleanup and an inactive smoothing call. The two actual
  `gimppaintcore.c` delta hunks are whitespace only. C++ header declaration
  glue and disabled branches are recorded without pretending they are active
  new painting algorithms
- Context changes add the extended MyPaint resource/object/name, property and
  signal, serialization/copy, thaw/removal fallback, notification and lifetime.
  The `template` to `template_` edits are C++ readability work, not a new saved
  value. Context bit positions are explicitly tracked instead of copied as
  numerically interchangeable with GIMP 3
- Paint-tool press/cursor changes call `gimp_item_is_editable`; these are the
  custom-layer editability contract, not pressure processing. Motion-buffer
  filtering additionally compares pressure, while mouse events without a
  pressure axis derive pressure from button state
- The device-manager `unique-names` change needs duplicate-device and restored
  setting checks under `30.007/device-name-identity`. A similarly named modern
  container is not a sufficient identity-compatibility test
- Compact/ordinary controls are retained through shared widgets. Inactive
  dynamics-editor destroy experiments are classified separately from live
  connection teardown requirements

## Standard bucket: source-to-target contract

Legacy `app/core/gimpdrawable-bucket-fill.c:186–212` passes the selection channel
into `gimp_image_contiguous_region_by_seed_ext()` and disables the previous
post-search `gimp_channel_combine_mask()` block. Legacy
`gimpimage-contiguous-region.c:314–343` constructs bounded source/mask regions;
`:347–425` accepts offsets, a fixed optional start color and transparency state;
`:559–681` computes distance/coverage; `:685–906` uses that coverage while
traversing bounded scanline segments.

At pinned GIMP 3 `95f6410f25c5186686db7a489d79c1e79187cd41`,
`app/core/gimpdrawable-bucket-fill.c:204–254` starts the seed traversal first,
then intersects its bounds with selection bounds. Its comment identifies
`gimp_drawable_apply_buffer()` as the later mask-data intersection.
`app/core/gimppickable-contiguous-region.cc:132–185` flushes and reads current
pickable pixels and has no source-selection-mask, fixed-start-color or explicit
search-rectangle input. The current API therefore cannot directly stand in for
the legacy bounded traversal.

### Selection is a soft distance penalty, not a universal boolean barrier

The old expression is:

```
max = (*src_mask * max + (255 - *src_mask) * 255) / 255;
```

That expression runs before the antialias/threshold result. Exact C extracted
from the pinned source gives these source-level examples:

| Color distance | Selection byte | Threshold | AA coverage |
|---:|---:|---:|---:|
| 0 | 0 | 30 | 0 |
| 0 | 0 | 255 | 255 |
| 0 | 128 | 30 | 0 |
| 0 | 128 | 100 | 117 |
| 0 | 230 | 30 | 255 |

Thus a zero-selection hole *inside selection bounds* blocks normal low-threshold
traversal, but need not block traversal at the maximum threshold. Partial
selection is not equivalent to a boolean test or simple postmultiplication.
A same-color corridor model reaches only the seed-side selected area at
threshold 30 with the old penalty, reaches both selected ends with global
search followed by clipping, and reaches both at legacy threshold 255.
This qualification preserves the actual old comparison contract; it is not a
permission to discard selection-aware traversal or alter the design's required
compatibility.

The transparent-candidate early return precedes the selection penalty; seed
alpha also affects the select-transparent policy. Bounds are reduced before
traversal, so a seed outside bounds, empty selection, source offsets and a
partial/negative-offset drawable each need legacy runtime fixtures. The legacy
upper-bound clamping combines image and drawable coordinates; do not silently
normalize that behavior without those fixtures.

`27.005/selection-threshold-contract` tracks the full mask/threshold/alpha/offset
matrix, implementation and comparisons. It supplements the selection-barrier
work rather than replacing it with a hard-mask shortcut.

## Verification and limits

- `python3 tools/audit_standard_paint.py --check`: exact per-path/hunk coverage,
  complete changed evidence and pinned source/target contracts
- `python3 migration/tests/test_bucket_selection_contract.py`: six tests pass;
  compile actual source-extracted coverage code with C99 warnings as errors,
  check partial/max-threshold examples, all 256 distances over five thresholds,
  the traversal counterexample, and source transparency/bounds witnesses
- `python3 tools/assign_legacy_hunks.py --check`: all-hunk assignment still
  reproduces after the more precise mixed-file routing

These are source/specification checks and a small connectivity model. **The
legacy GIMP executable, target GIMP full flood-fill, actual image pixels,
mouse/tablet replay and saved fixtures were not executed here**. Those results
remain open in 22/23/27/30/36; this audit does not substitute for them.
