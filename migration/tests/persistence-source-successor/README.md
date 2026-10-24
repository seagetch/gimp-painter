# 10.001 bounded persistence-source successors

Frozen source baseline: **7c60612b37**. This package is a source-metadata proof, not current HEAD/enum-successor admission, native runtime/UI verification, or completion of later WBS dependencies.

The ordinary metadata gate now passes **1,379 records, 873 candidate dispositions, 16 routes**. Original three family ledgers and their seals are preserved byte-for-byte. Original field and candidate TSV views are byte-identical. Config retains all 740 metadata assertions and 51 exact scoped program hashes. XCF has no source drift. MyPaint has four stale whole-file seals; config has three. No saved member/type/default/grammar change warrants deleting or replacing any original record.

`persistence_successors.py` admits only the seven exact predecessor/current source pairs in the hash-pinned `transitions.json`; source bytes on both sides and original ledgers are archived and hash-pinned. Every source not listed still requires its original seal. New aggregate `source_successors` records disclose provenance separately from actual current source hashes. Unknown changes to listed files remain rejected. Original field validators still run before aggregate acceptance; source admission does not replace semantics checks.

## Reviewed source changes

- Options: commits 0976ae344b, 65213bdc96 and 6b6a6cf393 append construction/type/constant-mode helper entries; existing property declarations/serialization unchanged.
- Mapping/Resource: a84cd0b051 adds retained-memory accounting; JSON decode/encode and defaults unchanged.
- Container: aa893b3c70 fixes nested membership/model notifications and leases; scoped serializers/deserializers unchanged.
- ToolOptions: f54dba608f protects retired/unregistered identity during context notifications; strict serialized-reference setters unchanged.
- Tools: 151507743e restores consuming public registration and safe startup publication; scoped toolrc read/write/clear bodies unchanged. Only committed bytes were audited, never the held dirty registration candidate.

Runtime identity/lifecycle behavior did change; this package does not claim global behavioral equivalence. Prior runtime proof packages are `mypaint-options-construction-08018`, `options-construction-publication`, `options-mode-entry`, `paired-stroke-09011`, `container-reentrant-membership`, `tool-options-retirement`, and `tool-registration-08018`. They were not replayed by this source-only task. Exact commit/source lineage is in `commit-provenance.json`; all commits were verified ancestors of the frozen baseline.

## Reproduction

On the frozen selected source plus this exact validator/aggregate patch:

```
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/test_persistence_field_contract.py --root .
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-source-successor/test_successors.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-source-successor/verify.py
```

The 12 focused groups include seven admitted pairs and unchanged-source behavior, plus rejection of unknown path/current digest, wrong predecessor/revision, missing transition/archive, forged commit, archive tampering, field removal and old-seal replacement. They use Resource/synthetic metadata only, not held preview diagnostics. The legacy full `--negative-controls` suite was **not run** because it includes paused preview paths; its code remains unchanged.

Current HEAD/public-enum metadata requires an independent subsequent audit; this bounded frozen admission must not silently accept it. Main dirty persistence candidates were not read, overwritten, or used for proof. Publication remains paused.
