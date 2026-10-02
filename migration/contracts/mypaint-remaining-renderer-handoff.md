# Remaining MyPaint renderer integration

## Established boundary

The original extended evaluator and all177 decoded resources are independent
of upstream libmypaint. Exact old numerical references cover nonlinear-u8
RGBA129, RGB385 and supported incremental Gray193 session records, plus earlier
mask/Surface/evaluator fixtures. Native Gray-alpha and Gray nonincremental now
have a documented safe two-byte extension; their192 paired modern sessions are
invariants, not an old undefined-path oracle. Generic strokes share one atomic
Session and native Undo snapshot. Interactive split timing remains distinct.

The registered tool now passes14 real-GTK synthetic-input groups, normal and
focused37-source ASan/UBSan/vptr, including Gray/Gray-alpha incremental and
nonincremental press/motion, actual XCF Save, reload of native bytes, Undo and
Redo. This exercises built-in D65 Grayscale with sRGB TRC; it does not establish
arbitrary ICC, high precision, physical tablet input or other platforms.

## 1. Native precision and transfer functions

Current explicit rejection is in `GeglSurface::Impl` in
`app/paint/painter-mypaint-surface/gegl-surface.cpp`. Only native nonlinear-u8
Gray, Gray-alpha, RGB and RGBA enter the renderer. This refusal is a temporary
capability gap, not final migration acceptance.

Recommended implementation boundary:

1. Keep the historical byte path and its exact rounding completely separate.
   Continue all three old session comparisons unchanged. Do not replace that
   path with upstream MyPaint or a generic float approximation.
2. Add a storage/working-format dispatch for other GIMP precisions and TRCs.
   Preserve the drawable's native format. Composite in a documented working
   domain using sufficiently wide scratch precision, then write only the dirty
   rectangle back. A float-only intermediate is insufficient to claim retained
   u32/double precision; double scratch or a native-type dispatch is needed.
3. Define foreground/resource-color, sampling and background interpretation
   together. The current controller gets foreground/background via default
   `R'G'B'A double`; simply removing the Surface guard would silently combine
   incompatible spaces. Resolve the native Babl space/profile and TRC for both
   color ingress and smudge sampling. Use modern GIMP color APIs; standard
   `gimpmybrushsurface.c` is useful API reference, not engine parity proof.
4. Specify finite HDR/negative RGB behavior, alpha bounds, zero-alpha hidden
   color, erasing, alpha lock and nonincremental accumulation. Avoid clipping
   untouched native values or quantizing the whole layer. Sampling still
   returns the evaluator's float values, but native storage should not be
   reduced to evaluator precision unnecessarily.
5. Preserve one native snapshot, segment-local accumulation, selection/offset,
   cancel, atomic failure and Undo/Redo across formats. Keep preview isolation
   and the single existing Session implementation owner.

Required acceptance is a focused backend slice followed by an actual
registered-tool Save/reload slice, rather than just more format names in a
constructor. Cover linear/nonlinear/perceptual TRCs, u16/u32/half/float/double,
Gray/RGB with and without alpha, values around quantization boundaries, HDR and
negative values where meaningful, zero-opacity preservation, cancellation and
native-format XCF roundtrips. Include at least built-in and non-default working
profiles, using independent conversion expectations. No old high-precision
oracle exists because the pinned old application is a byte renderer.

## 2. Active brush-pipe state across logical boundaries

The real-old warmed capture and demonstrated release-tail fix are documented in
`mypaint-active-pipe-release.md`. The coordinate-lifetime difference is real, but
both native built-in selectors ignore last_coords, so no speculative coordinate
carry owner was added. Active resource indices/current-child and global RNG stay
native; preview keeps its private copies and RNG.

The capture instead exposed a real defect: unconditional inactive draw rejection
lost positive-pressure interpolated release dabs and their native pipe selections.
Hover now preserves that tail only within an already active transaction; cold
and settled zero-pressure hover remain sampling-only, including constant-opacity
brushes. The raw coordinate difference remains visible in the records and the
comparator explicitly counts it while checking all consumed axes/native state.

The bounded native scope is eight built-in selection modes with current input
axes, smudge/no-smudge sampling, explicit/evaluator-driven splits, finish,
resource switches, warmed drawable switch, pixel Undo/Redo and external RNG
continuity. Port GTK adds actual release→XCF Save/reload. Custom subclasses that
consume previous coordinates, undefined old cold setup, physical input and other
platforms remain unclaimed. Native precision/ICC remains the substantial renderer
slice described above; byte-format and pipe success do not close that gate.
