# Original 13.001/gegl-operation-route: seven explicit routes

This is a source-backed integration checkpoint, not completion of dependencies.
All seven modes use the added `gimp:painter-legacy-mode`, not a similarly named
upstream operator. `legacy-seven-formulas.md` gives the exact equations and old
source provenance. `legacy-layer-modes.md` describes the native dispatch contract.

| Old raw / current enum | Mode | Why the dedicated byte operator is required |
|---|---|---|
|23 /64|Erase|Old approximate INT_MULT3 then INT_MULT alpha; RGB retained|
|24 /65|Replace|Old alpha rounding and nested integer ratio ordering|
|25 /66|Anti erase|Old approximate INT_MULT3 and remaining-alpha multiply|
|26 /67|SRC IN|Old rounded intermediate alpha, then old Replace, not generic IN|
|27 /68|DST IN|Exchange full operands before intermediate alpha and Replace|
|28 /69|SRC OUT|Complement-left alpha intermediate then old Replace|
|29 /70|DST OUT|Exchange full operands before complement-left intermediate and Replace|

Each row has the same explicit boundary contract:
- GEGL uses straight nonlinear `R'G'B'A float` with immutable nonlinear
  composite space, not premultiplied channels. The compatibility boundary clamps
  and quantizes to bytes before the listed formulas; outputs are byte/255.
- Native mask `Y float` is quantized separately. Absent mask uses the byte
  kernel's distinct sentinel256, not a forced mask255 that would erase old
  no-mask/full-opacity branch distinctions.
- Projection opacity truncates opacity*255; direct PaintCore uses *255.999.
  Mask and opacity are consumed in each old formula's order, not baked into
  source alpha before calling the operator.
- Union affected region preserves old source rectangle coverage. Outside the
  source extent backdrop passes unchanged; zero-alpha pixels inside still act.
- Bottom graph layer uses old initial-region source RGB and opacity/mask alpha,
  rather than an invented transparent-black backdrop blend. Direct PaintCore
  does not use this bottom-node special case.
- Isolated groups evaluate children first. Pass-through groups retain graph
  ordering; single-child reduction is disabled only for Painter byte modes,
  because baking the child's mask/opacity then lifting its mode changed byte
  rounding. Ordinary modern modes keep their existing optimization.

## Existing evidence, without expanding its scope

`legacy-seven-formulas/` compares unchanged old source bodies to current kernel
for 29,601,376 RGBA vectors and1,400,000 alias cases (optimized and ASan/UBSan).
`app/tests/test-painter-layer-modes.c` checks72 genuine old captured scenes in
kernel/GEGL/native-image contexts and216 direct calls; `legacy-mode-precision/`
checks six storage formats and source-extent offsets, not a float extension.
`legacy-mode-groups/` checks54 nested/group scenes and four ordinary optimization
controls; retained causal-red evidence demonstrates the fixed single-child bug.
`legacy-mode-group-undo/` checks18 reorders and108 Undo/Redo actions.
`legacy-mode-xcf/` additionally verifies mask/opacity and old projections across
54 save/reopen cycles, but XCF transport by itself does not prove all ordering.

Arbitrary group-level mask/opacity, broader reparenting/artwork and a separately
identified high-precision extension remain open. This document does not change
original task criteria or mark its prerequisite13.001 complete.
