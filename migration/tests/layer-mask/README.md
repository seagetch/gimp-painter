# 09.007 native layer-mask apply/nonapply evidence

Three native groups extend the real Surface app test from 15 to 18 cases. They
exercise actual PaintCore strokes, GimpLayer mask ownership, native opacity reads
and actual image projection, rather than a replacement model. The original
criterion and exact oracle/limits are in `../../contracts/layer-mask.md`.

Native runs use the unchanged 60-second target budget and authenticated portrait
Xvfb at 1080×1920×24. They do not claim UI screenshots or full-app sanitization.
Only selected Meson result fields are packaged; inherited environment maps are
excluded. Source/fixture hashes identify the executed implementation.

The first fixture required bit-exact native opacity from requested A/Y-double
reads. Actual GEGL nearest Y sampling includes floating color conversion,
including white1.0000000361224. The failed run is preserved under
`excluded-fixture-attempts/`; this is not a production failure. The corrected
fixture keeps an independent ideal alpha×mask oracle with an explicit dependency
precision budget, and adds all256 A/Y byte samples (512 reads) to verify that
budget rather than copying converter constants or assuming a float cast.

The distinct actual production failure and fix are described in the contract:
lazy projection damage leaves a shared GEGL hot tile readable until public
buffer flush retires it. `pre-fix-projection.json` preserves the observed
native failure without raw Meson environment maps. `hot-tile-probe.c` compiles
the real tile validator/chunk iterator, with no replacement renderer or stubs;
`run-hot-tile-probe.py --build <native-build>` runs normal and focused
ASan/UBSan variants with GEGL_SWAP=RAM. This is not full-app sanitizer coverage.

The final aggregate is 58 native TAP cases in five targets, with three
expected-red actual production mutations and restoration/rebuild. Projection
bounds are retained; broader projection task09.013 stays separately owned.

The pre-existing 234-line `test-painter-layer-mask.inc` and its three MyPaint
tool cases are preserved unchanged. New cases use the separate
`test-painter-surface-layer-mask.inc` to avoid a fixture-name collision.

The pre-fix diagnostic predates the new fixture's rename and records its
then-current filename verbatim; that name belonged to the historical tool
fixture. The historical source was restored exactly before the final build,
and only the newly named Surface include is changed by this task.
