# Video exact old-writer source oracle

The seven emitter/BE-string/original GType functions are byte-identical excerpts from pinned afa43fae3e920210146abed514f136fd49f671b5, as recorded in `pinned-writer-reference.json` (inherited from the audited Exchange writer foundation). This harness uses GimpInt32 for run/pattern/additive/rotated, and the actual original ImageID/DrawableID types for omitted context. Unused INT8 type helper remains present but is not exercised. Successful byte-I/O shim, primitive declarations, suppressed debug print and unused-kind adapters are infrastructure; this is neither an old application execution nor genuine old artwork.

Independent Python BE serializer confirms seven signed-extreme/zero/valid/invalid-pattern cases in normal and ASan/UBSan runs. No invalid pattern enters a pixel kernel: all values here are serialization data. Four INT32 frames preserve bits exactly; flag values are never normalized to booleans. Actual model/decoder/cache/rendering behavior remains a later native test.

Exactly90 bytes: name length4 +14 NUL-terminated bytes +run12 +omitted image8/drawable8 +pattern12/additive12/rotated12 +END8. Offsets: name length0/name4–17, run18/22/26, image30/34, drawable38/42, pattern46/50/54, additive58/62/66, rotated70/74/78, END82/86. There is no padding/alignment. This is a strict frame contract, not permission to infer missing payloads or coerce invalid patterns.

`run.py` executes only this tiny extracted-source harness. `validate.py` checks exact excerpts/artifact seals, both case sets and an independent serializer plus three negative frame controls. Original task16.022 remains DOING; no native persistence/UI/whole-route closure from this foundation.

Validator setup correction: the first attempted length-prefix mutation wrote an already-zero byte and correctly failed the negative-control self-check; it now changes that byte to0xff. This was a test-control construction error, not a product/old-writer failure.
