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

A concrete source-level lifetime difference remains to validate:

- Old `gimpmypaintcore.cpp::split_stroke()` ends the session but retains its
  Surface until the drawable changes
- Old `gimpmypaintcore-surface.cpp` stores `last_coords`/`current_coords` on that
  persistent Surface; begin/end do not reset them
- Old `gimpmypaintcore-brushfeature.hpp::prepare_brush()` calls native
  `gimp_brush_select_brush(last,current)` and then copies current into last
- New `GimpResources::Impl` initializes both coordinates to defaults, while
  `PaintCore::end()` and `retire_segment()` retire the resource provider;
  subsequent logical segments construct a new provider

Native pipe indices/current-child and global RNG remain on the actual active
brush object, but adapter coordinate carry can therefore differ at a split.
This is a source-grounded risk, not yet an independently observed pixel delta.
Preview uses a deliberately private deep copy/RNG and must stay isolated.

Next bounded steps:

1. Capture actual old active-pipe output and state using the existing old
   archives, not only direct new selector tests. Cover all selection modes,
   sample/dab count, sampling-only hover, pressure/tilt/direction, explicit and
   evaluator-driven splits, Save/idle finish, Undo/Redo, drawable/resource
   switches and repeated strokes. Record full pixels plus child/index state.
2. Handle old cold setup honestly: its Surface constructor does not initialize
   those coordinate fields. A defined warmed stimulus can first use a constant
   selector to establish coordinates, then switch selector mode. Preserve the
   cold/setup limitation separately instead of reproducing uninitialized data.
3. If the predicted carry difference is confirmed, keep a small explicit
   selection-state value on the existing controller/session, and share it with
   transient resource providers. Reset only at independently established
   boundaries. Do not clone active pipes, reseed their random stream per segment,
   or restore their state merely because pixel Undo occurred without evidence.
4. Keep begin/end-use balance, callback reentry, resource replacement and image
   lifetime tests. Use existing ObjectRef/BindingStore ownership; no second
   global singleton, qdata bridge or alternate GClass system.

Precision/ICC is a substantial renderer/API integration slice with two
acceptance stages. Pipe carry is a smaller state-lifetime/oracle slice, but its
old capture must precede changing reset semantics. Neither is closed by the
current byte-format success. Aggregate old-profile/open/edit/Save workflows
should continue alongside these bounded gaps; physical/platform gates remain
explicit rather than inferred from Linux harnesses.
