# Unimplemented original16 route: Red Eye Removal foundation

Original16.001 demands the complete fixture/preset-name mapping;16.004 assigns
all missing adapters,16.022 requires implementations/comparisons. The retained
108-name inventory has94 compiled selector entries,9 source-eligible scripts,
and5 direct-definition entries. Its93 `required` labels divide into59 general
kernel/isolated,23 context/resource and11 image/output routes. Fifteen rows have
bounded routes, not full-platform/general completeness. No row is removed here.

The native-point-precision child covers explicit normalized-real extensions to
Edge/Gauss/three point operations, not arbitrary old plug-ins or full real-format
owner-context parity. Four isolated helper routes and the parameter-name binder
cover only Blinds, Small Tiles, Retinex and Convolution Matrix. They do not
implicitly admit `plug-in-red-eye-removal` or dispatch a thematic GEGL operation.

## Exact proposed byte contract, before production integration

Pinned old `plug-ins/common/red-eye-removal.c` admits RGB/RGBA only. Four slots:
run mode, image-ID, drawable-ID, signed INT32 threshold. Noninteractive count is
exactly4. Old gimp-pdb-compat.c:61–65 declares the full signed32 range with pspec
default0. The plug-in static interactive default50 and widget0..100 range are
separate; neither clamps noninteractive PDB values. Old pdb-cxx-utils.hpp:68–80 overwrites runtime run-mode with NONINTERACTIVE
and runtime image/drawable IDs. Saved first-three scalar values therefore remain
metadata rather than narrowing the native execution mode. Context IDs remain saved metadata;
future native execution supplies independently captured source/context, never
rewrites saved arguments. Unknown/malformed inputs remain explicit failures.

For each U8 pixel, independently truncate R*0.5133333 and B*0.1933333 to integer,
leave G integer, and form A=2*(threshold-50). If adjustedR >= G-A and adjustedR >=
adjustedB-A, newR=clamp((G+adjustedB)/(2*0.5133333),0,255) truncated to byte;
otherwise copyR. Always copyG/B/alpha, even hidden RGB under zero alpha. No
round-to-nearest, premultiplication or modern GEGL color conversion is equivalent.
The independent oracle uses exact integer ratios5133333/10000000 and1933333/10000000,
not duplicated floating arithmetic. All RGB triples and both sides of each
pixel's precise threshold transition are checked against the literal old C loop.

### Full signed threshold boundaries

The original signed C expressions are undefined outside a bounded arithmetic
domain. For every possible U8 channel, the largest universally defined interval
is **[-1073741646,1073741873]**. At the lower endpoint G=255 produces INT32_MAX
in G-A; one step lower overflows. One step above the upper endpoint overflows
2*(threshold-50). INT32_MIN/MAX are rejected by the independent preflight and
never passed to old C. This is not an invented0..100 restriction or a claim that
undefined original behavior is meaningful. A future route should explicitly
reject unsafe values while retaining the saved definition/cache, not clamp them.

### ROI/owner/runtime work still required

Old remove_redeye uses the start `gimp_drawable_mask_intersect` rectangle, reads
source pixels and writes only its raw shadow, then calls ordinary shadow merge.
Final selection coverage, active components and alpha lock therefore belong to
the existing native owner-merge contract, not this point kernel. Signed width*
height progress arithmetic is another defined-domain bound. A future raster
adapter can reuse bounded point-shadow transfer/cancellation/private publication,
with immutable start ROI and independent output, but no such implementation is
certified by this scalar foundation. No gray/indexed rerun or high precision/TRC
extension is silently added: old U8 RGB/RGBA is the only numerical oracle.

Proposed sequence: (1) scalar old-C/rational proof; (2) bounded vector/spill raw
shadow plus input/domain/cancel/failure tests; (3) native owner/source/context and
strict saved arguments/XCF; (4) actual oncanvas UI and inventory acceptance. The
original16/17 dependencies remain unchanged and none is closed by this report.

Alternatives reviewed: old blur divides by summed alpha without a zero guard;
deinterlace leaves color scratch unwritten when averaged alpha is zero; Colorify
adds legacy COLOR argument representation. They remain genuine missing work,
not reasons to coerce their semantics into an existing operation.
