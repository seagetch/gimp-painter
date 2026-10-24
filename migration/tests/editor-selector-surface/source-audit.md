# Ordinary shape/paper selector acceptance audit

24.005: ブラシ形状 selector を移植する; dependency24.004; acceptance 選択が押印と採色へ反映.
24.006: 紙目 selector を移植する; dependency24.005; acceptance 資源と動的設定へ反映.

Current widgets/gimppaintermybrusheditor.cpp:647–650 embeds native brush/pattern boxes on sole Options context; no independent editable model. widgets/gimpcontainerentry.c:373–411 resolves typed exact resource names via container selection. Existing options provider sync_resources/resource_changed:158–229 maps enabled+specified named resources bidirectionally; existing Options named-resources covers context/model but not native selector entry nor pixels. Existing resource-late-resolution proves new Stroke owner name resolution, not editor or surface pixels.

Gap: joining supported ordinary selector entry through model, new Stroke-purpose native resource owner, actual stamping and sampling. No missing implementation established. Proposed deterministic independent oracle uses3x3 identity-transformed bitmap masks with single nonzero points; radius1.5/aspect1/angle0 preserves bytes via existing identity transform. Fixed dab center4,4 means top-left2,2. Actual selected mask uniquely determines the one stamped pixel and sampled target color. Two constant papers0/255 gate coverage; scalar grain1 and contrast0 provide exact boundary values. Resource names/options/two views/other settings and curves remain checked, no synthetic second authoritative model. This bounded test does not claim whole Engine dynamic evaluator, arbitrary transforms/highprecision or brush-window lifecycle.

Scope exclusions: no Preview-purpose resource owner, extended preview hooks, popup callbacks, acquisition reentry, generic lifetime/fault diagnostics. Actual native selector ordinary text-change only. All18 shared source duties and original owners retained verbatim in duty-owners.json.

Parent closure trace:24.001 acceptance dock・popup・横型 UI が同一データを操作 depends on19.016,20.018,21.020,22.010 (open chain).24.002 acceptance 独自パラメーターを全編集可能 depends24.001;24.003 acceptance 値と表示が双方向に同期 depends24.002.45numeric/5switch proofs directly support those scalar behaviors; no unrelated Save/menu condition is invented. Two-window proof does not establish untested popup sharing or close actual prerequisites. Curve endpoint/toggle/constant-setting residuals remain24.004 duties rather than invented scalar failures.
