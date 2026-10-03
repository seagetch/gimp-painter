# XCF adversarial recovery checkpoint

This slice was reconstructed from public commit
`3e30c05726039550cc09c0d68a3f1207a099c708` after execution-environment replacement.
Its source and verification are new. The missing unpublished 33-path patch and
its earlier 66/62-case results are not evidence for this implementation.

## Behavior

- A late `PROP_GROUP_ITEM` transfers the carrier's native item ID, tattoo,
  parasites, item attributes and layer attributes before replacing it. Selected,
  linked and named item-set references are retargeted. Legacy Clone/Filter
  replacement uses the same native identity transfer and retargeting route.
  The newly parsed marker wins if legacy custom markers are repeated; carrier
  provenance is copied before setting the current definition fields. Empty
  group bounds still follow the native child-derived resize rules.
- Hierarchy, level and tile addresses are absolute. The reader accepts backwards
  storage, shared tiles and raw tiles that alias structural bytes. It does not
  impose a general forward-address or nonoverlap rule. A following tile address
  is not an encoded-length boundary. Decoders use the existing bounded maximum
  encoded tile size and require exactly the expected decoded pixel count.
- Short image/hierarchy/level headers and offset fields fail. Truncated outer
  layer/channel/path tables are recovery failures, not successful terminators.
  Empty byte streams, zero-length RLE runs and short zlib output cannot certify
  a tile as complete. An explicit zero first tile offset remains the upstream
  legal empty-level representation.
- A failed first tile rejects that drawable. If a later tile fails, all earlier
  complete tiles remain available; the damaged tile is never committed. The
  drawable and image carry an explicit incomplete/save-refusal diagnostic and
  the reader warns. The upstream partial-image salvage path also refuses Save.
- The exact source snapshot remains owned by the image. Duplicating a recovered
  image preserves the refusal and independently references its source bytes,
  including after the original image is closed. Registered Save fails before
  replacing an existing destination. Removing a damaged item does not clear
  the image-level refusal.
- Recovered Filter caches are explicitly incomplete after native definition and
  snapshot restoration. Queued work is cancelled. Image duplicates cancel work
  on copied incomplete Filters after topology restoration, so inspection cannot
  silently replace the recovered cache.

## Independent wire scenes

`app/tests/test-painter-xcf-adversarial.c` registers 246 application tests:

- 144 legal storage orders: every permutation of hierarchy, level, tile A and
  tile B, for both 32-bit and 64-bit offsets and raw/RLE/zlib compression
- Six shared-tile cases, one raw structural-byte alias case, and a positive
  empty-level matrix for both offset widths and all three compression modes
- 84 malformed structure/tile cases across both offset widths and applicable
  compression types, including preservation of complete earlier tiles,
  duplicated source lifetime and exact existing-destination sentinels
- Three legacy partial Filter cases and one native-v1 partial Filter case,
  checking incomplete cache state and recovered pixels after main-loop drains
- Late group, legacy Clone and legacy Filter attribute/identity checks, plus
  repeated Filter-marker last-definition/name/raw-byte semantics
- A partial-image salvage case and a truncated empty offset-table matrix

The layout, alias and damaged scenes are constructed directly as wire bytes,
independently of the production writer. The native-v1 Filter case first saves a
valid native file, then independently locates and damages its second tile.
These synthetic scenes do not replace the genuine historical-writer fixtures.

## Verification and limits

`migration/tests/xcf-reconstructed-normal.json` records exact source hashes and
new executions of `xcf`, `painter-xcf-open`, `painter-xcf-adversarial`,
`painter-xcf-roundtrip` and `painter-provenance`. The initial final-source run
passed all five suites. `run_painter_xcf_adversarial_sanitizers.py` creates an
independent focused ASan/UBSan executable without replacing normal objects; its
report is `xcf-reconstructed-adversarial-sanitizers.json` when completed.
The first attempted overlay stopped before XCF tests because instrumented
BindingStore code saw entries created by native `-fno-rtti` MyPaint options.
That diagnostic is retained in the separate `-initial-rtti-failure.json` report.
The corrected runner uses the existing production RTTI compatibility closure
and distinguishes instrumented units from RTTI-only rebuilds. No vptr check is
suppressed. Only a report with executed test results having exit code zero and no changed
source/header hashes is a passing sanitizer checkpoint. Leak detection is disabled, and remaining GIMP
code and dependencies are not instrumented.

This slice is not a complete malformed-XCF audit. Arbitrary known-property
semantics, resource exhaustion/allocation injection, every recursive graph,
full platform/GUI lifecycle and cross-platform temporary-file behavior remain
separate gates. Retained records beyond the native parasite size limit still
use the existing explicit Save refusal; this slice adds no multipart format.

### Immutable final source checkpoint

The final `xcf-reconstructed-sealed-verification.json` passes all five normal
suites,246 adversarial ASan/UBSan cases and both explicit-cancel sanitizer cases.
Its immutable archive captures381 source/header/generated inputs before compiling
and verifies them afterward.31 production units are instrumented,34 get RTTI-only
compatibility rebuilds, and two additional Filter harness units are instrumented.
Other application/dependency objects remain ordinary; LeakSanitizer is disabled.

The preceding246-case report remains historical:141 of its143 recorded inputs
were preserved byte-exact before two evolving Filter sources changed. Its partial
archive and two-file drift manifest record that limit. The first sealing attempt
stopped before sanitizer tests because a filename glob selected an unregistered
context source. Its source snapshot, normal result and failure log are retained
in `xcf-seal-attempt1/`. The corrected runner selects registered compile-database
Filter units, reports excluded independent work, and fails for missing required
sources. The successful rerun has its own complete snapshot and exact hashes.

The snapshot contains the tested combined build. This commit owns only the
XCF registration line in the shared Meson file; its archived bytes equal the
preceding commit plus that line. Later independent Quit-fixture registration
is not part of this XCF change or its runtime evidence.

A separately observed cleanup gap for an already-parsed effect list on later
effect-table failure remains follow-on work. This slice does not claim every
malformed effect path has been audited.
