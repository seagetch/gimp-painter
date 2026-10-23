# Image, layer, buffer and display coordinates (09.002)

This is the correspondence contract needed before DrawableFeature buffer reads
and writes. It does not implement those adapters or certify migrated rendering.
The adjacent JSON pins all 13 assigned source duties at legacy revision
`afa43fae3e920210146abed514f136fd49f671b5`, plus eight context sources. Each duty
retains its exact source ID, path, blob, changed range and hunk SHA256, an
individual interpretation and an executable/static evidence category. No source
obligation is discarded because a newer API has a similar name.

## Spaces and boundaries

| Space | Origin and extent | Conversion / ownership of coordinates |
| --- | --- | --- |
| Image | Canvas top-left `(0,0)`; `[0,image_width) × [0,image_height)` | X right, Y down. A layer may extend outside these bounds. |
| Layer / drawable | Local `(0,0)` for ordinary layer storage; nominal size independent of image size | `image = local + gimp_item_get_offset(item)`. Negative offsets are legal. To enter layer space, subtract the offset exactly once. |
| GEGL buffer | Explicit `gegl_buffer_get_extent()` rectangle, potentially nonzero or negative X/Y | Rectangles passed to GEGL are in this coordinate space. Width/height do not imply origin zero. For a drawable buffer, these coordinates are drawable-local and image placement adds the item offset. |
| TempBuf | Caller-defined logical `x,y` plus width/height | The data pointer already points at the first local pixel. Logical placement must not be added to that pointer again. A child retains a clipped logical origin and a parent-relative data pointer. |
| Display / canvas | Canvas widget top-left, GTK logical pixels; viewport allocation is distinct from the image | Scale and pan, then rotation/flip. Inverse: undo rotation/flip, add shell scroll offsets, divide by the independent X/Y scales. Do not use global desktop coordinates or multiply by physical display scale here. |

Integer pixels occupy half-open cells `[x,x+1) × [y,y+1)`; a continuous point's
containing cell is selected with floor, including negative coordinates. A
rectangle `(x,y,w,h)` has exclusive ends `(x+w,y+h)`. Merely touching at a right,
left, bottom or top edge has no intersection. Zero-sized requests are empty;
negative sizes are rejected by the adapter admission contract. Image clipping,
layer clipping, buffer clipping and viewport clipping are separate decisions.
A drawable can retain pixels outside the image, and a viewport is not a storage
boundary.

For future adapters, calculate endpoints and offset additions/subtractions in
signed 64-bit arithmetic before narrowing. Reject inputs/endpoints not
representable by the required native gint rectangle. This is a safety contract,
not a claim that the old C code handles signed overflow safely. The standalone
model exercises INT_MIN/INT_MAX admission without executing undefined legacy
arithmetic. Avoid converting non-finite/out-of-range floating input to integers.

## Storage and partial regions

Native ordinary allocation and resize paths in `app/core/gimpdrawable.c` create
buffers with `(0,0,width,height)` and retain item offsets separately.
`gimp_drawable_real_set_buffer` sets offsets from `bounds->x/y`; it does not
normalize an arbitrary incoming buffer extent to zero. The adapter must inspect
the extent and not infer it from item size. `gimp_drawable_get_buffer` also has a
painting-buffer route; a buffer reference alone does not establish snapshot
immutability or update semantics.

For an image-space ROI `R` and drawable offset `O`, the local request is `R-O`.
Intersect in the selected buffer's space. Byte zero in an explicit packed output
is the requested ROI's top-left, not the clipped ROI's top-left. If the caller
requests abyss padding, `GEGL_ABYSS_NONE` gives zero outside the extent; clamping,
looping and nearest-edge duplication are different policies and cannot be
substituted silently. Writes must be clipped before mutation. The real GEGL
probe exercises a negative-X/nonzero-Y extent, a padded read, a write crossing
the extent, unchanged extent and unmodified row-padding bytes. This is a GEGL
API contract test, not a new DrawableFeature implementation.

Rows are addressed as `data + local_y * rowstride + local_x * bytes`. Rowstride
is bytes, not pixels, and is not necessarily `roi.width * bytes`. Keep allocation
size, accessible extent and output rowstride separate. The legacy constructor
uses packed storage, while `temp_buf_get_rowstride` follows the ancestor.

`temp_buf_subwindow` interprets its arguments in the parent's logical space,
intersects with the parent, and calculates:

    child.data = parent.data
               + (child.y - parent.y) * parent.rowstride
               + (child.x - parent.x) * bytes

Nested children retain the root rowstride. The executable old functions test
four byte widths, negative/nonzero parent origins, inside and clipped windows,
nested windows, and all four touching edges. Empty or disjoint requests are
rejected by GLib preconditions, returning NULL. The observed old free function
returns early for a child and leaks the child wrapper; the harness explicitly
releases that wrapper afterward. This observation neither approves the leak nor
proves modern ownership.

A second, different convention is present in old `PixelRegion`:
`pixel_region_init_temp_buf` stores the TempBuf data pointer unchanged and copies
the caller's region x/y unchanged; it does not subtract TempBuf x/y.
`pixel_region_configure` subsequently adds `y * rowstride + x * bytes` to the
original data pointer for a memory-backed region. Therefore those region x/y
arguments address memory relative to its first pixel. Passing a child's logical
TempBuf x/y as if they were its memory-relative coordinates applies an offset
again and can escape storage. This distinction is source-observed and tested at
initialization; the full old tile iterator is not executed by this checker.

## Multiple regions and masks

The six assigned PixelProcessor hunks add the fifth descriptor/callback route.
Each descriptor retains its own origin, width, height, stride and null status.
Parallel dispatch copies descriptors; serial dispatch passes originals.
Registration forwards all five pointers. Nothing in the dispatch translates a
layer rectangle to image, selection or another buffer coordinates. The compiled
serial p5 function is exercised with distinct origins/strides and a null second
region; parallel descriptor copying/forwarding is checked against pinned source.
Thread scheduling, iterator segmentation and locks remain separate obligations.

The old `GimpImageFeature::get_drawable_offset` reads the item offset, while
`get_drawable_region`, Undo and floating-stroke accessors forward coordinates to
local tiles. Its image selection accessor reads the image's mask. Thus a local
paint ROI must add the drawable offset to address the image mask. A layer mask
and image selection are distinct objects and must not share an assumed origin.
The existing `gegl-surface.cpp` selection path explicitly translates the mask
extent by the inverse offset and its access rectangle by the offset; this is
supporting current code context, not certification of all painting modes.

The scalar expressions in `app/base/pixel.hpp` contain no spatial state. They
neither scale coordinates nor turn an origin into an array index. Their byte
format/arithmetic meaning remains documented by 09.001.

## Display rounding and transformed bounds

Both old and current display macros define `SCALEX/Y` using `PROJ_ROUND`, which
uses `floor(value + 0.5)`. Despite its name and double outputs,
`gimp_display_shell_transform_xy_f` therefore rounds the scaled point before
pan/rotation. It is not an exact inverse pair with `untransform_xy_f` for arbitrary
subpixel input. In particular, -0.5 rounds to 0 and -1.5 rounds to -1. Preserve
this for presentation paths that deliberately select those APIs; do not apply
that rounding to model coordinates or brush input merely because they cross a
screen boundary.

The current private `transform_xy_f_noround` and `transform_bounds` retain
floating precision. For rotated/flipped bounds, transform all four corners and
take min/max. To derive integer coverage afterward, floor the minima and ceil
the maxima, then separately clip if requested. The current viewport function
uses that coverage expansion. The legacy viewport function instead uses two
integer-transformed corners; that historical behavior is not a justification
for dropping rotated corners in a new buffer contract.

The checker runs eight old unrotated zoom/unzoom pairs against current rounded
presentation results. It runs 40 precise transform/inverse round trips and five
four-corner bounds cases through extracted current functions and real Cairo
matrices (translation, rotation and both reflections). The minimal shell carrier
is a test double; it is not a GTK shell object. Physical tablet mapping, GTK event
routing, canvas scale two, monitor scaling and full legacy rotation behavior are
not validated here. No GUI change is made by this task.

## Observed invalidation discrepancy

During this task's baseline inspection, `gimp_drawable_set_buffer_full` added
`gimp_item_get_offset_x(item)` to `old_extent.y`; the equivalent new extent adds
`bounds->y`. For `(0,0,4,3)` at offset `(7,-3)`, the old image-space extent becomes
`(7,7,4,3)` instead of `(7,-3,4,3)`, making a same-bounds replacement appear moved.
This is an axis error independently of whether extent origins are zero.
A separate implementation/regression task owns the fix: retain `old_extent.y`
and add item offset Y, check identical translated extents avoid the old-bounds
pre-update, and check a real Y-only move still triggers it. Recording this
baseline finding does not close the behavioral defect; consult that task's
current implementation evidence. This contract checker does not freeze or
require the defect.

## Reproduction and scope

Source the configured native dependency environment, then run:

    python3 -B tools/check_coordinate_contract.py --report migration/tests/coordinate-contract/native.json
    python3 -B tools/check_coordinate_contract.py --sanitize --report migration/tests/coordinate-contract/sanitizers.json
    python3 -B tools/tests/test_coordinate_contract.py

The checker compiles temporary extracted legacy/current functions with real
GLib, GEGL, babl and Cairo. It does not modify or rebuild native GIMP. Reports
pin harness, source and contract hashes and library versions. Sanitizers cover
only the extracted code and harness; dependency libraries are uninstrumented,
LSan is disabled, and no unsafe legacy overflow input is executed.

The Python tests reject omitted/duplicate duties, changed provenance, omitted
context, origin/unit/interval/abyss changes and incorrect boundary expectations.
An independent set-of-pixels oracle checks 20,736 small rectangle pairs. These
are contract/safety-model tests; production buffer reads/writes, invalidation,
Undo, image-composition equivalence and all 09.003–09.017 acceptance gates remain
open until their own evidence exists.
