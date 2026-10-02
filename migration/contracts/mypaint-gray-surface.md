# Native Gray Surface

The first bounded Gray checkpoint accepts native nonlinear one-byte `Y' u8`
without changing drawable format or passing target pixels through RGB. Original
Gray equations use the brush's red component, not a luminance conversion;
color sampling returns replicated gray with alpha one. All existing native
selection offsets, paper, bitmap shape, smudge and Undo/cancel behavior apply.

## Independent old runtime reference

`legacy-mypaint-gray-session` contains 48 actual old incremental sessions,
144 complete 130x96 Gray images, and 193 records (3,599,558 bytes), SHA256
`28cc932d137e760f4b6d5200994cbe6d22779443b23875e585c24ea58de6e7dc`.
The pinned old evaluator, native TileManager Surface and transaction execute in
a separate harness linked to unchanged archives. The new native private
candidate matches every byte. The stimuli are synthetic; the captured outputs
are real execution, not invented goldens. The explicitly recorded warm-up/Undo/
reset is retained from the earlier RGB setup, with no cold-start parity claim.

Bitmap shape, paper, offset selection and smudge are crossed with normal,
eraser and actual layer alpha-lock states. Every scene records finish, real
Undo and real Redo. Sources, archive/executable hashes, compile/link commands,
full image records and runtime diagnostics are preserved with the fixture.

## Unsupported old paths and planned defined extension

The original code lacks two-byte Gray-alpha blend cases. Its floating pixmap
iterator reads alpha at byte three even when the floating format has two
channels. Separate bounded actual-old probes produce 55 unsupported-layer
diagnostics for Gray-alpha (no visible change), and 52 for Gray nonincremental
(with altered output). Both repeated record streams agree on the pinned x86
build, but repeatability does not supply defined missing two-byte semantics.
These probes are negative observations, never part of the positive oracle.

This incremental checkpoint explicitly refuses Gray-alpha and Gray
nonincremental before native start, with no pixel or Undo mutation. They remain
active implementation work. The next extension will use the original
one-channel and RGBA equations with correct two-byte alpha access, documented
zero-alpha behavior, invariant tests and modern native Undo/Redo. It will not
claim old equivalence for the undefined paths.

The byte renderer also explicitly rejects linear/high-precision formats rather
than silently quantizing. Native nonlinear format support does not by itself
establish arbitrary ICC/profile or physical device/platform acceptance.

## Registered and sanitizer gates

Four native groups pass for actual Gray transaction Undo/cancel/native format,
red-component blending and replicated sampling, twelve explicit rejected
precision/TRC formats, and undefined-old-mode refusal before start. The
registered193-record oracle and fixture-integrity target both pass. Each of
the native4 and full193 targets also passes ASan/UBSan/float-cast-overflow with
vptr enabled: 26 instrumented sources plus26 RTTI-only compatibility sources;
LeakSanitizer is disabled. Final refreshes have no source or ABI-header changes.
Concurrent RTTI edits and exact predecessor reports remain explicit. The unit
refresh used the pre-oracle runner version, preserved separately; the oracle
refresh rechecks the entire expected pixel stream rather than keeping a prior
comparison result.

The full application links, and freshly linked Surface10, generic13, old
RGBA129 and old RGB385 tests pass. This is Linux native headless acceptance;
physical tablet input, GUI interaction and other platforms were not rerun for
this slice. Tested source snapshots capture concurrent dependencies exactly.
