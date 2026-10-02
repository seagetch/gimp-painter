# Fixture origin and redistribution register

Recorded 2026-10-01. This register distinguishes source acquisition, format
self-tests and observed application output. It is not a legal opinion or a
claim that every third-party attribution has been independently audited.

## Pinned source bundle

Source: <https://github.com/seagetch/gimp-painter/tree/afa43fae3e920210146abed514f136fd49f671b5>.
The tracked `legacy-source/assets.tsv` contains metadata only. Materialized
copies retain original bytes and source-relative notice paths.

| Scope | Source notice | Recorded declaration | Handling |
| --- | --- | --- | --- |
| `data/mypaint-brushes/deevad/` | `readme.txt`, lines 5–7 and 20–21 | Public domain / Creative Commons Zero | Retain original notice; do not relabel as a new migration-created brush |
| `data/mypaint-brushes/kaerhon/` | `ReadMe.txt`, line 3 | CC0 | Retain original notice |
| `classic`, `experimental`, `ramon`, `tanda` and root brush support assets | Repository `LICENSE`, lines 1–4, and `COPYING` | Repository-wide GPL fallback; COPYING contains GPL version 3 | No brush-local override found in the pinned tree; retain notices and source provenance; independent upstream authorship audit remains part of 34.005 |
| `data/layer-presets/` | Repository `LICENSE` and `COPYING` | Same repository-wide fallback | Source definitions are not evidence that a generated layer stack was executed |

The hash of each notice is recorded alongside the assets. Do not infer that
source availability itself grants additional permissions. Distribution bundles
must carry the applicable notices and source obligations and resolve any newly
found conflicting notice before release. The current change publishes neither
private artworks nor new brush binaries.

## Other current and future fixtures

| Fixture class | Origin/status | Redistribution boundary |
| --- | --- | --- |
| `gimp3-baseline.xcf` | Existing modern baseline, documented by `../baseline/standard-tests.md` | Keep existing baseline provenance; not a legacy output |
| Stroke validator self-tests | Constructed in `../tests/test_stroke_records.py`; tagged `synthetic-format-test` | Test data, never measured brush output or UI input capture |
| Ordinary legacy XCF | Captured by actual old PDB/writer and reopened; `legacy-runtime/ordinary-layers.xcf` | Newly constructed minimal test scene; source script, build hash, capture log and pixel hashes retained |
| CloneLayer XCF | Two real writer/reader captures; `legacy-runtime/clone-*.xcf` | Custom types and source references retained; see per-run instrumentation and binary hashes |
| FilterLayer XCF | Actual async execution and writer capture; `legacy-runtime/filter-edge.xcf`; old reader crashes | Negative reader case is explicit; never label it a successful FilterLayer roundtrip |
| Anime/watercolor works | Not yet captured; 02.007 pending | Prefer minimal generated test artworks, with creator and license stated |
| Legacy strokes / layer / rotation / execution traces | Selected Clone pixels/geometry/name/delete+Undo and one Filter completion observed; comprehensive 02.010–02.013 coverage still pending | Per-run source/binary/instrumentation hashes and logs are retained; no tablet, rotation or full scheduler trace coverage claimed |
| User-provided or third-party private works | None imported | Keep outside Git and all distributable bundles; record only a local opaque fixture ID with permission and retention scope |

When adding a runtime fixture record: operator, creation date, source commit,
patch hash (including recording instrumentation), binary hash, build/dependency
manifest, input asset hashes, capture command/actions, expected observations,
output hashes, author/license/permission and permitted distribution scope.
A synthetic file can test a parser, but its origin must stay marked synthetic
and it cannot close a task requiring execution of the old reader or writer.

The captured runtime test scenes and their provenance are documented in
`legacy-runtime/README.md` and locked by `legacy-runtime/manifest.json`. No private
artwork was imported. Remaining runtime capture tasks retain their provenance
and permission requirements; a missing capture is not a redistributable fixture.
