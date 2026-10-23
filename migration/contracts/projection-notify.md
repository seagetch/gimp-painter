# 09.013 — projection notification to the actual canvas

Original action: projection 更新通知を接続する. Acceptance: 変更領域が表示へ伝わる.
Dependency: 09.012.

Native Painter input now publishes cancellation rollback to projection/display,
not only the restored drawable buffer. Actual authenticated portrait Xvfb with
oncanvas UI enabled verifies blue→live red→canceled blue→committed red through
raw root-window captures and an independent canvas pixel check. Production
updates must reach the projection notification and cover the changed point.
No fixture-side flush occurs between live/cancel input and their captures.

47 native TAP tests, a focused15-unit sanitizer UI test, and three actual
production notification mutants substantiate this bounded criterion. Causal
pre-fix buffer-restored/canvas-stale receipts and failed setup diagnostics are
retained separately. See migration/tests/projection-notify/README.md.

The entire429-line legacy DrawableFeature duty remains TODO. The explicit
09.013/gegl-graph-invalidation child retains exact source-node/cache reevaluation,
offsets, all graph consumers and CloneLayer/FilterLayer integration; this default
byte-layer UI case does not close those obligations. All inherited source duties,
engine/stroke-state obligations and reverse ledger links remain represented in
projection-notify.json and the proposed task/progress patch.
