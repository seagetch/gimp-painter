# Legacy Sobel raw-raster foundation

Status: **bounded raster-foundation acceptance passed on 2026-10-16**.
Final standalone normal/ASan/UBSan, default native build, dedicated C++14 header
probe and all ten focused registered targets passed on the source seals below.
This is the bounded arithmetic/byte-transport foundation for `plug-in-sobel`,
not the separate `plug-in-edge` procedure with its Sobel option. No live owner
route, final-context merge, old-XCF activation, UI route, installed application
execution, or original-runtime capture is claimed by this unit.

## Pinned source and independent oracle

Reference commit: `afa43fae3e920210146abed514f136fd49f671b5`.

- `plug-ins/common/edge-sobel.c`, SHA-256
  `f37f08e206559a1948977b3a2e688fc5ed19fa3c442b1766634ac5b6ed7d5402`
- `libgimpmath/gimpmath.h`, SHA-256
  `5be55b44288b5d3bac28e9fc51e5003af07734dc41b098f137e35ef6188f3ffe`

The restored original Sobel source was byte-compared with that immutable Git
blob. The test-only `legacy-sobel-oracle.c` retains the entire original
`sobel_prepare_row`, `RMS`, and `sobel` body, including its row ring, byte loop,
alpha counter, arithmetic, and non-preview row writes. The original `ROUND`
macro is retained separately. The runner checks both marked blocks byte for
byte against the pinned source and header before compilation. C and C++ are
compiled separately; production C++ is not used to calculate oracle gradients.

Adapters replace only GIMP pixel-region access with tightly packed native
Gray, Gray-alpha, RGB, or RGBA memory; provide a captured intersection; allocate
row storage; and make progress, flush, merge, and update hooks inert. Preview
is never invoked, and preview stubs abort if reached. Output starts at zero so
raw writes outside the intersection remain distinguishable. This zero fill is
a foundation workspace policy, not an observed old shadow allocation. The
source retains preview code for exact-body verification, but no preview-domain
or preview-memory behavior is certified. Oracle calls are sequential because
the test adapter uses a single current drawable.

## Exact arithmetic and transport

The production interface carries all three original signed INT32 arguments.
Every nonzero value is true, including negative values and INT32 extrema;
there is no dialog-range or canonical-boolean restriction.

For each color channel:

- The old horizontal flag selects the difference between weighted top and
  bottom rows; the vertical flag selects weighted left minus right columns.
- Both enabled: compute integer squared gradients, square root, original
  `ROUND`, divide the rounded integer by `5.66`, then truncate into integer
  gradient before converting to a byte. Keep-sign has no effect in this mode.
- One enabled: keep-sign gives `127 + ROUND(gradient / 8.0)`; absolute mode
  gives `ROUND(abs(gradient) / 4.0)`.
- `ROUND(x)` means C conversion of `x + 0.5` to `int`, truncating toward zero.
  It is not `floor(x + 0.5)` or `std::round` for negative values.
- Both disabled: keep-sign produces color 127 and synthesized alpha 255;
  otherwise color and alpha are zero.

Each directional gradient is in `[-1020,1020]`; their sum of squares is at
most 2,080,800 and does not overflow INT32. In one-direction signed mode the
result is in `[0,255]`; absolute mode is in `[0,255]`. In both-direction mode,
`ROUND(sqrt(2 * 1020^2)) / 5.66` is below 255, so integer result is at most
254. Consequently every computed color gradient is representable as a byte.

Production input/output is tightly packed, straight RGBA8. Gray-alpha is
represented by replicated RGB with its source alpha; Gray/RGB may be promoted
with alpha 255 by a later caller. Hidden color participates normally regardless
of source alpha. Output alpha is synthesized as 255 if any color gradient is
strictly greater than 10, otherwise zero; source alpha is not copied or used
in color arithmetic. Native no-alpha oracle results supply independent color
bytes, and tests derive the promoted output alpha from those bytes using this
threshold. The range proof above makes that derivation valid; every such result
is also compared with the unchanged C oracle on alpha-bearing promoted input.
No old live FilterLayer promotion behavior is asserted without runtime capture.

## Captured ROI and raw workspace

`filter_sobel_shadow_validate` checks scalars without allocation or raster I/O.
`filter_sobel_shadow_raster` accepts a checked half-open, drawable-local
`FilterSelectionRegion`. An unselected region must cover the complete drawable.
A selected region with no intersection returns `no_merge`: its initialized
private zero output must not be merged into the image. This no-Sobel-merge
outcome is supported by pinned `edge-sobel.c:368-370`, which returns on false
`gimp_drawable_mask_intersect` before Sobel progress, allocation, shadow writes,
or merge. The result follows `libgimp/gimpdrawable_pdb.c:488-499` through
`app/pdb/drawable-cmds.c:432-444`, `app/core/gimpitem.c:2165-2171`, and
`libgimpbase/gimprectangle.c:85`, whose intersection predicate requires strictly
positive width and height. This does not mean no flush anywhere: `run()` later
detaches the drawable and `libgimp/gimpdrawable.c:92` performs a generic flush.
The zero workspace is the standalone policy described above, not evidence of
old allocation bytes or a native no-intersection runtime capture.

The full private shadow is zeroed first, then only ROI pixels are filtered.
Horizontal neighbors replicate the ROI endpoints. Vertical neighbors clamp to
the full drawable and can lie above or below the ROI. This mixed boundary rule
is why an ordinary cropped image or the existing Edge kernel is not a valid
replacement. A 1x1 or single-row ROI can still read surrounding full-image rows.
Final selection weights, active components, alpha lock, layer offsets, owner
lifetime, stale generations and cache publication remain the later owner's
responsibility. This unit neither reads owner state nor certifies its behavior.

Full drawable dimensions must be positive INT32 values. Packed extent must fit
signed 64-bit storage offsets. A nonempty RGBA intersection has width at most
`INT32_MAX / 4 - 2` (536,870,909), keeping the original signed `(width + 2) * 4`
row allocation and byte-loop arithmetic defined. All positive ROI heights and
widths, including 1x1, 1xN, Nx1 and 2x2, are supported within that bound. A narrow
ROI may lie in a wider full drawable. The original returns before allocations
for an empty intersection, so the ROI row-width bound does not apply there.
Contained ordered INT32 endpoints also keep `y - 1`, `row + 1`, `y + height`,
loop increments and endpoint products within the extracted algorithm's signed
domain. These are standalone raster/algorithm bounds, not full original-runtime
admission bounds. The old `libgimp/gimpdrawable.c` wrapper separately computes
signed tile rounding (`dimension + TILE_SIZE - 1`) and tile-count products.
Those can overflow at extreme dimensions and are bypassed by the oracle's
memory adapters. Large declared/pre-cancelled extents prove only checked kernel
transport; native drawable/tile admission needs separate integration evidence.

For original native Gray/RGB input without alpha, the old `counter` is never
reset and can overflow if enough color gradients exceed 10 over the invocation.
The C oracle conservatively requires `ROI width * ROI height * native channels
<= INT32_MAX` for these formats, independent of input bytes or flags. This is a
sufficient defined domain, not a claim that every larger particular image is
undefined. The alpha-bearing RGBA production path resets the counter equivalent
after every pixel and has no such area restriction. Overflowing legacy domains
are rejected rather than assigned a claimed parity result.

## Resource and failure boundary

Input and output must have independent backing storage and exactly
`width * height * 4` bytes. Direct object aliasing is rejected; the caller must
exclude indirect aliases. Input, flags, and captured region are immutable for
the call. The vector convenience wrapper allows the same vector as input and
output by publishing transactionally after successful completion.

The raster kernel uses three `(1024 + 2) * 4` halo buffers, one `1024 * 4`
result buffer, and one 16-KiB zero buffer. Total explicit byte-array storage is
32,792 bytes (plus scalar/frame overhead), independent of image or ROI width.
Reads are at most 4,104 bytes, filtered writes at most 4,096 bytes, and zero-fill
writes at most 16,384 bytes. No scanline-sized or raster-sized allocation occurs
inside the raster algorithm. The vector convenience wrapper does allocate a
full private destination, whose admission belongs to its future caller. The
test C oracle intentionally retains original row-sized allocations; its memory
use is not production resource accounting or measured RSS.

Cancellation is checked before and after every read, write and flush, and
within arithmetic work. It returns `cancelled`; storage exceptions propagate.
Either may leave partially written private raster output, which must never be
published. Source input is never written. Transactional vector output remains
unchanged on cancellation or exception. Progress is monotonic over zero fill
and ROI writes, and reaches one only after flush and a final cancellation check.
This is raw worker behavior, not scheduler/owner lifetime acceptance. The
monotonic progress mapping is an adapter policy, not a replay of old progress:
the original emits `row / height` using the absolute drawable row every 20
rows, then emits 1.0 before the final flush/merge. Offset ROIs can therefore
report at least one before processing is complete. Future genuine live-context
capture must observe actual merge ordering and must not assume another
operation's final-progress hook timing.

## Bounded acceptance results and remaining scope

`painter-filter-sobel` is registered in `app/painter/meson.build`. A dedicated
C++14 translation unit includes `filter-sobel.hpp` first and twice, without
other project headers, and checks the INT32 option fields and outcome type.
Both Meson and the standalone runner compile this header probe. Its test
matrix contains 2,880 comparisons among native-format source C, vector output
and real spill-backed output. It covers every boolean flag combination plus
noncanonical signed truthy flags; Gray/Gray-alpha/RGB/RGBA transport; tiny,
image-edge, tile-crossing and 1023/1024/1025/2049 strip geometry; complete and
interior ROIs; horizontal ROI replication; vertical full-image halos; and
zero-outside-ROI output. Additional executed tests cover:

- 512 one-direction negative/absolute byte sweeps and explicit asymmetric
  rounding witnesses, plus a ROUND-before-division RMS witness
- Strict alpha thresholds at gradients 10/11, hidden colors, source-alpha
  independence, uniform tiny fields, and both-disabled behavior
- Bounded transfers, no-merge behavior, source immutability, exact extents,
  inconsistent regions, row-width overflow and direct/vector aliases
- Every read, write and flush cancellation/error ordinal in a three-strip
  nonzero-origin ROI for vector and real spill storage, including zero-fill
  and final flush; transactional publication and monotonic progress
- Valid declared 64-bit extents with pre-cancellation and no storage access;
  these are not actual high-offset I/O or huge-image execution observations

The source-only verification command is:

```sh
python3 app/painter/tests/run_filter_sobel_tests.py --verify-only
```

Normal and focused ASan/UBSan reruns use the shared execution lane and fresh
report paths:

```sh
flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_sobel_tests.py BUILD --report NORMAL.json
flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_sobel_tests.py BUILD --report SANITIZED.json --sanitize
```

The runner seals pinned sources, exact oracle blocks, all recursively included
local headers, production/test sources, the runner, Meson registration and this
contract. It checks source seals after compilation and execution, hashes the
executable before and after the test, records complete commands/results, and
refuses to overwrite an existing report. Sanitizer mode enables ASan and UBSan
with LeakSanitizer explicitly disabled; **no leak-check claim is made**.

Final standalone C11-oracle/C++14 execution passed in normal no-RTTI mode and
focused AddressSanitizer/UndefinedBehaviorSanitizer mode with RTTI enabled for
UBSan. Each final run passed 2,880 native-format/vector/real-spill comparisons,
512 negative/absolute ROUND checks, and 264 cancellation/storage-error cases
covering every read/write/flush ordinal in both storage routes, plus the listed
semantic, no-merge, alias, progress and geometry assertions. All eight
compile/link/test commands per final mode exited zero with empty stderr. Total
build-plus-test time was 6.414265642 seconds normal and 13.789943335 seconds
sanitized. Leak detection was disabled; no leak result is asserted.

The dedicated first/twice-include header translation unit compiled with C++14,
`-Wall -Wextra -Werror`, exceptions and no RTTI in the final normal standalone
run, and also compiled in the final sanitizer and native Meson builds. This
checks the actual Sobel header independently of other project includes.

The final default native build completed. All ten serial registered targets
passed: `painter-filter-sobel`, `painter-filter-sharpen`,
`painter-filter-sharpen-shadow`, `painter-filter-native-kernels`,
`painter-filter-context-native`, `painter-filter-context`,
`painter-filter-scheduler`, `painter-filter-raster`,
`painter-public-header-compilation`, and `painter-additional-header-routes`.
The strict result classifier found no skips, crashes or unexpected diagnostics.
The additional-header-routes target's normal successful 17-test Python unittest
report was emitted on stderr and is retained, so native stderr is not claimed
to be empty. The public-header target passed 191 C and 210 C++ probes. Native
build configuration warnings about SIMD-module compatibility, `run_command`
and the declared Meson minimum version remain recorded; this is not a
warning-free build claim.

The final publication-safe receipt is `migration/tests/legacy-sobel-raster.json`.
Acceptance uses only `final-normal.json`, `final-sanitizers.json`, and the
`sobel-final` native results. Earlier standalone/integrated runs and the
mistakenly named sanitizer report that actually ran normal mode are retained
as superseded records, not substituted for final sanitizer evidence.

Execution used public base `e26f4cb520bc49b246fee0c685176bf3f2cc4572` plus the
receipt's explicitly hashed Sobel additions. All 21 standalone input seals
were unchanged through both final runs; each executable matched before/after
execution and subsequent read-only inspection. The eleven-file native source
copy seal also remained unchanged through final execution. This acceptance
prose and ledger completion were written afterward. The receipt preserves their
execution-time hashes and separately records post-run documentation updates;
all twenty executable/header/test/runner/Meson inputs remain identical.

This acceptance is limited to the independent raw-raster arithmetic, transport,
private-output behavior and the enumerated build/test checks. Genuine old
direct-PDB/live FilterLayer/raw-shadow/final-merge/writer capture, native owner
route, XCF activation, precise owner admission and lifecycle tests remain
separate subsequent work. No installed-runtime, UI, cross-platform, whole
aggregate or whole-port acceptance is implied.
