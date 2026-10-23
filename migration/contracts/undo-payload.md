# 09.017 native buffer Undo checkpoint

Original acceptance: 描画前後の差分と復元が一致.

Actual native saved-payload bytes and whole-image Undo/Redo differences are checked
across32 native precision/model/alpha/segment-mode scenes, three cycles each.
The test catches lossy snapshot format conversion and saving post-paint content.
This is stronger than checking history depth, but it is still pixel history only.

Every36 original source-specific inventory duty stays whole-hunk TODO. Original
source scope, range, blob, hash, action and acceptance plus all inherited owners
are retained. Each successor explicitly carries the unchanged original Undo
criterion for that exact source hunk. Original MyPaintCore/Undo engine/stroke
replay, lifecycle hooks and associated enums remain09.011/mypaint-undo-stroke;
remaining representation, PixelRegion/dispatcher and consumer requirements stay
31.017 and their previous owners. There is no typedef/alias-based completion and
no claim that GimpDrawableUndo implements the original engine-state contract.

The ledger validator reconstructs all36 duties and rejects weakened criteria,
lost inherited obligations, missing reverse links or premature inventory closure.
Runtime receipts bind an explicit committed Engine checkpoint independently of
concurrent unfinished engine work; that future work needs its own current proof.
