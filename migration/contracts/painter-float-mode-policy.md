# Review proposal: original 13.012 opt-in high-precision compositor

Status: bounded policy approved for isolated kernel implementation; native registration and public enum/XCF integration remain pending. Existing criteria unchanged.

## Identity and compatibility

Append nine explicit `GIMP_LAYER_MODE_PAINTER_FLOAT_*` IDs 73..81 in order
Erase, Replace, Anti erase, SRC IN, DST IN, SRC OUT, DST OUT, Normal, Multiply.
Labels `Replace (Painter float nonlinear)` etc. appear in the default/modern mode menu,
not the legacy menu. Changing storage precision NEVER changes a selected mode.
IDs 64..72, old raw0/3/23..29 mappings, legacy labels, byte oracle and XCF behavior
remain unchanged. The new modes have no old raw ID, and cannot be exported as one.
They are continuous analogues of Painter equations, not historical equivalence.
Use distinct operation `gimp:painter-float-mode`, not a new flag on the byte op.

## Exact arithmetic contract

Straight RGBA float in immutable nonlinear RGB (same channel interpretation as
Painter, not linear-light). Finite RGB retains signed/HDR values, with no [0,1]
color clamp. Nonfinite input RGB becomes0. Alpha, mask and opacity: nonfinite
becomes0, finite clamped to[0,1]. Arithmetic intermediates use double; final finite
RGB saturates only at +/-FLT_MAX to avoid float overflow. Output alpha clamped
[0,1]. This intentionally supports finite nonlinear HDR, not physical-light math.
No 1/255 or opacity-byte quantization anywhere. Mask absence means1.

Let A=(Ca,a) backdrop, B=(Cb,b) source, w=opacity*mask.
Erase: Co=Ca, ao=a*(1-b*w).
Anti erase: Co=Ca, ao=a+(1-a)*b*w.
Replace: ao=(1-w)*a+w*b; Co=Ca+(Cb-Ca)*(w*b/ao) if ao>0,
otherwise Co=Ca (specified hidden RGB, no division by zero).
SRC IN: replace after b := a*b.
SRC OUT: replace after b := (1-a)*b.
DST IN/OUT: exchange complete A and B first, then corresponding SRC equation.
Thus DST zero-opacity preserves the exchanged left operand, deliberately;
this is not generic Porter-Duff semantics under familiar names.
Normal: s=b*w; ao=a+(1-a)*s; Co=Ca+(Cb-Ca)*s/ao if ao>0,
otherwise Ca. Multiply: first Cb:=Ca*Cb, b:=min(a,b), then Normal color
calculation; output alpha is a if a>0, otherwise Normal's ao (continuous
analogue of Painter's unusual preserved backdrop alpha).

Graph-only bottom layer initialization bypasses blend equation: Co=Cb,
ao=b*w, retaining hidden sourceRGB. Outside source extent return backdrop
unchanged (including its bits); inside transparent pixels still participate.
Direct cached PaintCore dispatch applies equation to every supplied sample,
uses effective opacity exactly, and is never misidentified as bottom graph node.
No difference in graph/direct quantization exists for these new modes.

## Integration touchpoints / ownership

- operations-enums.h plus generated .c, libgimp/gimpenums.h and pdb/enums.pl:
  append explicit public values; assert app/public/PDB identity and existing IDs.
- new float operation .c/.h + operations registration/Meson; layer-mode registry
  adds flags locking blend/composite-space and UNION, all layer/paint contexts.
- default mode menu exposes distinct labels; existing combo emits usual changed.
- cached dispatch marks new float direct op just as byte op is marked separately.
- group single-child reduction must be disabled for these nine too: even without
  byte rounding, Replace/IN role/order and opacity semantics are not generally
  algebraically movable. Ordinary modern reduction remains enabled.
- XCF native PROP_MODE stores new typed enum ID; old raw conversion rejects it.
  Current item_record removes legacy-mode for noncompatibility modes. Parent
  XCF owner to review whether existing typed native mode record suffices;
  preferred bounded design is no redundant capsule field/schema version, with
  explicit tests that legacy-mode is ABSENT after legacy->float save and that
  modern IDs survive two roundtrips (including Clone/group). Unknown enum fails
  through current validation, never maps to old raw IDs. If native-mode scalar
  validation is range-limited update its allowlist, not historical raw parser.
  Old GIMP unsupported-mode fallback is not promised; do not advertise backward
  compatibility. Do not write redundant new capsule policy without necessity.
- no oncanvas lifetime, renderer, popup, MyPaint or FilterLayer executor changes.

## Required proof before closure

Independent double reference (not copy of production helper), nine modes across
alpha0/1/fractional, opacity/mask0/1/fractional, negative/HDR RGB and NaN/Inf policy;
alias safety, non-grid fractional values, values beyond[0,1], zero-alpha hiddenRGB.
GEGL plus real image graphs and direct dispatch compare tolerance to independent
reference (absolute/relative float tolerance explicitly bounded). Sub-byte input
changes must survive and differ from unchanged legacy output in float storage.
Storage u8/u16/u32/half/float/double tests account only for format conversion;
not all storage formats can represent HDR. Signed offsets/source boundaries,
bottom layers, masks/opacity, isolated/pass-through/nested groups, reorder Undo.
Native public enum/PDB identity, XCF mode IDs and absent legacy raw tag, UI selector
normal signal path with actual portrait oncanvas screenshot. Focused sanitizer
includes new op/dispatch/group/harness; separate XCF owner evidence for C++ paths.
Rerun all existing old formula/native fixtures unchanged as compatibility gate.

## Bounded rollout

1. Review/approve this semantic policy and ownership (no production edits yet).
2. Implement isolated kernel + independent numerical/GEGL tests.
3. Register enums/dispatch/menu with parent-coordinated XCF update and native tests.
4. Group/direct/XCF/UI integration and old-regression reruns; only then evaluate
   original13.012 acceptance, preserving any dependencies still open.
