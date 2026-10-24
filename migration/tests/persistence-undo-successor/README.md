# Exact native Undo-header persistence successor

This source-only admission binds the original MyPaint family header seal to commit
5c77f4ef20b78476394b568d0c65ad78563aafbd. The full before/after headers are archived.
The independently parsed 112 existing names/aliases retain their numeric values;
only MyPaint group/action 110/111 are appended. Group bounds remain 0–48. The enum
remains PDB-skipped. Exact transition reconstruction also preserves every old
annotation/description and every byte outside this enum, including persisted
view, dynamics and mode enums. No serialized history field is introduced here.

The original three family manifests, 1,379 records, 873 candidate dispositions,
16 routes and both TSV views remain byte-identical. Earlier seven-transition,
public brush-mode and deletion successor packages remain unchanged. The aggregate
manifest adds only this explicit source provenance and validator digests.

Validation: metadata gate 1379/873/16; three new focused groups (renumbering,
range, appended ID, unrelated saved enum and PDB annotation mutations; endpoint,
unknown source, missing/tampered proof rejection), plus earlier 3 + 6 + 12 groups.
No held candidate diagnostics, whole-runtime replay, new UI evidence or readiness
candidate admission. Existing native Undo runtime proof belongs separately to
`migration/tests/painter-undo-enums/` at the cited commit; this package makes no
fresh native-test claim. Snapshot is selected frozen 7c60612b37 sources plus the
separately admitted public enum, deletion and this exact header transition, not
an assertion that all current HEAD sources were validated.

Run with PYTHONDONTWRITEBYTECODE=1:
- `python3 migration/tests/persistence-undo-successor/test_successor.py`
- `python3 migration/tests/test_persistence_field_contract.py`
- `python3 migration/tests/persistence-undo-successor/verify.py`
