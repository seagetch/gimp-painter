# Bounded deletion error-propagation source successor

Source-only admission for committed **272f56f10a**, following d186d1c04a persistence admission. Exact current tools source must differ from its archived predecessor in only one replacement: `gimp_tool_options_delete (tool_info->tool_options, NULL)` becomes the same call with `error`. Every other byte, including saved schema/reader/writer/registration behavior, is identical. Held dirty Bucket registration bytes are never used.

The original seven-transition and enum-successor archives/pins remain unchanged. The tools source chain records its prior admitted digest as an explicit intermediate and appends the exact correction commit. Original family metadata/seals/evidence and both TSV views remain byte-identical. Aggregate source provenance changes only for this tools source; test-source hashes reflect the new bounded validator.

**Metadata gate: 1,379 fields / 873 candidate dispositions / 16 routes PASS.** Three new focused groups pass (exact-line-only constraint, wrong chain endpoints, unknown current source, missing transition, three tampered runtime proof dependencies). Prior enum6 and source12 groups still pass.

The validator binds immutable results/report/tested-source bytes from the separately committed `tool-options-delete-result` package: causal NULL-error red and ten native + ten selected-four-unit sanitizer cases. It verifies that those before/after tools bytes are exactly the archived transition, with source/link seals unchanged. No native test is replayed here and no Preferences GUI result is claimed.

Validation used the previous selected7c source plus committed18c public enum providers, d186 admission and the exact272 tools source. This is not whole-latest-HEAD metadata acceptance. In particular, readiness candidates are not admitted. The separate5c77 Undo enum change to `app/core/core-enums.h` still needs its own source/semantic review before a current-HEAD aggregate claim.

```
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/test_persistence_field_contract.py --root .
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-delete-successor/test_successor.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-enum-successor/test_successor.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-source-successor/test_successors.py
PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/persistence-delete-successor/verify.py
```

No main dirty persistence candidate was read or overwritten. Exact patches target committed d186 base. No full preview negative controls, held runtime paths, publication, field removal or blind source-hash refresh.
