# 10.001 public enum registrar evidence successor

This source-only package follows immutable predecessor admission `d1387732d3` and production enum commit `18c43bbe7c2157484201e127c558e77f7c683086`. Validation used the previous selected 7c60612b37 source snapshot plus d138 validators and exactly the seven committed enum providers, not a whole-current-HEAD application build.

**1,379 fields / 873 candidate dispositions / 16 routes PASS.** All three original family ledgers (including old seals and old evidence) and both original TSV views remain byte-identical. Original `persistence-source-successor` package remains unchanged and independently verifies.

The only stale field evidence comprises two entries of `mypaint.option.mode`: the private registrar's Colorize row and the property's private registrar expression. `transition.json` explicitly maps those original entries to the public libgimpbase registrar and current property expression; the aggregate exposes this mapping under `enum_successor`. No old evidence is removed and no arbitrary missing anchor is waived.

## Independently checked wire semantics

The validator parses the archived predecessor's private registrar numeric/name/nick tuples, the current public header's assigned integer values and current registrar tuples. Both must equal exactly the four original tuples (0 normal, 1 normal-and-erase, 2 lock-alpha, 3 colorize), with the same runtime type name. Current set/get bodies must equal predecessor bodies; property class initialization must equal predecessor after only the exact registrar-alias substitution. Thus defaults, flags, registration and property read/write are unchanged, not inferred from matching ledger text.

Public translation domain/context/value descriptions are restored non-wire metadata, not an asserted absence of production change. Public exports, generated enum tables and the application header are bound among the seven exact providers. Their native/header/sanitizer proof is the separately committed `options-public-enum` package; it is not replayed here.

Options source admission is explicitly chained: original old seal → prior reviewed helper state → committed public registrar state. The prior seven-transition archive/hash pins are unchanged. All seven new provider hashes and the committed transition are archived/hash-pinned, and the aggregate discloses their source provenance. Unknown current source bytes, wrong predecessor/intermediate/final digests and changed original field/evidence metadata remain rejected.

## Checks

```
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/test_persistence_field_contract.py --root .
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-enum-successor/test_successor.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-source-successor/test_successors.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-enum-successor/verify.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-source-successor/verify.py
```

New focused suite: **6 groups PASS**, including nine semantic mutants (integer/name/nick/type/default/flags/set/get/export), seven provider-drift cases, exact two evidence mappings only, original evidence-tamper rejection, chain endpoint rejection and transition-proof tampering. Existing predecessor **12 groups PASS**. These controls do not run held runtime preview paths or the full `--negative-controls` suite. No fresh UI/native runtime/complete-WBS claim.

No dirty main persistence JSON/TSV candidate was read or overwritten. Publication remains paused. New aggregate patch is against exact d138 committed base and must be applied without overwriting unrelated worktree candidates.
