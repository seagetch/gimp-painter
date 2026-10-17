# Edge raw-shadow foundation

Task: `16.003/edge-raw-shadow-foundation`. Status: **DONE**, bounded foundation.
Implementation base: `8cade86785af938a254216448a2a5aab3a464cd6`.
Exact native-tested commit: `87c03ed868f2bc7a0c98350bdd93be7afd38b7e4`,
tree `d12095be05ceaaadac2d4ccfba60cfdc891ee3c9`.
Normal, focused sanitizer, fresh default build and thirteen complete selected
registered targets passed. Post-test changes are acceptance/evidence documents
and reconciliation of the separately accepted GUI receipt from public
`098f2fdb3d3c0f1c6dae4a43c9ba7ac334200fd3`; all23 executable/header/oracle/
runner/Meson inputs retain their native-tested hashes. The final documentation
and receipt tree was not itself natively rerun.

## Independent scope and unchanged existing contracts

The new `filter-edge-shadow.hpp/.cpp` accepts a checked drawable-local ROI,
immutable `EdgeOptions`, and explicit private input/output storage. It owns no
GObject, GEGL, owner callback or live context. It provides all six algorithms
and all three full-drawable borders. Every native color channel is transformed,
including hidden RGB at alpha zero; source alpha is copied. Native Gray/GrayA
is independently promoted by channel replication, and Gray/RGB without alpha
by alpha255. No color conversion, selection weights, component mask or alpha
lock enters this calculation.

`filter_edge`, `filter_edge_raster`, their shared arithmetic and normalized-real
entry points remain byte-identical to the public base. Their complete-output
contract preserves original hidden RGB at alpha zero. The new raw API uses
shared production arithmetic, but all expected raw bytes come from separate
literal C, never from that shared `detect()` implementation.

## Pinned source and independent oracle

Reference commit: `afa43fae3e920210146abed514f136fd49f671b5`.

- `plug-ins/common/edge.c`: SHA-256
  `38512426b7145167e7e0b220164bd9cecee70f9b754276ad0d006adc1a18234b`;
  literal `edge()` lines255–393 and dispatcher/six detectors402–596
- `libgimp/gimppixelfetcher.c`: SHA-256
  `da851302005ad64b8518c90d2bb9ab17fcd3052d7ebc4c492fdbd8f511ddbcf3`;
  literal pixel-fetcher border body190–260
- `libgimpmath/gimpmath.h`: SHA-256
  `5be55b44288b5d3bac28e9fc51e5003af07734dc41b098f137e35ef6188f3ffe`;
  literal CLAMP0255 definition
- `app/core/gimpitem.c:2086–2133` and
  `app/pdb/drawable-cmds.c:370–405`: mask_bounds selection/offset/clamp contract
- `app/paint-funcs/paint-funcs.c:1298–1365`: full-coverage merge derivation

The separately compiled C11 oracle retains complete literal marked blocks and
licenses. Memory adapters provide native Gray/GrayA/RGB/RGBA byte regions,
full-drawable fetching, captured mask bounds and flush/merge/update counters.
A full-ROI region allows both literal fast and slow paths; tile boundaries
only select equivalent sampling paths and are not production storage behavior.
One padded row makes original post-last-row pointer increments well-defined.
The oracle preserves caller initialization outside the ROI; its padded image
copies are test storage, not production scratch or RSS accounting.

The adapter conservatively rejects native rowstride above
`INT32_MAX-(2*channels-1)` to keep the literal PIX signed additions defined,
ROI area above INT32_MAX, and packed extent plus padded row above PTRDIFF_MAX.
These adapter limits differ from the production tile-independent storage
limits. The original native runtime's tile count/rounding admission is not
certified by memory-backed calls or large pre-cancelled declarations.

## Amount domain and arithmetic proof

Finite amounts below1, including negative values and signed zero, clamp to1
exactly as in `edge()`. For 8-bit kernels, Sobel/Gradient/Roberts/Differential
each have directional gradient magnitude at most1020. Each integer square is
at most1,040,400, within signed32 before multiplication by the double amount.
Prewitt's maximum is nonnegative and at most1275 (five positive unit weights).
Laplace lies in [-2040,2040]. Thus the conservative common oracle domain is
clamped amount <= INT32_MAX/2040. Linear detector magnitudes fit signed32;
the square-root detectors are below sqrt(1020*INT32_MAX), hence far below
INT32_MAX. The original double-to-gint truncation before CLAMP0255 is defined
throughout this domain. Conversion then byte clamping equals production's
safe byte saturation for these results, including negative Laplace values.

Production accepts every finite amount and retains the existing safe saturation
extension for larger amounts. Values outside the defined old conversion domain
are never assigned an old-C parity claim. NaN and infinities are rejected.
Tests distinguish the bounded-limit source-C comparisons from DBL_MAX safe
extension checks against unchanged existing APIs.

## ROI and empty-result semantics

Full dimensions are positive INT32 values and RGBA8 packed extent fits signed64
storage offsets. Ordered contained half-open endpoints must agree with the
intersects flag. Unselected means a complete drawable, while selected can be
full, interior, edge-touching or empty. ROI area fits INT32 because old edge.c
computes `(x2-x1)*(y2-y1)` before dividing by100 for its progress denominator.
All scalar geometry/options, exact extents and direct aliases are checked before
byte reads/writes/flush. The caller must exclude indirect storage aliases.

The full private output is explicitly zero-initialized, then only ROI pixels
receive calculated raw values. Zero initialization is a foundation workspace
policy, not a claimed old shadow allocation. Both horizontal and vertical halo
neighbors are relative to the full drawable, even outside the ROI; neither axis
uses Sobel's horizontal ROI replication. Wrap reads the opposite drawable edge,
smear repeats its endpoint, and black provides zero samples outside it.

`writes` and `no_writes` describe raw ROI writes only. `no_writes` is expressly
not Sobel's `no_merge`. The literal original `edge()` calls mask_bounds without
an intersection early return, then still executes progress1/flush/merge/update
after an empty pixel-region loop. Oracle counters test those hooks, without
claiming a genuine owner capture or determining final merge disposition. Live
start/final selection timing, offsets, component changes and empty final-context
behavior remain later integration work.

## Fixed transport, cancellation and publication

Three4104-byte halo rows, a4096-byte result strip, a16384-byte zero buffer and a
9-byte channel kernel use32801 explicit byte-array bytes, plus scalar/frame
bookkeeping. This corrects the initial checkpoint's32792 count, which omitted
the nine-byte per-channel kernel. Scratch is independent of image/ROI width.
Reads are at most4104 bytes, filtered writes4096, and zero writes16384. Wrapped
edges may add four-byte reads. No production scanline-sized allocation occurs.
The vector convenience wrapper separately allocates a full private output.

Cancellation is checked before/after every read/write/flush and every256 pixels.
Storage exceptions propagate. Failed/cancelled raster output can be partial
and must remain private. Source bytes never change. Vector publication is
transactional, including in-place input/output aliases, and failed/cancelled
calls preserve completed output. Progress is monotonic across initialization
and ROI writes;1 is reserved until flush and the final cancellation check.
This is normalized transport progress, not old pixel-region callback timing.

## Executed test matrix

The standalone and registered Meson target share the same C11 oracle,
C++14 first/twice-header probe and C++ test program. Source tests cover:

- Every algorithm/border and fractional/minimum-clamped/bounded-max amounts;
  negative/positive Laplace saturation and separately labelled huge-safe amounts
- Four native channel formats with independent promotion, all256 alpha values,
  hidden colors, full/interior/empty ROI, each one-edge-touching partial ROI,
  bottom-right corner-empty ROI,1x1/1xN/Nx1,63/64/65 and
  1023/1024/1025 boundaries, and nonzero-origin multi-strip ROI
- Literal-C versus transactional vector versus actual temporary spill storage;
  poisoned private output and exact outside-ROI initialization
- Every read/write/flush cancellation and injected failure ordinal in each
  border path, including wrap's additional four-byte reads; source/completed
  output isolation, progress, invalid geometry/extents/options/direct aliases
- Valid declared large pre-cancelled extents with no byte I/O; no enormous-I/O
  or actual high-offset execution claim
-76 genuine historical RGBA and45 relevant native Gray Edge outputs after an
  independent full-coverage REPLACE_INTEN specialization; no fresh capture claim

For that independent merge specialization, source alpha equals raw alpha=a,
mask and opacity are255 and all components are active. Original INT_BLEND yields
a, and for a>0 its integer color ratio is255, selecting raw colors exactly;
for a=0 the original retains source hidden colors. Tests apply this independent
specialization to raw oracle and production output, then compare every captured
byte. It is not a general selection/component merge oracle.

The strict runner checks exact pinned blocks and full adapter/header hashes,
source/fixture/object/executable seals before/after, unchanged existing public
contracts, C/C++ compile separation, exact final corpus marker and failed-stage
preservation. Source-only mode cannot claim native acceptance. Python controls
mock all native commands; their success does not validate kernel arithmetic.

Standalone reproduction (acquire the shared lane for native execution):

```
python3 -B tools/check_tasks.py
python3 -B app/painter/tests/run_filter_edge_shadow_tests.py --verify-only
python3 -B app/painter/tests/test_filter_edge_shadow_runner.py
flock /workspace/shared/gimp-painter-build.lock python3 -B app/painter/tests/run_filter_edge_shadow_tests.py BUILD --report NORMAL.json
flock /workspace/shared/gimp-painter-build.lock python3 -B app/painter/tests/run_filter_edge_shadow_tests.py BUILD --report SANITIZED.json --sanitize
```

Normal mode is C++14/noRTTI; focused sanitizer mode enables ASan, UBSan and
float-cast-overflow, with LSan explicitly disabled.

## Acceptance and retained failures

The normal standalone run passed in18.343 seconds and the focused sanitizer
run in116.510 seconds. Each executed15408 native-channel/vector/actual-spill
cases,162 amount cases, all256 alpha values, and2224 enumerated cancellation/
failure cases across wrap, smear, black and tiny/full-smear paths, plus empty
controls. The76 genuine RGBA and45 relevant Gray outputs matched after the
independent full-coverage merge specialization. Both C++14 first/twice-header
probes and exact source/fixture/object/executable before-after seals passed.
There are24 sealed inputs (23 executable/test/build inputs plus this contract)
and312 sealed fixture inputs. Thirty Python runner controls passed separately.

A fresh external default Meson/Ninja build completed3465 steps in401.060 seconds
with no sanitizer, using the approved dependency prefix and pinned gimp-data.
All13 selected complete registrations passed in26.723 seconds, serially and
without filtering within each executable:

- painter-filter-edge-shadow; painter-filter-edge; painter-filter-gray
- painter-filter-raster; painter-filter-raster-kernels
- painter-filter-native-kernels; painter-filter-kernel-progress
- painter-filter-context-native; painter-filter-context; painter-filter-scheduler
- painter-filter-point-shadow; painter-public-header-compilation
- painter-additional-header-routes

The complete registry has164 targets; these13 are not an aggregate run. Source,
submodule, full build ELF inventory, build freshness, registry and staged locale
seals all remained unchanged. Compiler/configuration warnings from the full
build are retained; this is not a warning-free build claim. Standalone compiler
commands and native test diagnostics passed their strict gates.

The first standalone attempt failed at compilation on a test-only range-loop
copy warning before native execution. The const-reference fix was checkpointed
in87c03ed868 before both successful runs. The original receipt/stderr remain,
and its stdout is deterministic gzip of the exact original bytes, with raw and
compressed hashes in `edge-raw-shadow-native-attempts/evidence-packaging.json`.
The original raw stdout also remains outside the repository.

The first full-build evidence driver incorrectly classified the13 successful
Meson results against the selected single-suite registry, losing qualified names
from the complete multi-suite registry. Its failed receipt and original driver
remain unchanged. A separately labelled evidence-only correction uses the
sealed complete registry and exact command/name matching, with12 negative
controls (including wrong names, missing/extra/duplicate results and ambiguous
suites). The unchanged strict helper accepts all13. No native run was repeated
or silently relabelled; the correction explicitly retains the failed attempt.

`migration/tests/edge-raw-shadow-foundation.json` indexes the evidence archive.
Its publication manifest distinguishes original-run hashes from delivered
artifact hashes. Only explicit environment fields and the complete multiline inherited-
environment preamble (raw text bytes[59,9418), lines3–10) are removed; the
preceding header and complete test-section suffix remain byte-identical, as do
all names, commands, statuses, counts and diagnostic strings. An initial
line3-only packaging transform was rejected for retaining multiline environment
values; that v1 directory remains local and is excluded from publication. The
corrected v2 verifier includes multiline-leak and invalid-boundary controls. Private originals remain locally retained and are not
included. Extract the archive and run `python3 -B verify-public-evidence.py .`
to recheck delivered seals, all13 classifications and12 negative controls.
The three publication controls are separate from the12 classifier controls.
The optional `--originals PATH` checks every exact env-only transform against
private originals. The original scripts and failure histories are retained.

No live Edge owner/selection merge disposition, fresh original-runtime capture,
XCF, new UI, installed-application execution, full aggregate, LeakSanitizer,
actual huge-offset I/O, other OS or whole-port acceptance is claimed. The GUI
receipt reconciled here concerns the separate Value Invert task only.
