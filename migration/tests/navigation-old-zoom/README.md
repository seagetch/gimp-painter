# Applied inherited zoom: old-source acceptance

This checkpoint completes the applied-zoom portion of the existing
`25.009/drag-zoom-scroll` criterion; it does not by itself close parent25.009.
The original criterion remains: Shift rotation / Ctrl scaling / normal scrolling
begin, drag and end; distance300; device→image and origins compared with old
operation fixtures. Pan and rotation retain their original unchanged fixtures.

## Implementation boundary

Only the inherited/default zoom gesture calls `scale_painter`. Explicit custom
bindings and public native scale/rotate/pan retain their native policy. Old
requested-factor target truncation occurs before model clamping; old rounded
clamp extents, truncated draw extents, axis fitting, centering and queue handling
are retained. The inherited route uses old RETAIN_CENTERING_ELSE_BEST_GUESS;
this is not a claim to change or validate every public focus policy.

A bounded per-shell exact-origin cache prevents inverse-round-trip ULP loss
from changing subsequent integer focus truncation. It is keyed by image identity,
size/resolution, monitor resolution, factor/scales, viewport, rotation/flips,
integer offsets and device fractions. Public/native mutations explicitly
invalidate it, including numerical no-ops; resize and image replacement also
invalidate. Initial bridge state is derived from the current native view, not
claimed to recover an arbitrary unknown historical double origin. Subsequent
inherited operations retain the exact old arithmetic origin.

The old inverse uses an actual 1×1 Cairo context with unchanged
translate / mirror / rotate(angle /180 × pi) / translate ordering. Merely
inverting an equivalent final matrix produced a different ULP at270°, affecting
integer focus. `cairo-inverse-order.c.txt` is a diagnostic reproducer, not old
application execution. No epsilon snapping or relaxed pixel/origin oracle is used.

Nonfinite/unrealized input is rejected before pointer access or narrowing.
The G_MAXINT/2 requested/applied image-extent cap is a conservative safety
exclusion: exact compatibility is claimed only for admitted finite representable
arithmetic, not every possible old overflow/huge coordinate input.

## Renderer failure, not a skipped case

The retained80,000px synthetic motion at max256 zoom originally failed a real
GTK draw with Cairo out-of-memory. Diagnostic interposition localized the first
failure to layer-boundary repeating-stipple sampling at a ~79k user-space origin.
Rectangle clipping alone did not fix it and was reverted byte-exact. Reducing
the repeating pattern translation by whole tile periods fixed the isolated
case with the original rectangle. The production fix is narrowly in layer
boundary stroke, reduces only sampling origins beyond16384, and preserves
ordinary rasterization byte-for-byte. Diagnostic interposition is **not** used
in final native/sanitizer acceptance. `oom-diagnostic.json` preserves the failed
and exploratory outcomes; final repeated group outcomes are authoritative.

## Tests and limits

Normal and ordinary Meson ASan+UBSan tests run as independent original35,
rotation2 and new zoom10 processes, each with the unchanged60s timeout.
`b_ndebug=false`, O2, debug symbols disabled; dependency libraries are not
instrumented and LSan is disabled (ptrace limitation). No substituted objects.

New cases cover24 H/V/aniso/angle scenes, two zoom reentries, global clamp and
same-clamped-factor behavior, requested-vs-applied scale, extreme-pointer draw,
exact-origin invalidation, invalid/unrealized inputs,48 byte-identical stipple
reference renders, mixed pan→rotate→zoom→pan→zoom, actual no-queue pointer and
queued outside-canvas fallback, resize preference/custom zoom-model clamp,
queue nonconsumption and subsequent nonresize zoom recovery.

Portrait screenshots are actual rendered app at authenticated Xvfb1080×1920×24,
oncanvas UI explicitly enabled. The added image ROI detector checks transformed
landmarks/trails independently of toolbar pixels and checks upright overlay
matrix behavior. Original raster tests keep their original detector/environment.

The unchanged pinned old source oracle has25 functions,7 normal +7 focused
sanitizer groups,7680 source-arithmetic scenarios/run and4 source mutation controls.
The old app/Edge runtime remains **USER-SKIPPED**. No physical tablet, platform
parity, menu-origin callback or inactive-window resize claim is made here.
Historical pan and rotation evidence is replayed at its immutable commits,
not relabeled as a fresh build of the new zoom source.
