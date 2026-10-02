# Explicit legacy-origin ordinary brush geometry

Pinned original: seagetch/gimp-painter `afa43fae3e920210146abed514f136fd49f671b5`.

## Selection and ownership

`painter-legacy-brush-geometry` is a persistent GimpPaintOptions boolean, false
by default. Only proven-Painter profile conversion materializes it automatically.
Texture, a Painter blend mode, brush name, GIMP version and modern MyPaint are
not provenance. It accompanies the existing brush-property copy, config duplicate,
serialize/deserialize and reset paths. Dedicated Painter MyPaint and Smudge retain
their already separate legacy engines.

The marker is native C GObject private data. The native BrushCore private data
owns its transformed TempBuf cache, with explicit dirty/resource/parameter and
marker invalidation and matching finalization. Public instance/class headers are
unchanged. The C++ adapter is stateless and calls the existing verified legacy
bitmap/generated math helpers. It adds no C++ implementation owner, store or
qdata key.

## Defined legacy behavior

Generated brushes use the original odd dimensions, selected native angle plus
option/dynamic turns, native aspect when the option is zero, the original
absolute-aspect mapping otherwise, and native hardness times the option/dynamic
multiplier. Scaling starts from the original native dimensions, rather than
GIMP3's different bounding box. Bitmap geometry uses the original fixed-point
bilinear transform and blur. Soft and pressure masks use the same original byte
rounding already used by paper. A marked untextured stroke also uses that old
rounding; an unmarked untextured stroke keeps its modern rounding.

Existing native PaintCore buffers, stroke extents, cancellation and single Undo
remain the owner of publication. For supported byte/Painter modes the existing
byte publication adapter applies to marked strokes with or without paper. This
selection does not infer geometry from the mode or paper setting. Other modern
modes and higher-precision publication keep native PaintCore routing.

The original empty/default dynamics curve was constructed with17 control-point
slots and a cubic neutral segment. Modern empty defaults have zero points and
exact-linear samples. Only marked empty smooth defaults recreate the two old
sample values needed by each lookup; existing curves with points and freehand
curves keep their own sample tables/identity semantics. No shared curve is
mutated. The actual old256-sample table and1,028 mapped-value checks establish
this distinction, which otherwise changes a few final pressure bytes.

RGB pixmap channels reuse the original bitmap kernel. Marked pixmap publication
retains the old wrapped paint-area coordinates and raw-mask alpha for soft
brushes, and refreshes that area every dab. Its48 actual old full strokes cover
soft/hard/pressure, four transforms, dynamic settings and texture on/off.

Legacy GUI-linked generated angle/aspect defaults remain zero option offsets;
the native generated brush supplies its own values. The one-shot profile
hardness bridge resolves to multiplier1 for marked geometry, avoiding a second
multiplication by native hardness. Unmarked options keep their modern absolute
hardness normalization. Explicit numeric values remain explicit.

GIMP3 reflection is an extension applied to the complete legacy stamp; it is not
claimed as a pinned-old feature. Nonfinite or unrepresentable transform input is
rejected rather than relying on invalid integer casts or allocations.

## Verification gates

The 288-scene oracle was captured by linking the unchanged pinned old archives
before integration. It records real native Paintbrush initial/finish/Undo/Redo,
six bitmap/generated shapes, four scale/angle/aspect combinations, three mask
modes, dynamic size/hardness/angle/aspect and paper off/on. Four channel layouts
and constant/incremental application are distributed across those cases.
The original unmarked GIMP3 executable independently captured the same288 scenes
before implementation, including selected paper and Painter modes.

Normal and focused ASan/UBSan/vptr proof pass336 exact old full strokes,
all2,592 actual old transformed mask records, all288 untouched-modern scenes
and seven native property/reset/default/cache/invalid-domain/dynamics cases.
Real GTK profile integration passes nine normal and nine instrumented tests.
The focused build instruments26 units and separately rebuilds37 production
C++ units for compatible RTTI/vptr metadata; it does not claim full-host
instrumentation. LeakSanitizer is disabled in this environment.

Existing ordinary paper/mode, queued Smudge and three-route Fill regressions,
five dynamics tests and all four registered geometry Meson targets pass.
An additional normal-only oracle captures144 two-spike generated strokes and
native axes at0.0625 degrees. It rules out a suspected rounding-name difference:
GIMP3 deliberately defines RINT as floor(x+.5), matching old ROUND for valid
positive native angles. No speculative production correction was made.
No real tablet or other-platform proof is implied by these comparisons.
