# Committed unsafe-Undo image source successor

Admits only `app/core/gimpimage.c` from committed `84ee103a0ec676937784fe67f7cdf63cf92530b4`, predecessor `5d1c22bc7b`.
The original XCF family hash is preserved. Three exact additions are removed to
reconstruct the entire predecessor byte-for-byte: private signal index, boolean
signal registration, and read-only getter. The header is identical after removing
its getter declaration/comment: no GimpImageClass layout/vfunc ABI change. Existing
properties/defaults/serializers, Save query semantics and every unrelated source
byte are unchanged. This is source admission, not a new persistence/runtime claim.

The immutable runtime package `unsafe-undo-query` establishes the bounded native
148/scoped sanitizer32 behavior. This package pins that original manifest; it does
not replace or reseal earlier runtime evidence.

Read-only metadata proof starts from the verified `5d1c22bc7b` committed-input
snapshot (16,474 hashes checked before and after), overlays exact committed84ee
paths and the explicit new admission module/dispatcher/checker. The strict manifest
failed before its derived provenance update, then passed **1379 records /873
candidates /16 routes**. Every original family JSON and both TSV views are unchanged.
Only gimpimage source hash, its explicit successor, and module/dispatcher/checker
hashes change in the derived aggregate. No dirty main persistence candidate was
used or overwritten. Seven focused tests reject unrelated source/property drift,
header drift, missing transition, wrong endpoints, and changed runtime proof.
No held preview/acquisition/Smudge diagnostic or full negative-control suite ran.
