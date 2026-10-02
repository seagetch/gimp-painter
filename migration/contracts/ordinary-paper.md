# Ordinary BrushCore paper

Pinned source: seagetch/gimp-painter `afa43fae3e920210146abed514f136fd49f671b5`.

## Actual old option/resource contract

Ordinary BrushCore has exactly `use-texture` and the native context pattern.
Paper scale, inversion, grain, contrast and their input curves are **MyPaint
settings**, not additional ordinary paint options. The port preserves that
separation. Both engines use the same validated native byte paper view; MyPaint
keeps its existing dirty-invalidated owned snapshot/provider and dynamic kernel.
There is no new object-data or BindingStore implementation mechanism.

The selected pattern is retained by native BrushCore and replaced at stroke
start, as before. Each paper result is owned by that core and rebuilt for each
selected brush mask; it never modifies or aliases the cached source mask.
The public setter holds explicit core/pattern operation references through
signal callbacks and old-pattern finalization. Reentrant replacement and loss
of the caller's last core reference are native regression cases.
Dirty pattern pixels are read again. Replacing/disabling paper invalidates the
subsample cache because byte paper uses the original asymmetric mask rounding.
Queued Fill Brush and Painter Smudge copy the pattern into their existing frozen
input envelopes, alongside brush and dynamics. Later selection or pixel edits
cannot rewrite already accepted input. The exact built-in `GimpPatternClipboard`
subtype is admitted alongside plain `GimpPattern`: its native duplicate vfunc
returns an independent plain pattern. Unknown subclasses remain rejected by
queued snapshots and the MyPaint preview provider. Native tests mutate and
finalize the source clipboard pattern; both queued tools have real GTK snapshot
regressions. No system clipboard contents are altered by those tests.

`use-texture` serializes through GimpConfig, duplicates independently, resets to
false and follows brush-property copying. The standard context serializes the
pattern identity. One shared native options widget supplies the expanding
Texture toggle/pattern chooser for Paintbrush, Fill, Painter Smudge and the
compact canvas options. Legacy-profile interpretation belongs to the separate
profile migration bridge.

## Coordinates and arithmetic

For the selected soft/hard/pressure mask of width W/height H:

- Paper origin is `floor(dab.x) - (W >> 1)`, `floor(dab.y) - (H >> 1)`
- The dab is in drawable-local coordinates. Layer offsets are removed before
  mask generation. Paper is not anchored to the display, view, stroke start or
  image-global origin, and does not rotate/scale with the brush
- The old TempBuf mask origin is zero in native BrushCore mask producers
- Pattern channel0 is read verbatim for Y, YA, RGB and RGBA; alpha is ignored
  and RGB paper uses red, not a luminance/colorspace conversion
- Texture follows transform plus soft/hard/pressure selection. Its byte product
  is exactly INT_MULT, including rounding
- Original u8 subsample input rows use +127; final kernel flush rows use +128
- Original pressure accumulates a double lookup then truncates; its near0.5
  shortcut uses the old percent-rounding interval. Modern nonpaper and float
  masks keep their native algorithms

The old x coordinate is mixed with unsigned column i before modulo. This is
observable for negative x and non-power-of-two pattern widths and is retained.
The old negative-y remainder can index before the pattern allocation. The safe
extension wraps negative y upwards. Nonfinite centers, unrepresentable integer mask origins and unsupported
pattern formats produce a checked error, not an invalid cast/read. Origins
are calculated in double before the checked integer conversion.

## Native publication and precision

For original nonlinear-u8 drawables with explicit Painter compatibility modes,
ordinary paper publication reproduces old byte canvas accumulation, paint-alpha
masking, image opacity, selection and channel masks. It reuses native PaintCore's
original/canvas/destination buffers, extents, single Undo and cancellation;
there is no flattened image or separate undo owner. An actual compatibility
publication error aborts the dab; it cannot fall through to native publication.
Only an explicitly unsupported mode/precision returns the unhandled result.
Native byte color handling
retains the old Gray foreground conversion and the distinct no-alpha INT_BLEND
rounding. Alpha-bearing composition uses the existing canonical legacy layer
composite kernel. Fill publication calls the same adapter.

Defined old opaque-mode quirks are retained: Erase/Anti-Erase fall back to
Normal and Src-Out falls back to Replace. The old opaque DST-IN/DST-OUT path
reverses1/3channel destination and2/4channel paint inputs, then its replacement
routine uses the larger source1 channel count for destination writes. This
invalid channel-layout path can write outside the destination row/ROI and is
not a supported byte oracle. The port uses deterministic opaque-as-RGBA
composition and discards the unrepresentable alpha, without reproducing those
invalid writes. The recapture harness does not execute these combinations.

Float masks multiply the native first-channel paper value without byte
quantization. High-precision destinations and modern paint modes continue
through native PaintCore. This is an explicit GIMP3 extension, not a claim that
the old byte renderer supported higher precision. Native float-stroke tests
use modern Normal mode and check that white paper matches its untextured
high-precision result exactly,
retains non-byte initial values and supports complete Undo/Redo. Byte-defined
Painter compatibility blend modes retain their separate mode precision contract;
this adapter does not replace that contract.

## Evidence and scope

- `legacy-paper`:216 actual old paired masks and24 ordinary native
  Paintbrush scenes with initial/finish/Undo/Redo, Y/YA/RGB/RGBA,
  soft/hard/pressure and constant/incremental application
- `legacy-paper-modes`:384 defined scenes across9 legacy modes,4 channel
  layouts,3 mask modes,2 application modes, absent/soft selection and layer
  offsets.48 unsafe opaque DST-IN/OUT combinations are deliberately not run
- `legacy-paper-smudge`:48 actual old textured Smudge scenes with4 channel
  layouts, dynamic size, pressure, blending, rate and full Undo/Redo
- `legacy-paper-fill`:12 actual old textured Fill scenes, paint/erase,
  rate0/50/100, selection and Undo/Redo
- `legacy-paper-transform`:1,296 paired actual old masks including3 bitmap
  and3 generated shapes, scale/angle/aspect changes and negative x. These are
  exact inputs for paper-kernel/phase verification. Explicit
  `painter-legacy-brush-geometry` options now consume all2,592 records including
  the untextured masks through the separate geometry adapter; see
  `ordinary-brush-geometry.md`. Ordinary unmarked modern geometry is unchanged

`run_paper_checks.py` compares every supported pixel record and all2 Smudge/
3 Fill owned routes; `compare_paper.py` registers normal/mode comparisons in
Meson. The untextured GIMP3 pressure-mask records intentionally retain modern
rounding and are not relabeled as legacy evidence. The C++ test separately
consumes unchanged old raw masks, tests all65,536 byte products, resource
lifetime/dirty replacement, invalid domains, config roundtrip and actual native
float stroke Undo/Redo. GUI tests exercise the real shared controls, queued
pattern immutability and cancellation/reuse in both asynchronous tools.

The focused sanitizer builder records instrumented units separately from the
production RTTI-only compatibility closure, preserving UBSan vptr checking.
LeakSanitizer is disabled in this environment; no full-host leak check, real
pen/tablet, Windows or macOS verification is claimed.
