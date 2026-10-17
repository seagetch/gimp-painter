# Saved-field contract (original task 10.001)

Source-only acceptance: **PASS** (1,378 assertions;873 classified candidates;16 routes;32 direct controls rejected; independent final review cleared37 focused controls).

This contract covers the original saved-field/type/default specification criterion across the **existing 16 persistence routes**. Its scope is source metadata and schemas. Publication and the retained native acceptance refresh are coordinated separately; the original WBS row remains DOING until that handoff is accepted.

## Fixed scope and population

The input route index is `migration/inventory/persistence-routes.tsv`; its row order defines stable route IDs01–16. The pinned legacy source is `afa43fae3e920210146abed514f136fd49f671b5`, compared with native source based on public `ade58551ffc6326b8feecadaa3351ba14cd0aa0a`.

`migration/inventory/persistence-sites.tsv` contains **873 syntax candidates across49 files**, not873 saved fields. Every candidate has exactly one explicit disposition in `persistence-site-dispositions.tsv`:496 dynamic-schema uses,132 non-persisted sites,80 saved-field uses and165 unchanged-standard uses. Declarations, wrappers, nonpersistent parasites and out-of-scope dockrc calls are distinguished rather than turned into invented saved keys.

The derived contract has **1,378 records**:904 saved-field records,310 unchanged-standard records and164 dynamic-schema records. This is a specification-record count, not a count of distinct runtime instances or a Cartesian expansion of all keys. It includes fixed nested members, admitted version alternatives, inherited registrations and explicit parameterized/opaque schemas. Shared subclass members are linked through inheritance rather than asserted on every subclass.

| Family | Contract records | Candidate dispositions |
|---|---:|---:|
| XCF, legacy extensions, capsule/transport and XCF config members |437|592|
| MyPaint resources, imports, previews and options |201|27|
| Configuration, presets, context and inherited members |740|254|
| Total |1,378|873|

Route record counts are01:219,02:74,03:10,04:3,05:131,06:5,07:79,08:65,09:1,10:21,11:356,12:340,13:16,14:2,15:8,16:48. Counts are derived reports; they are never completeness oracles.

## Artifacts and field meaning

- `migration/inventory/persistence-field-contract.json`: fixed route/input hashes, family files and IDs, source/test hashes, derived counts and cross-family domains.
- `migration/inventory/persistence-{xcf,mypaint,config}-fields.json`: authoritative detailed field/schema records, source evidence, reader/writer mappings and candidate classifications.
- `migration/inventory/persistence-fields.tsv`: readable common-column view. Version-specific defaults, wire tags, owner changes, source rules and parameter domains remain in the detailed JSON.
- `migration/inventory/persistence-site-dispositions.tsv`: complete873-site disposition appendix with exact field references and reasons.

Every record carries storage/version, saved name, native and legacy type, owner, default/absence rule, serialization flags, native/legacy reader and writer anchors, and a specific executable source assertion ID. Where a historical layout is absent, that absence is explicit. Where ownership or defaults differ, the version-specific distinction is retained.

Open-ended dictionaries, third-party parasite bytes, metadata-library payloads, installed GEGL operation properties, recursive GimpConfig objects, typed arguments, arrays and input curves have explicit bounded or parameterized schema rules. Their defaults name the constructor/registration/reader rule, including exact conditional overlays, rather than saying only “see GParamSpec.” Known finite common ancestors and custom nested members are separately listed.

Important reconciliations include native versus legacy GParamSpec flags; selected-resource and user-context overlays; text font/color/markup defaults; grid/symmetry construction; inherited preset options; old PATHS/VECTORS ownership and version branches; legacy sample-points tag27 versus native tag39; dialect-dependent layer mode; channel versus selection alpha; allocated tattoos; capsule requiredness, typed tuples, diagnostic origins and multipart members. The historical185-row XCF matrix and earlier wire/acceptance receipts are preserved unchanged.

## Executable source acceptance

From the repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B migration/tests/test_persistence_field_contract.py --negative-controls
```

The normal acceptance path first executes each independent family validator. The XCF validator parses decoder branches, primitive transfers, concrete writer transfers/getters, registrations and checked constructor/default rules. The MyPaint validator independently derives the declared setting/input population and checks each metadata and serialization rule. The configuration validator derives inherited registrations, resolves conditional/default expressions and checks finite custom schemas against reader/writer programs. Source hashes provide provenance and stale-source rejection, not a substitute for these assertions.

The unified validator then rejects duplicate or omitted routes, fields and syntax sites; invalid cross-family references; missing owner/type/default/flags/anchors; unresolved assertion IDs; unexecuted test targets; changed source seals; and inconsistent derived views. The two tool-options sites referring to inherited context fields are explicitly bounded. The preset-options domain includes both `GimpPainterDeviceOptions -> GimpToolOptions` and `GimpPainterMybrushOptions -> GimpPaintOptions -> GimpToolOptions`, with the appropriate family members.

The bounded32-control suite exercises the actual public family validators and the same unified schema/manifest acceptance functions used normally. It changes metadata, omits fields while adjusting a fabricated count, omits/misroutes candidates, changes parameter domains, removes a route, drops cross-family members, and overlays changed XCF source defaults/types in memory. It does not edit production sources. Independent family review also exercised510 MyPaint and173 configuration controls; their scope is recorded separately from this32-control run.

`migration/acceptance/persistence-field-contract-source.json` records the final executed assertion IDs, exact manifest hash and direct negative-control results. `migration/acceptance/persistence-field-contract-review.json` records the independent review and reviewed hashes. These are source-only evidence. No legacy executable was built or run.

## Limits and retained acceptance

This completes the saved-field metadata/schema inventory only when the source assertions and independent review pass. It does not establish runtime roundtrip, known-property order/duplicate behavior, rendering, broad malformed-input behavior, platform equivalence or full-port completion. Original10.002–10.018 and14.001 remain separate obligations.

The existing Clone gate seals all tracked source. Adding these contract artifacts makes its previous receipt stale without changing that receipt's historical validity. The same strict focused native gate must be refreshed against the final frozen source; it is not weakened or repurposed as evidence for every persistence field.
