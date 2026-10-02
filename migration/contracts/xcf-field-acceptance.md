# Field-level XCF acceptance ledger

This audit indexes every row of `../inventory/xcf-wire-records.tsv`, the modern
writer-only fields, all configured GimpText properties, native GEGL effect
records, and the current Painter v1 dictionary keys. The generated
`../inventory/xcf-field-acceptance.tsv` is an explicit **coverage ledger**, not a
blanket all-fields-pass claim. Its `remaining` and `acceptance` columns are part
of each row's result. Rebuild it with `../tests/build_xcf_field_matrix.py`.

## Independent gates

- Wire framing and complete input retention do not prove that a field is live
- A normal modern native value roundtrip does not prove every old field order
- Complete configured GimpText equality plus an actual rendered text edit is
  stronger than comparing saved pixels, but does not exercise all alternate
  nondefault text styles or every legacy gdyntext encoding
- Genuine old writer evidence is identified by its fixture manifest. The new
  `legacy-xcf-fields` scenes use the unchanged pinned legacy writer and include
  editable text, custom units, an indexed palette, links, masks, locks, channels,
  image/layer/channel parasites, guides and resolution
- Native generated fixtures are clearly labelled. They cover seven owner types,
  current profile/simulation-profile metadata and item sets, and modern effects
- Absence/default facts are source grounded; a row says explicitly when an
  absent-property mutation has not been run

## Confirmed preservation defects fixed in this checkpoint

1. Path visibility locks were accepted by the reader but omitted by the writer
2. Native custom/text carrier replacement left named item sets pointing to the
   destroyed ordinary carrier. Construction-time sets are now retargeted before
   that carrier is released. Native Clone/Filter definitions remain authoritative
3. A valid custom-layer envelope on a group/channel/mask/path/text owner could
   be rewritten as `kind=ordinary` on Save. Owner-aware schema acceptance keeps
   the active capsule byte-exact and inert, with the separate origin archive
4. Unknown modern effect properties were skipped without owner-local retention.
   The existing typed provenance component now retains ordered duplicate records
   on the native effect; the sole XCF writer re-emits them in that context.
   Native effect duplication copies that immutable provenance. Unsupported
   effect argument records are likewise retained rather than silently discarded
5. Native effects passed only one argument to two-argument blend/composite
   property writers, and their loader forced those properties to AUTO. Their
   real saved values and stable negative AUTO encoding now roundtrip
6. Modern channel float opacity could be quantized through the default byte
   color and later COLOR records. Loading now preserves independent float alpha
   while applying subsequent RGB/float-RGB values
7. Native path item sets and legacy linked paths now use the existing core set
   model and standard item-set records instead of disabled reader/writer hooks
8. Effect masks were omitted from the Save transaction's item identity/metadata
   collection. They now participate in the same file-local IDs and snapshots

These changes add no GObject instance/class layout or public libgimp ABI, no
second C++ bridge, and no second serialization backend. Effect records remain
native XCF records and Painter records retain their documented v1 envelope.

## Supported large-record boundary

The whole input file has no arbitrary 256 MiB cutoff. Each Painter metadata
capsule is supported only up to 256 MiB minus 4096 bytes, including its 12-byte
magic. Encoding/decoding and aggregate retained sequences have explicit bounds.
A valid retained origin larger than the bound stays available in memory, but
Save fails during preflight before destination replacement. The test supplies
an actual 256 MiB+1 file-backed GBytes and checks both direct prepare and the
registered Save procedure against an unchanged sentinel destination. No data
is silently dropped. Chunked retained metadata is **not implemented**.

## Remaining scope after these assertions

- Missing, version-incompatible or unsafe native GEGL operations do not execute.
  A typed, duplicate-preserved refusal marker now prevents Save from dropping
  their definition. Complete original bytes remain in memory. An editable opaque
  native-effect model is not implemented
- All eight supported GEGL argument families have generated native fixtures;
  unknown/mismatched records remain inert and are re-emitted exactly. This is
  not exhaustive testing of every installed GEGL operation or property range
- Nondefault mirror symmetry config, LCh sample points and native path sets now
  have assertions. Other symmetry classes, malformed known properties and full
  absent-property/order/context permutations remain separate gates
- Actual native precision cases cover RGB/Gray, all18precision/TRC combinations,
  and both compression choices with exact2x2native buffers. They do not prove
  every huge tile/offset graph or a real big-endian platform
- Active Save tests cover real RUNNING and IMPORTING Gaussian spill jobs and
  independent reload/close convergence. They do not prove sustained GUI stress,
  fatal-signal safety or Windows temporary-file behavior
- This checkpoint's final normal test report is authoritative. Focused sanitizer
  verification of the newly expanded field/active cases remains pending if it is
  not accompanied by a passing frozen-source report; earlier sanitizer runs
  cannot be used as evidence for these added source changes
