# Original15.013 scope and prior evidence

Criterion unchanged: ファイルは開け、定義を再保存できる. Dependency15.012
remains open. Current native typed record reconstruction and active opaque
argument model preserve unavailable procedures without replacing them with a
supported operation.

Existing `test-painter-xcf-roundtrip.c:replace_uninterpreted_arguments` already
contains unavailable-procedure Open/resave assertions, but the complete case
later executes the user-skipped Edge route. It was read, not rerun or relabeled.
`migration/tests/xcf-unknown-retention-definition/result.json` explicitly reports
source-policy consistency only and is not a native file test. The large
multipart test also preserves unavailable Filter metadata, but its >256MiB
resource scope does not substitute for this small six-round cache/failure test.
All those historical receipts remain unchanged.

This new fixture creates two unavailable definitions: three known typed scalar
arguments, and a serialized optional-variant with an unknown argument type.
Both carry independently specified binary raw bytes and a distinct opaque
cached color. Each is saved/opened three times through registered public XCF
procedures; deliberate invalidation between rounds must fail without starting
an executor. Names, raw bytes and cache remain exact after failure, and the next
round preserves typed/opaque arguments as well. Unknown opaque argument warnings
are expected and kept in stderr. No known-but-invalid procedure is rewritten as
unknown, and no claim about arbitrary malformed recognized definitions is made.
