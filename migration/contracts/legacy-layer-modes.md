# Explicit Painter layer-mode identities and GEGL operation

Old raw 23–29 cannot be reused as current GimpLayerMode numbers. They mean erase,
replace, anti-erase, SRC IN, DST IN, SRC OUT and DST OUT. The new named
`GIMP_LAYER_MODE_PAINTER_*` values append after all upstream values, including
the private anti-erase value; no preexisting enum changes value. Explicit
`gimp_painter_layer_mode_from_legacy` and reverse functions own wire conversion.
Raw 0 and 3 now map to additional exact Painter Normal/Multiply identities;
the other raw 0–22 values retain matching upstream legacy identities. Full pixel
parity of every remaining upstream floating-point mode is still unestablished.

One registered `gimp:painter-legacy-mode` GEGL operation implements these seven
identities. It plugs into the normal layer-mode registry, process function,
legacy menu group, composite-space contract and application initialization.
Generated operation enums and PDB enum metadata are updated by the normal build.
No hidden FilterLayer executor starts inside operator evaluation.

## Exact u8 arithmetic and source ordering

- Inputs/outputs are nonlinear straight RGBA; opacity is truncated to its old
  byte value. Byte mask values and the old INT_MULT / approximate INT_MULT3 /
  strictly-greater-half INT_DIV behavior are retained
- Erase/anti-erase retain destination RGB while changing alpha
- Replace blends alpha first, then calculates the original integer color ratio
- IN/OUT first form the old intermediate source alpha, then use replace
- DST variants exchange both source roles before replacement, including the
  surprising zero-opacity behavior; they are not substituted with a current
  Porter-Duff mode with a similar name
- Transparent pixels inside the layer extent remain meaningful. Outside the
  source rectangle the backdrop passes through. Bottom-layer processing follows
  old initial-region opacity/mask arithmetic rather than applying a mode against
  a fictitious black layer
- The node forces its immutable nonlinear composite space in prepare, including
  direct GEGL callers that leave the parent object's property defaults unchanged

This compatibility operation intentionally computes historical u8 results.
Higher-precision/HDR composition and its explicit user-facing policy are still
an outstanding gate; this is not a claim of lossless float-mode arithmetic.
Source buffers are not rewritten by the operation.

## Evidence and limits

`migration/fixtures/legacy-modes/` contains 56 scenes captured from the actual
pinned old application, each with separate direct projection and merged output.
The first test checkpoint compares all 56 direct projection references in three
independent contexts: the C byte kernel, a real GEGL graph, and a full GimpImage
with two ordinary layers and masks. Every RGBA byte, including hidden RGB, must
match. These are not regenerated expected values derived from the new code.

`migration/tests/painter-modes-testlog.{txt,json}` records the normal app run;
`painter-modes-sanitizers.json` records focused ASan/UBSan instrumentation of
the operation and test adapter. Remaining upstream libraries are uninstrumented;
LSan is disabled. The test runner uses unique Meson log names and removes
unrelated inherited environment before recording evidence.

Remaining: native high-precision behavior/policy, all offset/group/indexed/gray
and zoom paths, newly saved mode metadata, and old 0–22 exact rounding. In
particular, the ordinary legacy XCF projection still differs from the current
upstream legacy Multiply/Normal pipeline by at most 1 RGB on 910 pixels. A direct
old projection capture confirmed this is real, not a merge/projection mismatch.


## Normal/Multiply refinement

The same kernel/node now implements raw 0 and 3 using the historical byte alpha,
integer multiply and truncated floating color interpolation. Sixteen additional
real scenes in `legacy-normal-multiply` bring the combined suite to 72 exact
projection cases in each of the three execution contexts. Normal and Multiply
use distinct new IDs and the reverse mapping retains their old wire meanings;
`gimp_painter_layer_mode_is_compatibility` identifies all nine explicit modes.
The byte kernel also handles either input aliasing its output by copying inputs
before modification. Mask 256 is the explicit no-mask sentinel, preserving the
old separate no-mask/full-opacity-mask arithmetic branches.

Application policy and exact historical XCF image checks are owned by the XCF
reader integration. The ordinary fixture's 910-pixel gap is closed with this
mapping: all 7,680 pixels match the real old direct projection. A shared-layout
file's canonical historical interpretation does not prove arbitrary authorship;
explicit standard recovery remains available and modern typed scalar records
retain modern identities. See the reader contract for its source-backed tests.
