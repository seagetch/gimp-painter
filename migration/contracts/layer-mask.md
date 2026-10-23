# Native layer-mask read — original 09.007 acceptance

Action: **レイヤーマスクの読取りを接続する**. Acceptance: **適用・非適用を区別**.
Dependency: 09.006.

A layer mask is not the image selection. The old DrawableFeature reads raw
layer pixels and a separate image selection. Multiplying layer-mask coverage
into Surface writes would erase the distinction and change both legacy painting
and native GIMP semantics. The current integration keeps raw PaintCore strokes
on the drawable, publishes native drawable updates, and uses GimpLayer's actual
native mask/read/projection paths:

* `gimp_layer_get_opacity_at` reads raw alpha and multiplies the attached mask
  only when `gimp_layer_get_apply_mask` is true. Its native buffer reads request
  A-double and Y-double, respectively.
* The native layer graph connects the mask-offset node to the mode node's aux2
  input only while the mask is applied. `gimp_layer_set_apply_mask` changes that
  connection, updates bounds and publishes drawable invalidation.
* A GimpLayerMask can itself be the PaintCore target. Editing that independent
  grayscale drawable changes the mask, not the associated layer's pixel storage;
  its apply flag still controls whether projection reads use it.

## Native acceptance tests

`test-painter-surface-layer-mask.inc` runs real PaintCore strokes, image/layer/mask
objects, native opacity reads and real projection graph evaluation. Incremental
and non-incremental modes are checked at `(0,0),(-7,5),(9,-6)` item offsets.
Identical raw strokes are compared with mask initially applied and disabled.
Zero-mask locations must still contain changed raw pixels, proving the mask was
not silently treated as an image selection. Mask storage must remain unchanged.

After painting, true/false/true apply transitions are read through both native
GimpPickable layer opacity and actual image projection, using an independent
raw-alpha × mask-byte oracle. Native GEGL nearest sampling can traverse floating color conversion even when
Y-double output is requested (white is not necessarily bit-exact1). A separate
native group exhaustively checks all256 byte values: A-double error≤1e-15,
Y-double error≤2×FLT_EPSILON. Layer opacity therefore retains the independent
ideal product oracle with bound2×FLT_EPSILON+1e-14, not a copied color converter
or an assumed exact-double sampler. The byte-precision
projection has an explicit 1/255 final-alpha-rounding bound. The test does not
use one native read implementation as the other's oracle. It also paints the
mask independently in both apply states and verifies unchanged layer storage,
unchanged apply state and thawed native preview ownership.

The acceptance test exposed stale projection pixels after an apply transition:
native layer and image graphs were correct, but the shared GEGL hot tile bypassed
lazy validation. The synchronous GimpProjection pickable flush now retires that
cache via public gegl_buffer_flush only when a pending update region existed.
It does not render the whole projection or introduce a Surface selection mask.
The public API may also flush backing storage; this bounded fix makes no new
performance claim. This is a pixel/read criterion,
not all mask-edit UI controls, show-mask behavior, arbitrary filter graphs,
full projection invalidation coverage (09.013), or a full-app sanitizer claim.

## Preserved whole-header work

The assigned source is the complete 429-line legacy DrawableFeature header.
Its exact work ID, source ID/range/blob/hash remain in the JSON contract, and its
whole-hunk checkpoint remains TODO. All original 09.003/09.004/09.006 obligations
are retained verbatim with explicit existing owners for the other read/write,
selection, snapshot, allocation, undo, projection, TempBuf, lifetime, sampling
and non-incremental duties. 31.017 retains whole-consumer replacement. Completing
native layer-mask apply semantics does not complete the entire header.


## Regression evidence and limits

The preserved pre-fix diagnostic reports projected alpha 0 instead of 180/255;
both direct graphs returned 180/255, invalidation/update signals fired, and
public buffer flush restored 180/255. Diagnostic intervention is absent from
the final acceptance fixture. An independent actual-C tile-validator probe
reproduces blue→red graph mutation returning stale blue until buffer flush.
Normal and focused ASan/UBSan probe builds are recorded; dependencies are
uninstrumented and LSan is off. This does not sanitize GimpProjection or the app.

The native aggregate includes Surface18, TempBuf6, coordinate/write7 and
projection-bounds3 and the unchanged historical MyPaint tool24 (58 TAP cases). Three expected-red mutations remove the
apply-state check, misuse the layer mask as image selection, and retain the
stale hot tile. Each mutation is restored and the final aggregate re-run.
The projection-bounds suite is a regression check, not closure of all09.013.

Primary implementation audit used pinned GEGL0.4.62:
[hot tile retirement](https://github.com/GNOME/gegl/blob/GEGL_0_4_62/gegl/buffer/gegl-buffer.c),
[public flush](https://github.com/GNOME/gegl/blob/GEGL_0_4_62/gegl/buffer/gegl-buffer-access.c)
and [lazy tile damage](https://github.com/GNOME/gegl/blob/GEGL_0_4_62/gegl/buffer/gegl-tile-handler.c).
The exact native mechanism, rather than these links alone, is verified by the
packaged executable probe.

The pre-existing 234-line `test-painter-layer-mask.inc` and its three MyPaint
tool cases are preserved unchanged. New cases use the separate
`test-painter-surface-layer-mask.inc` to avoid a fixture-name collision.
