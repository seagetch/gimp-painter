# Original per-entry MyPaint stroke Undo acceptance

Unchanged task09.011/mypaint-undo-stroke: **MyPaint stroke の pop/free と所有権を現行 Undo に接続する**.
Unchanged criterion: **旧 pop の空処理を引き継がず Undo/Redo で画素と stroke 状態が往復し、pointer を一度だけ破棄**.
Dependencies09.011 and01.005/gimp3-remaining-callbacks are complete; the latter is signature review only, not substituted for runtime evidence.

## Eight assigned source duties
The exact original hunk-wbs rows are retained in original-eight-duties.tsv, including their original criterion and historical status (not rewritten retroactively).

- 000134/135/137/138/142/143: six enum declaration/registered-name/nick/description additions are restored as distinct appended IDs110/111. Independent112 old named values and110 old metadata remain exact; actual typed entry uses111. New group classification, native dirty masks, nested ordinary+Painter group paired roundtrip and group/action rejection are tested in painter-undo-enums.
- 000939: original164-line Undo implementation's empty pop is replaced by real paired native pixels and immutable evaluator/Resource restoration. Typed StrokeState leases survive observer-triggered free/dispose, and deferred parent pixel/item cleanup plus holder lifetime release exactly once. Current committed enum source is replayed through all16 actual native bridge cases, normal and selected9-unit ASan/UBSan.
- 000940: original50-line header's public type names/macros/get_type are restored with opaque declarations and typed facade. Native DrawableUndo private storage replaces the old GimpUndo/raw Stroke field layout. The old `g_param_spec_object(...G_TYPE_POINTER)` plus pointer-GValue mismatch is not reproduced. This is an explicit source-contract replacement, **not binary/layout ABI equivalence** or admission of arbitrary raw pointers.

## Stronger actual state/pixel behavior
Pinned paired18 proofs cover real Engine/Resource and native continuation coordinates, repeated live reconfiguration, destroyed/replaced owner distinctions, shared-controller groups, coherent update and GEGL buffer observers, prepublication allocation/frozen failure and cancel/redo preservation. Engine snapshots preserve mutable settings/random/timing; installation is no-throw/noalloc. Native pixel allocator behavior remains unchanged, not a new global OOM guarantee.

Pinned geometry14+Tool8 successor proves supported public resize/translate early cancellation and explicit no-write rejection of direct post-mutation geometry/buffer/companion conflicts. Baseline/checkpoint leases are retained until disposal; native cleanup thaws once, Save is unblocked and a later explicit gesture may recover. No arbitrary external pixel journal or general merge/expansion promise is made.

Historical per-entry receipts remain immutable and replayed at their recorded source checkpoints. The current bridge16 replay specifically closes the enum-successor integration loop; no claim is made that all earlier full application dependencies were re-instrumented.

## Separate remaining work, not an ambiguity in this criterion
Original21.014 explicitly requires one physical press→release to be one user Undo. A separate diagnostic demonstrates three logical entries for a long physical gesture in the current Tool and old core. That task remains open and is being addressed deliberately; this per-entry closure does not redefine physical stroke as logical segment. Nor does it claim legacy Stroke event-list serialization, raw C++ ABI, all other handles/C entries, or other painting WBS duties complete.

Tests use authenticated1080×1920 Xvfb and oncanvas enabled. These are lifecycle/model tests; already delivered paired/geometry UI images remain pinned historical visual evidence, not new screenshots from this closure-only replay.
