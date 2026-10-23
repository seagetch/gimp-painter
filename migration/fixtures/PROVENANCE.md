# Fixture origin and redistribution register

Updated 2026-10-15 for the bounded WBS 02.004–02.013 comparison collection.
This is a source-notice and origin inventory, not a legal opinion, a release
clearance, or an independent audit of all third-party authorship. The wider
resource/dependency audit remains 34.005 and release notices remain 38.010.

## Pinned source and exact notice evidence

Source: <https://github.com/seagetch/gimp-painter/tree/afa43fae3e920210146abed514f136fd49f671b5>.
`../baseline/baseline.json` identifies the source, diff base and modern baseline.
`legacy-source/assets.tsv` and its manifest lock 386 source asset/notice/support
files by Git blob, SHA-256 and size. They are metadata, not runtime evidence.

`provenance-notices/manifest.json` separately locks exact original notice bytes
and complete leading copyright/license comments from the source files used by
these capture overlays and copied rotation sources. Every entry records the
full source-file SHA-256, source URL, byte range and notice hash. The original
files under `provenance-notices/original/` preserve line endings, including the
Deevad CRLF file. No copyright name or license grant has been invented.

| Source scope | Original evidence | Recorded declaration and boundary |
| --- | --- | --- |
| Application core and portions not explicitly licensed otherwise | Original `LICENSE`, first paragraph, and `COPYING` | Repository fallback; the complete bundled COPYING is GPL version 3. This does not override explicit per-file notices. |
| Most copied/modified app files in these packages | Complete per-file leading comments in `provenance-notices/headers/` | GPL version 3 or later, with original author notices. |
| `app/base/glib-cxx-impl.hpp` in capture overlays | Its original leading comment | GPL version 2 or later; do not report this header as originally saying GPL version 3. |
| `app/paint/mypaintbrush-brush.hpp` in the stroke overlay | Its original leading comment | Martin Renold's original permission-to-use/copy/modify/distribute notice and warranty disclaimer. This is an explicit per-file exception to a blanket GPL description. |
| GIMP support libraries | Original `LICENSE`, last paragraph; complete `libgimp/COPYING` | The source explicitly distinguishes these libraries and refers to the LGPL text. The copied text is source evidence, not a license inventory of every linked system library. |
| `data/mypaint-brushes/deevad/` | Complete original `readme.txt` | Declares public domain and Creative Commons Zero. This is the source's declaration, not a bundled full CC0 legal-code document or an independent title investigation. |
| `data/mypaint-brushes/kaerhon/` | Complete original `ReadMe.txt` | Declares CC0. Included for the existing source asset register; no Kaerhon brush is used by the three new stroke captures. |
| `classic`, `experimental`, `ramon`, `tanda`, root brush support assets and `data/layer-presets/` | Original `LICENSE` and `COPYING` | No brush/preset-local override found in the pinned registered directories. Record the repository fallback without guessing individual brush/preview authors. |
| Generated enum header and `configure.ac` | Source hashes/URLs in notice manifest | No leading license comment; retain repository notice context. The enum header is copied source, not newly authored fixture input. |

The complete original root notices are also already sealed inside
`legacy-brush-strokes/`, together with its Deevad notice. Other new packages
reference this shared register and `provenance-notices/`; an extracted standalone
package must travel with those applicable notices and provenance references.
Do not strip source-file notices. The notice bundle is not complete corresponding
source for all linked programs, nor a substitute for release/source-delivery
review. Preserve the pinned source and full capture overlays/rebuild records.

## Comparison inputs, outputs and source resources

`comparative-assets.json` identifies the package integrity records and locks the
12 explicit unchanged resource/source copies: three brushes, their three
previews, two presets, the enum header and three complete rotation source files.
All 12 were compared byte-for-byte with Git objects at the source pin. Original
brush preview artwork is redistributed source material, not a synthetic canvas
and not newly attributed to the capture operator. The stroke resource manifests
record previews as loader icons, not brush geometry or sampling inputs.

| Fixture | Construction and origin | Evidence / limits |
| --- | --- | --- |
| `gimp3-baseline.xcf` | Newly constructed fill/save/reopen baseline from `../tests/baseline-smoke.py` | `../baseline/linux-build.md` and `standard-tests.md`; modern output, not a legacy fixture. No separately identified human artwork author or asset-specific license is recorded. |
| Stroke validator self-tests | Constructed by `../tests/test_stroke_records.py` | `synthetic-format-test`; not measured brush output or UI input. |
| `legacy-runtime/` | Newly constructed minimal ordinary/CloneLayer/FilterLayer scenes, executed by the real old core/writer | README, capture patches/logs and manifest; original edge FilterLayer reader failure remains negative evidence. |
| `legacy-filter-positive/` | Newly constructed colored layers; original brightness/contrast FilterLayer output and real writer/reader capture | Manifest and original instrumentation; successful bounded save/reopen does not prove scheduler completion (pending=1 retained). No external artwork input. |
| `legacy-composite/` | Synthetic rectangle/inset canvases with two unchanged original preset JSON files | Manifest and `preset-provenance.json`; anime re-edit/reopen and watercolor live edits with negative reader evidence. Historical flat-seed archive retains earlier bounded evidence of the same constructed scenes. Presets are source resources, not newly invented fixtures. |
| `legacy-brush-strokes/` | Constructed two-color canvases and 18 explicit synthetic engine-boundary events per brush; genuine old-engine output | `SHA256SUMS`, build manifest and per-brush resource records. Original classic pen/textured_ink and Deevad watercolor_expressive brushes and preview PNGs are retained unchanged. Not physical tablet input, private art or an independently licensed new brush set. |
| `legacy-layer-operations/` | Newly constructed colored rectangle/inset scenes and explicit old-core structural edits | Manifest, source provenance, logs/XCF/raw pixels; source copies/patches retain original notices. Negative FilterLayer reopens and detached pending states remain explicit. |
| `legacy-rotation-operations/` | Transparent test canvas, synthetic GDK events/button calls and separately labeled extracted-source/Cairo probes | Manifest and source provenance; full copied old source files retain their headers. Final canvas PNG is a realized test checkerboard, not user artwork. Original GUI teardown crashes are retained. Published and remote bytes verified at commit `1ea9e3375a5ab67bf7aa3e6b2f4ea7e05f60df04`. |
| `legacy-filter-timeline/` | Constructed 256×256 solid-color layers, controlled execution/cancellation/edit scenarios and original-core observation | Manifest, overlay, build provenance and timelines in the published package. GUI and diagnostic headless records are distinguished. Published and remote bytes verified at commit `78bf4ed5cbb1b9a03578f19fe41ceb448c85659d`; this inventory adds no runtime acceptance. |
| User-provided or third-party private works | None imported into this bounded collection | No private-work distribution permission is asserted. Keep any future private work outside Git/distribution, with only an opaque local ID and explicit permission/retention scope. |

Capture operators are identified only as recorded by the harness/package;
no human artist identity or new copyright assignment is inferred. Newly
constructed test inputs and measured outputs are distinguishable from copied
source/resources. The repository context is recorded, but this inventory does
not grant an asset-specific license to newly created files or declare all output
public domain merely because the source or an input brush is available.

## Boundaries, unknowns and integration gate

- This review covers the comparison collection above and its referenced pinned
  source asset register, not every fixture archive, external library, font,
  installed plug-in, or build dependency elsewhere in the repository. Hashes and
  dependency lists establish identity; they do not establish redistribution rights.
- No independent upstream ownership audit or new permission from the creators
  of the original brushes/previews/presets is claimed. Their recorded source
  notices must remain attached; unresolved ownership/conflicting-notice questions
  remain release-audit matters, not reasons to invent attribution.
- The scope does not distribute the recorded capture binaries or installed
  libraries. Their hashes/paths are provenance records. A later binary bundle
  needs its own complete dependency, notice and source-delivery review.
- Rotation and timeline captures were independently reviewed, published and
  byte-verified before accepting this inventory. Provenance tests were rerun
  against the maintained copies. This does not close any unmeasured runtime
  requirement with an inventory or a format test.
- Earlier runtime negative cases, including edge/watercolor reader failures,
  synchronous pending state, and GUI teardown failure, are preserved as results,
  not converted into compatibility passes by this inventory.

For future runtime fixtures record operator as actually known, creation date,
source commit, patch/instrumentation and binary hashes, build/dependency manifest,
input asset hashes, capture commands/actions, actual observations, output hashes,
original notice or explicit permission evidence and intended distribution scope.
If author/license/permission is unknown, write unknown rather than guessing.
