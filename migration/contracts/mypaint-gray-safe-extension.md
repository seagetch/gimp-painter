# Defined safe Gray-alpha and nonincremental rendering

This extension is derived from the existing one-channel Gray and RGBA equations.
There is no claim of an old Gray-alpha pixel oracle: the actual old two-byte
paths are unsupported, as captured separately by the prior Gray checkpoint.

Targets remain native `Y' u8` or `Y'A u8`. The floating accumulation buffer uses
native two-byte `Y'A u8`, with alpha at index one and byte strides checked by
the same bounded Surface rectangles. No target is converted through RGBA.

Gray still uses the red component directly. Two-byte normal, eraser and
alpha-lock equations match the red/alpha components of the original RGBA
expressions, retaining their operation ordering and byte rounding. Sampling
accumulates gray times alpha and replicates the gray result to the three engine
color components. Transparent samples retain the engine's existing no-color
sentinel. A zero resulting alpha has canonical hidden gray zero; locked fully
transparent pixels also use hidden zero without dividing by zero. Cancel
restores the exact original hidden bytes as part of the native snapshot.

Nonincremental rendering first paints the logical segment into the two-byte
floating buffer, then composites its gray/alpha onto the segment's original
native pixels using stroke opacity. A one-channel target remains opaque; a
two-byte target preserves its alpha semantics. Interactive logical split timing
and atomic generic batch behavior use the existing controller unchanged.

## Verified scope and evidence distinction

- 180 standalone synthetic scalar comparisons exercise normal, eraser,
  alpha-lock and weighted sampling against the existing RGBA equations, plus
  exact two-byte alpha access under ASan/UBSan/float-cast-overflow
- 192 synthetic paired native sessions compare every finish pixel's gray/alpha
  with modern RGB/RGBA red/alpha output, with identical grayscale colors and
  initial values; each performs real Undo/Redo and verifies the initial bytes
- Pair dimensions cover Gray/Gray-alpha, incremental/nonincremental,
  bitmap shape, paper, offset selection, smudge, normal/eraser/layer alpha lock
- Explicit transparent full erase and transparent alpha-lock cases verify
  finite output, canonical hidden zero and exact cancel restoration
- The independent old supported Gray193 reference remains a separate required
  gate, alongside existing RGBA129/RGB385 and native lifecycle tests

The scalar and paired-session checks are modern invariant tests, not invented
old goldens. Registered targets pass all180 scalar cases, both native extension groups
(including192 complete paired sessions and transparent cancel cases), native
Gray4 and independent old Gray193. The scalar target passes focused
ASan/UBSan/float-cast-overflow, and both native extension groups pass the same
checks with vptr enabled across26 instrumented and26 RTTI-only units. LSan is
explicitly disabled. The final RTTI-only refresh has no source/header changes;
its rejected predecessor and exact compiled source snapshot are preserved.
The full application links and freshly linked Surface10, generic13, old
RGBA129 and oldRGB385 pass. A separate registered-tool GTK Save/reload
workflow is being finalized; it is not folded into this backend evidence. Higher precision/TRC and arbitrary ICC policy remain separate work.
