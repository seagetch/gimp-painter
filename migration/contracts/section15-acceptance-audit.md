# Original section15 acceptance/source audit

This is a source/evidence reconciliation, not task closure. All52 original work
rows and16 source hunks remain exact, plus six explicit cross-owned viewable
hunks from section14. Original criteria/dependencies and all historical child
receipts remain authoritative. No paused source or old Edge runtime is used.

| Task | Original criterion | Current finding / bounded gap |
|---|---|---|
|15.001|名前と型付き引数を独立に保持|Separate std::string procedure/raw immutable bytes/FilterArguments typed model. Real08.020 predecessor still open; no aggregate closure.|
|15.001/gvalue-deep-copy|元 GValueArray と保存先を独立に解放し、getter の値を正しく返す|Current c_owned_values_and_model_identity exercises12 rich argument slots, both free orders, caller/result mutations, independent strings/nested arrays, exact getters and unchanged saved identity. No duplicate implementation needed.|
|15.001/pdb-temporary-gvalue|変換用一時値を全経路で unset し、引数配列との所有権を分ける|Old pdb-cxx-utils string temporaries are not ported wholesale. Native RAII Value/ArgsRef and checked bounded preparation replace admitted paths; generic arbitrary-PDB compatibility remains separate16/17 ownership. Need precise complete old-site mapping before closure, not merely no old filename.|
|15.002|変換後も保存された元情報を失わない|Raw serialized values and weak object-ID descriptors survive execution mapping; independently owned materialized values do not rewrite originals.|
|15.003|設定の読取り・変更・通知が一経路|Read-only filter-procedure/filter-arguments/filter-original-definition/opaque properties materialize via one get_property and notify only installation/Undo path. Source mapping, not a new complete property matrix.|
|15.004|位置と階層に応じ旧範囲を選ぶ|Current stack snapshot excludes owner/upper siblings and follows actual parent. Historical named receipts used Edge; proposed non-Edge ancestry/stack receipt will not rerun them.|
|15.005|入力と出力 buffer の座標が一致|Inherited offset/size callbacks configure native input/output extents; independent graph invalidation worker owns new ROI/offset proof. Pinned old flip only resets/invalidates, while scale/resize/rotate/transform call parent. Current inherited flip is a concrete geometry/cache mismatch candidate.|
|15.006|独自型でも通常属性が適用される|Native GimpLayer attributes remain inherited. New pickability proof establishes loaded/result projection separately, not exhaustive mask/mode combinations.|
|15.006/gegl-result-source|未完成・旧世代の結果を見せず source 変更が投影を更新し operator 評価から runner を起動しない|completed_buffer_only and completion_flushes_image_projection historical receipts, newGaussian refresh; independent09.013 worker owns current exactROI/source-cache proof.|
|15.006/filter-pickable-opacity|旧 gint 返却のゼロ透明を現行 gdouble 契約で保ち、合成結果との矛盾を試験|1ae9221541 current325 zero queries+3 projection pixels+3 picking controls; loaded cache then native Gaussian refresh.|
|15.006/filter-progress-start|cancellable と message の新しい引数順を守り、進捗通知・取消しが逆転しない|Existing completed native-owner-progress child covers actual current ABI and cancellation; unchanged15.005/06.016 prerequisites retained.|
|15.006/editability-contract|廃止された GimpItemClass.is_editable を仮定せず、定義編集と通常画素編集の権限を検証|2fbd2e72c5 causal red repaired;10 contexts,definition Undo/Redo exact payload,Clone4/prepared18 and focused sanitizer3.|
|15.007|保存済み cache の初回表示を区別|Loaded cache and clean/stale lineage distinguish first display from new execution; native Gaussian pickability receipt now also proves loaded green cache then red refresh.|
|15.008|procedure/引数変更で結果を失効|Definition revision distinct from cache generation; opaque-definition child carries separate completed receipt.|
|15.009|並替え・移動で結果を失効|Topology refresh invalidates generation and rebuilds subscriptions. Six cross-owned viewable ancestry hunks need explicit non-Edge current reparent/subscription checkpoint, not generic renderer lifetime work.|
|15.010|定義は複製し実行中 runner は共有しない|Duplicate immutable definition/cache lineage but never share an active scheduler or runner; original09 chain remains.|
|15.011|旧定義と再評価状態を復元|Dedicated nonDrawable definition Undo restores saved definition/cache conservatively stale, then reevaluates. Current15.006 editability proves this remains allowed after pixel-cache prohibition.|
|15.012|復元後に最新下層から評価|Deletion/restoration native image Undo reconnects current lower content; historical receipt retained, not rerun on paused Edge path.|
|15.013|ファイルは開け、定義を再保存できる|Unsupported definitions preserve opaque/raw metadata and cache without executable substitution; XCF field/roundtrip module separately verifies wire preservation.|
|15.014|設定・属性・複製・Undo が一致|Model structure tests are not full old popup equivalence. Entire old popup hunks keep explicit29/30/13 owners, historical GTK receipts and current platform holds; no coverage removed.|

## Highest unaffected gap proposal

A Gaussian-only ancestry/subscription model test can establish the six shared
viewable duties and original15.004/15.009 behavior without rerunning Edge. Move
one Filter between root and distinct groups; compare exact uniform lower-source
colors, generation invalidation, actual old/new stack subscriptions, and absence
of obsolete source reruns. Existing product routes already exist; change code
only if the bounded test exposes a causal mismatch. This complements, not
duplicates, the separate09.013 exact GEGL invalidation/ROI audit.

Other unproven areas remain explicit rather than silently widened criteria:
complete legacy temporary-GValue-site mapping for admitted PDB routes; old
flip-only reset versus modern inherited flip; complete attributes matrix;
and full popup source obligations owned by29/30 (including current platform
holds). Unsupported arbitrary procedures remain named16/17 compatibility work.
The real08.020 prerequisite prevents automatic closure of15.001 and its chain.
Historical documentation about old unsupported route counts must not be read
as current counts after later native/isolated executor checkpoints.

## Concrete geometry lead found during full-hunk review

Pinned afa43fae FilterLayer::flip (lines415–422) performs filter_reset and
invalidate_layer only. Neighboring scale/resize/rotate/transform call the parent
then reset/invalidate. Current Filter class registers no flip override. Test the
exact old flip-only contract before proposing a narrow override; do not suppress
all modern transforms or copy Clone no-op behavior wholesale. This is a source
mismatch candidate, not yet a measured defect or a completed task.

## Audit checkpoint and later flip chronology

The map providers are pinned to committed bf547afdb60bbad58d42678791105ac3b9201570,
before the flip fix. Its hashes intentionally do not describe an evolving
working tree. The separate15.005 candidate subsequently reproduced x=20 versus
old expected x=4, then added only a flip callback/registration. Four native
horizontal/vertical direct/item-image scenarios now pass, including ordinary
lower-source flips, derived pixels and whole-image Undo/Redo. Four neighboring
scale/resize/rotate/transform slots remain identical to the native parent.
Selected sanitizer verification and the implementation commit are still separate
from this audit checkpoint; this document does not certify their future result.

## Current supplement (2026-10-21, after cddafca8dd)

The table and chronology above describe the immutable bf547/audit checkpoint,
not today's implementation. They are retained so their provider hashes and
historical gap statements are not silently rewritten. The following separately
committed evidence advances those gaps without changing any original acceptance
or dependency:

* 15.001/pdb-temporary-gvalue: 7adadb8f4f maps both original temporary string
  sites to removed debug-only work and admitted current RAII value ownership.
* 15.003: cddafca8dd, `migration/tests/filter-property-contract/`, verifies six
  publications with four coherent notifications each, independent scalar input
  and getter ownership, raw/opaque bytes and no opaque execution. Native1 and
  selected sanitizer1; not arbitrary boxed-value proof.
* 15.004/15.009: 109b2963e1 ancestry proof follows seven root/group stages,
  reentrant parent changes, 28 subscription counts and16 pixels; native3/san2.
  These are the six cross-owned ancestry duties, not paused renderer lifetimes.
* 15.005: 11ea92ef08 restores old flip-only invalidation (no parent flip), while
  2f34bbc47c rotate and e68ce702a0 matrix delegate to native then invalidate.
  9fad7ffbc6 adds the same pinned old unconditional scheduling after native
  translate/scale/resize. The no-op failures were scheduling parity mismatches,
  not demonstrated wrong pixels. 0feeaa9d95 and2ef2dd32ee render actual portrait
  oncanvas API-driven states; unchanged regenerated rotate/matrix appearance is
  expected, and no pointer gesture interaction is claimed.
* 15.007 already has Gaussian first-loaded-cache evidence in 1ae9221541:
  distinct green saved cache and run_count0 initially, then lower edit produces
  a red completed Gaussian with run_count1. No need to rerun old Edge fixtures
  or invent another equivalent first-display test.
* 15.010: 985c217c99 duplicates an actual RUNNING Gaussian without shared runner,
  independent destination group/source/definition and cancellation; native2/san1.
* 15.012: 915c6e00ae exercises six native Undo restorations and four Redo deletes,
  reconnects latest lower sources and proves eight pixels/six subscriptions.

The original08.020/09/10 dependency chain and other explicitly open obligations
remain open. This supplement is factual indexing of existing receipts, not a
new runtime result or permission to run the user-skipped old Edge path.
