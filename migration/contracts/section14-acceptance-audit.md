# Original section14 implementation/gap audit

Original criteria below are unchanged. Existing implementation and bounded
receipts are distinguished from missing coverage; no task is closed by this
source audit. Original09.017 is now DONE;08.020 and the unchanged section14
prerequisite chain still govern closure.
The initial three model probes did not establish a production defect. A later
source-duty audit did establish missing Clone drawable-Undo suppression; its
causal red and narrow policy repair are recorded below.
The correct next step is precise acceptance reconciliation, not rewriting already
working contracts merely because their parent ledger rows still say TODO.

| Task | Original criterion | Existing implementation / concrete gap |
|---|---|---|
|14.001|旧仕様に沿う参照が close で解除される|Core WeakRef/Connection ownership and image-disconnect handlers exist; source_lifetime_and_detach, freeze_and_close, owner-image tests. Dependencies08.020/09.017/10.018 are not waived.|
|14.002|探索順と同名の解決が fixture と一致|recursive_first_match/deferred_name and source-backed name-resolution contract exist; preserve original depth-first same-name rules and stable-ID persistence distinction.|
|14.003|編集領域が clone に更新される|partial_update_and_graph and component/source-mode regressions already cover cache/source-node ROI. TODO does not mean missing implementation.|
|14.003/gegl-source-node|source の変更 ROI、offset、参照解除が projection の再評価へ届く|Existing source-node tests lack combined image projection/offset/detach observation. New model probe exercises this combination; primary selected pass plus committed clean normal/focused sanitizer (eddac4e650). Detach intentionally retains cache under pinned old contract.|
|14.003/clone-pickable-opacity|旧 gint 返却のゼロ透明を現行 gdouble 契約で保ち、参照画素・子レイヤー表示への影響を試験|gdouble opacity_at returns0.0; existing projected_pixels checks one point. Selected native probe now proves648 zero queries,6 rendered/updated pixels and6 pixel/cycle/bounds assertions for ordinary/group sources. Committed c7b99c9b9a clean selected normal and focused sanitizer pass in clone-pickable-contract/.|
|14.004|子の変更と合成が更新される|duplicate_and_group_update and group reference tests exist. Committed30-stage corpus now compares retained grouped edits/copies against independent model and pixel captures.|
|14.005|移動と resize の見え方が一致|offset_size_and_noop_transforms and source_resize_undo_legacy_fixture cover measured resize/mask history. Source offset alone preserves clone offset under old semantics.|
|14.006|source 座標との合成が一致|Inherited translation plus explicit no-op scale/flip/rotate/transform route tested. New projection probe checks translated location rather than only item coordinates.|
|14.006/editability-contract|廃止された GimpItemClass.is_editable を仮定せず、許可操作と選択・移動の可否を確認|Removed is_editable is not referenced. content_locked alwaysTRUE; inherited position lock/translation. PDB/selection/movement probe passes. Four old image-Undo source duties exposed missing derived-cache suppression, repaired specifically for Clone DrawableUndo descendants before dirty/redo publication, including prepared/frozen paths. Ordinary user-content-locked layers remain supported. See clone-editability-contract/ for scoped evidence.|
|14.007|旧版の更新範囲と表示が一致|DONE component visibility and source-mode child regressions cover old27 non-Dissolve modes, all component formats and delayed ROI. Dissolve has its own captured fixture test; do not substitute child completion for whole parent.|
|14.008|既存参照を旧仕様に沿って維持|reference snapshots/name watchers and renamed-then-expired history tests exist. Typed stable IDs preserve live identity despite rename; old lazy-name behavior remains distinct.|
|14.009|保存前後で別レイヤーへ変わらない|recursive-first-match and persistence stable IDs exist. Same-name resolution versus existing reference identity stay separate. clone_duplicate_name_identity in committed painter-float-xcf normal/sanitizer receipts proves chosen stable source across two roundtrips and post-reopen edits.|
|14.010|複製先の source が旧仕様と一致|duplicate_and_group_update and cross-image-copy tests exist, including source policy and floating ownership.|
|14.011|内部・外部参照の扱いが旧版と一致|group_duplicate_internal_reference and group_duplicate_complete_hierarchy remap internal references; genuine RGB cross-image comparison exists.|
|14.012|旧挙動を保持し stale pointer を残さない|source_delete_undo/source_lifetime_and_detach and retained-item tests exist; detach keeps last cache, not transparent substitution.|
|14.013|旧接続解除と新接続が一度ずつ行われる|New clone_source_connections_exact_once proves104 actual handler counts over three A/B replacement cycles and detach, plus live/retired update routing; selected clean normal/sanitizer pass. No unrelated paused callback tests invoked.|
|14.014|旧版で有効な参照を壊さず無限更新を防ぐ|cycles test and bounded updating/reference-generation logic exist; source-backed legacy cycle acceptance must not become blanket rejection of previously valid references.|
|14.015|source と参照状態を復元|DONE reference-undo child covers live/pending/expired/none and nonmutating snapshot. Legacy setter still has no implicit history; opt-in API is separate.|
|14.016|復元後に追従が再開|source_delete_undo and measured resize/reference histories exist; the committed30-stage corpus now includes retained delete/restore operations.|
|14.017|遅延通知が clone を触らない|image-disconnect invalidation and retained-item tests exist. No need to execute paused popup/canvas/MyPaint lifetime scopes to establish Clone-only criterion.|
|14.018|コピー・移動の旧意味と矛盾しない|DONE genuine RGB cross-image comparison19 records plus37 scoped tests. Other image formats/operations are not automatically covered.|
|14.019|位置・階層・参照・複製・削除の全 fixture に一致|Committed fab14bc3e5 replays all30 retained Clone-operation stages,238 object-model rows and268 independent raw pixel records, clean normal plus focused sanitizer. This is live operation parity, not all saved-file reopening. Original13.014 is now DONE;14.018 prerequisite remains.|
|14.020|再読込後の source 編集が反映される|Committed clones_edit_save_reopen normal/sanitizer receipts use two genuine old Clone files, prove source edits after reopen and identity/offset/opacity/pixel preservation after a second roundtrip. Original12.016 dependency remains; all possible artwork is not invented as an additional criterion.|

## Priority findings

- Source ROI already reaches the drawable source node. The added narrow probe
  tests actual image projection at shifted clone coordinates, neighboring pixel
  preservation, translation and detach cache retention. It is not a new source
  node implementation and must not be reported as one.
- Pinned old `get_opacity_at` returns integer0 (`afa43fae`, old Clone source
  lines451–455); the current slot returns double0.0. This controls picking, not
  whether the clone cache renders. Current pixel and bounds picking intentionally
  differ. The old comment about child content is not a requirement to turn a
  CloneLayer into a group type.
- Pinned old `is_editable` returnsFALSE (lines476–478). The current replacement
  is content locking, not position locking. Explicit selection and movement stay
  allowed; current PDB content mutation is refused. Public lock preflight and
  low-level mutation APIs must not be conflated.
- New focused receipts will be packaged one original task at a time. Clean
  crossdomain proof remains separate from any explicitly scoped primary probe.

## Source-duty inventory checkpoint

The original work inventory retains106 section14 work rows from38 source hunks,
including delegated shared group/container/item/Undo behavior. None is removed
or automatically closed by the existing native corpus proof. Source mapping
must distinguish Clone behavior from unrelated renderer/popup lifetime duties;
paused platform scopes remain paused. Original08.020 is still TODO and is a
real prerequisite for14.001, not an invented new acceptance condition.

## Concrete Clone history repair

The old image-undo.c duties000338–341 rejected drawable pixel Undo for the old
uneditable Clone type before normal history publication. The old frozen branch
read an uninitialized pointer; that defect is not copied. Native content-lock
preflight alone did not cover low-level public/generic/prepared history pushes.
The causal test accepted an ordinary drawable control then failed because a
Clone drawable push returned non-NULL. The fix rejects only Clone-derived-cache
pixel Undo (including descendants), before dirty/redo changes. It collects
construction values once under image ownership and keeps references alive
through construction; prepared rejection consumes ownership with an explicit
NOT_SUPPORTED error. Ordinary content-locked layers still use native history.
No claim is made that this resolves separate Filter editability duties.

## Current evidence versus historical acceptance limits

`section14-source-duty-map.json` maps all106 original work items and38 source
hunks, with a direct per-task artifact index. Its source hashes are committed
provider snapshots, never hashes of paused working candidates. The historical
`migration/acceptance/clone.json` is retained unchanged: its former missing
component/mode, same-name XCF and anime/corpus observations have newer evidence
indexed here. Its genuinely unmeasured indexed/non-RGB cross-image or broad
artwork scopes are not converted into claims of proof, nor invented as new
original acceptance criteria. Source-directed Indexed branch coverage remains
a useful independent check.

Six old shared viewable parent-change hunks have explicit Filter owners
15.004/15.009/15.001/core-scheduler-edge: the old Clone connection is commented,
while the old active consumer is Filter. Current committed ancestry-changed
emission and Filter topology-refresh mapping are recorded, not claimed proven
by Clone operation fixtures. No paused coreviewable lifetime changes are used.
