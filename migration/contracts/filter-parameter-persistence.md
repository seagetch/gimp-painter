# Schema editor persistence through ordinary XCF

Phase D (`12.015/parameter-schema-roundtrip`) integrates the existing four
schema editors with ordinary registered `gimp-xcf-save` / `gimp-xcf-load`
through `file_save()` / `file_open_image()`. The
[XCF v1](xcf-painter-v1.md) and [multipart](xcf-multipart-transport.md) contracts
remain the wire authority. There is no new schema field, wire version, route,
provider query, algorithm or scheduler policy.

## Reference identity correction

A core imported reference has descriptor fields and an independent weak target.
An import can deliberately retain a signed 64-bit recorded ID and declared
supertype different from the actual target's current ID and concrete type.
Phase C's immutable GTK patch already retained both. The old XCF writer instead
looked the descriptor ID up in the process-wide image/item table and dispatched
on its declared type. This could refuse a valid import, or select another live
object whose current ID happened to equal the recorded ID.

The writer now acquires the descriptor and the actual target through one weak
lock. The snapshot accessor first retains the immutable argument model and
transfers that exact strong lease to the caller. The writer keeps the lease
through type, same-image, file-ID and external-reference checks. It never looks
up an object using the descriptor ID. The saved model still holds only a weak
target; neither an edit nor a completed Save installs a strong reference in it.
Expired/unset descriptors return no target and retain their fields exactly.

The existing `(kind, file-ID, recorded-ID, declared-type, was-set, expired)`
tuple has all required information, so its wire layout is unchanged. These
representations must not be conflated:

- In the current model, an explicit import may supply a recorded ID unrelated
  to the target's current runtime ID. Save writes that descriptor and derives
  only the file-local identity from the actual retained target
- On XCF Open, the established reader resolves file-local identities, then
  normalizes **live current-model IDs** to the newly allocated runtime target
  IDs. That existing runtime API behavior is unchanged. Expired IDs stay exact
- The input tuple is preserved separately in the imported capsule and, on a
  subsequent Save, `original-argument-model`. Re-editing/resaving never replaces
  this historical model with the freshly allocated runtime IDs

An actual external target is saved unresolved even if its recorded ID happens
to name an object in the saved image. The live session binding is unchanged;
reopening never searches another image. Declared supertypes are checked against
the actual target, rather than being mistaken for concrete identity classes.

## Native integration matrix

The new tests are included by `app/tests/test-painter-layer-ui.c`. They use the
real registered metadata and GTK response path. Successful saves use ordinary
application file entry points. Close/Open cases release the old image before
loading the file, and assert its finalization. Additional Save/Open observations
retain the owning image only where its Undo/Redo history must be exercised.

| Native group | Boundary |
|---|---|
| `filter_schema_persistence_routes` | Four routes, 11/12-slot Convolution, three ignored scalar tail types; clean cache bytes/zero initial runs; re-edit, one Undo, persisted Undo/Redo, image Duplicate and independent re-edit |
| `filter_schema_persistence_special_bits` | Untouched binary64 NaN payload, positive/negative infinity, signed zero, matrix elements and unknown border enum across edits, history and repeated files |
| `filter_schema_persistence_large_tails` | Five 2 MiB string/STRV/numeric/raw-array tails; actual multipart capsule, bounded preview, no-op, changed scalar, Undo/Redo and repeated Save/Open |
| `filter_schema_persistence_reference_identity` | G_MAXINT64 recorded ID with generic live type; a recorded ID colliding with another layer; truly expired negative ID; external target with local-image ID collision; on-disk tuples, original archive and fresh runtime target mapping checked separately |
| `filter_schema_persistence_reference_lease` | Acquired target survives destruction of the source snapshot and caller's last object reference, then finalizes when the returned lease is released; expired/unset and invalid-index behavior |
| `filter_schema_persistence_double_cache` | RGB DOUBLE linear Convolution actually runs to completion, then its exact native-format committed buffer is compared across Save/Open; a matrix coefficient and offset edit runs and repeats that buffer comparison |
| `filter_schema_persistence_nested_null` | Null/empty strings, STRV, raw arrays and nested models; nested INT64/UINT64 boundaries, binary32 NaN/Inf/signed-zero bits, unknown variant content; edits and history preserve these values |
| `filter_schema_persistence_unknown_opaque` | 2 MiB + 19 byte unknown dictionary field and unknown typed argument model through actual multipart Save/Open, Duplicate, no-op/Cancel, and editable-model scalar patches; opaque model remains preserve-only |
| `filter_schema_persistence_active_save` | Real 1025-square RUNNING jobs for all four routes and IMPORTING for Small Tiles; Save with a different unaccepted textbox value; entire prior committed cache and accepted typed/raw definition survive; stale epoch reloads with zero runs, independently completes once, then reopens clean |
| `filter_schema_persistence_failures` | Edited Convolution with 1 MiB + 17 byte GBytes tail; duplicate-key preflight refusal, preparation/write/final-close cancellation, injected write/final-close errors; exact previous destination, model/history/cache, dirty state, identities and temporary-file counts survive; ordinary Save/Open retry succeeds |

Reopened editors additionally check unchanged OK, equivalent numeric text,
noncanonical signed-flag toggle-and-return, and Cancel without an extra
definition revision, generation, run or Undo. History persistence compares
cache completeness and equal/stale generation relationships, not session token
numbers. Original raw `PROP_FILTER_SPEC` bytes remain independent of edited
typed values.

All newly constructed scenes are **generated modern fixtures**, not outputs
from the old writer. Existing historical fixtures stay byte-identical. The
genuine legacy Edge fixture is exercised by the existing ordinary XCF regression;
its documented old-reader crash is not reclassified as a successful historical
roundtrip. Arrays and references the old writer did not save are not invented.

## Scope and limitations

The acceptance report records exact sources, executables, commands, exits and
raw logs, including the reproduced pre-fix Save refusal and a corrected test
fixture setup. Focused ASan/UBSan adds XCF reader/writer, codec/transport and
duplication units to the existing GTK instrumentation. It is not whole-application
or whole-dependency instrumentation; LeakSanitizer remains disabled. The new
large cases cross the 1 MiB multipart threshold; previously accepted >256 MiB
and 512 MiB transport/resource evidence is reused rather than rerun or relabeled.
Preparation cancellation is at preparation start; existing multipart acceptance
separately covers mid-preparation/disk failure. Injected stream errors use the
existing GFile replacement-stream seam, while successful retries use the ordinary
file entry points.
The final-close fault is an injected error whose wrapper explicitly abandons
replacement; it is not a separately reproduced operating-system rename failure.

This is native GTK integration, not a new full-app keyboard/menu walkthrough,
relocation run, OOM campaign, pixel corpus regeneration or whole-port gate.
Those activities were outside this Phase D result. Phase E now records the
[bounded Linux integration](filter-parameter-acceptance.md), while broader
migration, whole-app OOM and release gates remain open. The known independent
GTK/GDK AT-SPI defect `34.003` and the previously documented Undo-push reentry
atomicity limitation remain open. Missing installed/test-profile assets and
existing compiler/runtime diagnostics are recorded separately from failures.

See `../tests/filter-parameter-persistence/acceptance.json` for the final result
and compact replayable evidence; do not infer completion from this contract
without that result.
