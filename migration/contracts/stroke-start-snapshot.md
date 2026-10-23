# 09.008: immutable stroke-start snapshot

Original action: **ストローク開始 snapshot を実装する**.
Acceptance: **開始前画素を後続変更から分離**. Dependency09.007 is unchanged.

The native GimpPaintCore start allocates an independent undo buffer via
gimp_gegl_buffer_dup. PaintCore passes that already captured buffer into
Surface.begin_session_from for a fresh transaction. Surface retains this
immutable snapshot rather than capturing the drawable too late. Standalone
begin_session captures once with gegl_buffer_dup, not once per dab.

The native handoff fixture starts the actual GimpPaintCore on a real layer,
retrieves its native undo buffer, and mutates the drawable after native start
but before the first Surface dab. U8 nonlinear, float linear and double
nonlinear images run at signed offsets (0,0),(-7,5),(9,-6). Exact packed native
snapshot bytes must stay equal to the initial bytes throughout all subsequent
writes and dabs. Non-incremental composition is compared with an independent
Surface control whose target retained the original blue pixels, including a
second external write before a later first-touch region. Cancellation restores
the start snapshot only within the dirty rectangle; external writes elsewhere
remain unchanged. Native cleanup must thaw the layer preview.

The existing standalone snapshot-isolation fixture is unchanged. Its default
and supplied groups cover byte/double, incremental/non-incremental modes,
multiple tiles, writes before first touch and later touch, live sampling versus
frozen composition, and dirty-only cancellation. Both normal and focused
ASan/UBSan/float-cast-overflow runs use the real production Surface and GEGL;
dependency libraries are uninstrumented and LSan is off. This is not full-app
sanitization or proof of every buffer-lifetime path (owned by region-lifetime).

Two expected-red native production mutations replace native duplication with
an alias and replace the supplied start snapshot with a later drawable copy.
A third isolated mutation aliases the default standalone snapshot and is
rejected by the existing standalone pixel comparison. The restored aggregate retains neighboring mask, TempBuf, coordinate,
projection-bounds and historical MyPaint tool cases. No test is removed.

## Preserved whole-header scope

The complete429-line legacy DrawableFeature header keeps its original work ID,
source ID/blob/range/hashes. Whole-hunk status remains TODO. Read, write,
selection and layer-mask ancestor snapshot obligations are retained verbatim.
All remaining allocation, dirty, undo/commit/cancel, projection, TempBuf,
lifetime, sampling/non-incremental and whole-consumer replacement duties keep
explicit existing owners. This criterion closes content isolation only, not
the whole header. Legacy image undo lazily copied tiles on first touch; the
specified GIMP3 stroke-start criterion intentionally separates the initial
snapshot from intervening external writes instead of reproducing that timing.
