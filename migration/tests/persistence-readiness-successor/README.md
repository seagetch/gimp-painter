# Readiness source successor, not a saved-field replacement

Admits only four exact providers from a2f3ec947000aca3f895d4fc016943ffa1f11d82:
Context, Options implementation, C compatibility header and typed Options header.
The latter two add readiness getter entry points; their current bytes are also
bound, including the C header previously bound by the public-enum admission.
Before/after sources and explicit original endpoint hashes are archived.

Independent metadata comparison establishes unchanged Context set/get, Config
interface, serialize/deserialize and per-property bodies; all 28 property-related
registration calls are identical, including 16 Config property macros. Options
set/get and its entire class initialization are unchanged. Thus saved flags,
defaults, type/name/domain and serializer expressions remain intact. Existing
public-enum numeric/name/nick proof is rerun on current providers. All original
family JSONs, 1,379 records, 873 candidate dispositions, 16 routes and two TSV views
remain unchanged. Original seven-transition, enum, delete and Undo proof packages
are immutable. Explicit chained provenance prevents old enum pins from replacing
actual current source hashes in the aggregate manifest.

The behavioral change is intentionally not described as zero semantic change:
readiness records historical successful adoption in the existing Options slot;
implicit construction seed is excluded, NULL/dispose do not reset past adoption.
A private Context recursion scalar and non-property reaffirmation signal handle
same-source successful selections. These transient states are not new saved
properties. The exact source binding and archived runtime receipts are essential;
lexical metadata equality alone does not prove lifecycle correctness.

Receipts from the exact committed readiness proof are pinned: normal53,
selected five-unit ASan/UBSan20, header42routes/88probes, five causal reds, final
input/source/dependency seals, tested input archive and source-derived original
readiness oracle. This package checks these existing receipts without rerunning
native tests or claiming rendered UI proof. The old oracle was source-derived,
not a historical binary execution. No whole-application sanitizer, leak/vptr or
arbitrary native allocation guarantee is added.

New source-only tests: three groups covering six causal metadata/model mutants,
all four source drifts, all thirteen receipt tamper cases, missing proof and exact
original/enum chain endpoints. Existing Undo3/delete3/enum6/source12 tests remain
unchanged and pass. Full metadata gate passes 1379/873/16; held-path negative
controls were not run. Snapshot is selected frozen7c plus admitted exact public
enum/delete/Undo/readiness sources, not all current HEAD. Pending physical batch
Session/PaintCore candidates are excluded.

Run with PYTHONDONTWRITEBYTECODE=1:
`python3 migration/tests/persistence-readiness-successor/test_successor.py`
`python3 migration/tests/test_persistence_field_contract.py`
`python3 migration/tests/persistence-readiness-successor/verify.py`
