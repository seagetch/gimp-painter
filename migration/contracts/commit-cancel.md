# 09.012 — native pixel commit/cancel

Original acceptance: **cancel と commit で正しい画素が残る**. Dependency: 09.011.

The actual PaintCore/GeglSurface/native GimpPaintCore transaction is exercised
across byte, float and double buffers, incremental and floating composition,
and eight exit paths (48 cases). Every case starts with a retained Undo item
and a redo branch. Two separated logical segments change the full native buffer;
commit preserves exactly those bytes and publishes one Undo item. Cancellation,
external finish of an atomic batch, configuration change, invalid input and
controller destruction restore the complete prior bytes without deleting redo.
An empty committed batch leaves bytes/history unchanged. Retried operations
remain undoable/redoable; preview thaw, inactive state and repeated finish/cancel
are checked. Existing interactive/reentrant/native RGB tests remain registered.

This is native model evidence under authenticated 1080×1920 Xvfb, not a rendered
UI screenshot or displayed projection claim. It does not establish historical
engine/stroke-state restoration. The entire 429-line legacy DrawableFeature
source duty remains TODO, with unchanged source identity and explicit ancestor,
successor and reverse-ledger links; 09.011/mypaint-undo-stroke is retained.

Evidence records full normal and selected 11-unit ASan/UBSan/float-cast-overflow
runs; dependencies, whole application, vptr and leak detection are excluded.
Native production mutations must reject omitted rollback, rollback-on-commit,
and incorrectly committing an atomic batch through external finish.
