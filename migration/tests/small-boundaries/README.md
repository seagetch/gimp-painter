# 09.016 bounded native buffer boundaries

New actual PaintCore tests cover72 combinations:1×1,2×3,17×9 image/layer sizes;
negative/zero/positive layer offsets; byte and double precision; RGB/Gray; alpha
and opaque storage. Negative drawable-local brush centers overlap edges, then
all corners are painted. Actual update rectangles stay within the native extent,
pixels change, precision/extent remain identical, and native Undo/Redo restore
complete native bytes. Existing TempBuf6 and buffer-read8 groups exercise actual
production C TempBuf/pixbuf code and padded read strides/guard bytes across144
formats. Together with all existing Surface cases there are37 native TAP groups.

Selected fourteen-unit instrumentation passes the72-cell group plus TempBuf6;
standalone BufferRead8 passes ASan/UBSan separately. Native vptr/LSan and dependency
instrumentation are not claimed. Two actual production mutations are rejected:
discarding one-pixel-wide intersections and replacing padded destination stride
with packed row width. Native sources are restored and the tiny matrix reruns.

The first fixture incorrectly fed image-space offsets into PaintCore, whose API
consumes drawable-local coordinates (the native tool performs translation). Its
excluded no-paint failure is retained. The corrected fixture explicitly includes
negative local inputs and independently varied layer offsets; no production code
changed. These are model/native tests under portrait Xvfb, not visible UI evidence.

## All original obligations retained

The69 source-specific inventory duties remain whole-hunk TODO, with exact original
action/acceptance, source identity and full-file pinned hash. Each has an explicit
successor retaining its *unchanged original boundary criterion*, not merely a
cross-reference to these generic native tests. PixelRegion descriptors/iteration,
five-region serial/parallel dispatch and all consumers remain31.017 (plus prior
owners). The34 contiguous-region search duties remain actual search work under
27.004/gegl-bounded-search,27.006 and27.010: generic BufferRead is not an
implementation or boundary proof for that algorithm. Original DrawableFeature
and engine/graph children remain independently owned. No inventory row is closed.

## Source checkpoint and concurrent work

After native/sanitizer/mutant runs,45 exact source files were frozen. Another
worker then began separately authorized Engine state work. The three Engine files
match the explicit committed checkpoint in report.json; the archive is not
resealed to that new untested implementation. --check-source intentionally fails
if current sources differ. --check-committed-engine-checkpoint verifies those
three exact committed blobs and all other current owned sources, explicitly
without claiming the unfinished Engine changes passed. Final aggregate/current
Engine proof is a separate required successor gate. No paused popup/preview or
canvas implementation was compiled by this test job; native tests do not create
those UI features.
