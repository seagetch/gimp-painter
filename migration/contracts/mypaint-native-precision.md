# Native precision / TRC / ICC extension

Status: native implementation and the144-cell Session /18-precision GTK-XCF
workflow passed normal and focused ASan/UBSan/vptr in the sealed initial snapshot.
The reviewed background-setter correction additionally passes eight backend
groups normally and under sanitizers, plus the refreshed144-cell native normal
matrix. Its native/GTK sanitizer aggregate remains pending a coherent full relink;
earlier results are not relabelled as testing that delta. Exact source snapshots,
format inventory and stage-specific runtime reports are retained below. This is
a defined extension of a byte-only renderer, not old high-precision equivalence.

## Domains

- The extended evaluator's existing float RGB/HSV state has drawable-space
  nonlinear RGB coordinates. Context foreground/background enter through the
  drawable's Babl space. Resource HSV numbers remain brush coordinates in that
  same domain; they do not own an unrelated profile.
- The wide renderer uses straight (unassociated) double RGB or Gray values in
  the native drawable's actual linear, nonlinear, or perceptual transfer curve.
  Babl converts dab/background RGB from the evaluator domain to this domain,
  and shape-aware samples back before the evaluator's documented float limit.
  In particular Gray uses colorimetric RGB→Y conversion, not the red channel.
- Native storage is decoded/encoded by scalar type without a float intermediate.
  The four historical nonlinear-u8 implementations retain their byte arithmetic,
  accumulation order, and existing Gray compatibility behavior. Their native
  profile is used for RGB foreground/background coordinates; existing built-in
  sRGB/Gray reference records must stay exact.
- The selection is a linear scalar coverage, read as double in the wide path;
  the legacy path retains its Y-u8 selection and float mask arithmetic. Geometry
  and native byte brush/paper masks retain their existing resource ownership,
  transforms and coordinate conventions.
- Background updates are atomic. The historical byte path accepts finite RGB,
  clamps out-of-gamut coordinates to [0,1] before its float conversion, and
  preserves every old in-range result. Nonfinite input is rejected before any
  component changes. The wide
  path converts into temporary doubles and publishes only a fully finite result;
  it never writes the unused legacy float background. Finite HDR/negative source
  coordinates remain allowed when that native Babl transform has finite output.
  A rejected update leaves an active session and its previous background usable.

## Wide compositing

Clamp opacity, selection, source alpha and destination alpha to [0,1]. Coverage
includes the existing shape/paper factors. For normal/erase let
`t = coverage * opaque * (1-lock_alpha) * (1-colorize)` and source opacity `s`.
For an alpha destination, `A = (1-t)*a + t*s`. If A>0, combine straight color
with normalized weights `(1-t)*a/A` and `t*s/A`; when A=0, set touched color to
zero. Pure erasure (s=0) preserves straight color while reducing alpha, except
at final zero alpha. For an opaque destination combine old color with
`s*brush + (1-s)*background`, using t. Then apply the lock-alpha color blend with
`t_lock = coverage * opaque * lock_alpha`; keep native alpha bytes untouched
when normal/erase did not change them. Fully transparent locked pixels retain
all hidden color bytes. Colorize retains the existing behavior: it suppresses
normal blending; there is no newly invented colorize operator.

Sampling is alpha-weighted in the same native-TRC working domain, then converted
to drawable-space nonlinear RGB and clamped to [0,1] at the retained float
evaluator callback boundary. Native floating destination HDR is not restricted
by that evaluator boundary.

Only changed scalar components are encoded. Zero mask, zero effective opacity,
no-op values and locked components retain original native bytes, including
hidden/nonfinite values that were never consumed. Finite negative/HDR RGB is
legal in floating storage; no [0,1] color clamp is introduced in working math.
Integer storage clamps to [0,1] and rounds nearest (half upward); half uses IEEE
round-to-nearest-even, and floating overflow clamps to the largest finite value
of its native type. Finite alpha outside [0,1] is normalized only when the
operation modifies alpha. Any nonfinite *consumed* component or converted source
rejects the complete dab before writing; no unrelated pixel is sanitized.

Nonincremental work remains a segment-local double straight-alpha GEGL buffer.
Composite its accumulated color/alpha at stroke opacity over the segment's
native initial snapshot. Sampling continues to observe the displayed target.
Use the current single native Session/BindingStore owner, Undo snapshot, dirty
notifications, cancellation and generic atomic batch. There is no second engine.

## Validation gates

The 18 GimpPrecision values × RGB/RGBA/Gray/GrayA form a 72-cell inventory, not
proof. Construct each with actual native APIs and record the effective format,
space, component type and transfer curve. Profiles are accepted according to
actual native Babl/GIMP support, not a small profile-name whitelist; test built-in
sRGB and Gray, non-default RGB primaries/TRC and Gray TRC, with independent
analytic or LCMS numeric expectations. Unsupported ICC kinds must fail native
profile setup explicitly rather than falling back silently.

Backend tests must cover all cells, native quantization boundaries, u32 values
that collide as float, double LSB distinctions, HDR/negative values, alpha,
erase/lock, selection/offset, paper/shape sampling, nonincremental accumulation,
rollback and generic atomic transactions. Then verify real GTK press/motion/
release, XCF Save/reload of native pixels/format/profile, Undo/Redo, normal and
focused ASan/UBSan/vptr. Existing RGBA129/RGB385/Gray193, Gray-safe extension and
pipe2817 fixtures remain immutable regression gates. The intermittent empty-shell
teardown observation in mypaint-active-pipe-release.md remains separate and open.

## Prototype findings and established scope

The initial native Babl probe is preserved as `mypaint-precision-probe.cpp/.log`.
It demonstrates why direct native scalar codecs are necessary: RGB-u32 values
2147483648 and2147483649 both become0.5 through Babl RGB-double conversion, and
adjacent doubles collapse through linear→nonlinear→linear Babl conversion. No
such fish is used to decode or encode the destination native pixel components.
Only evaluator-facing colors use those transforms; this is an explicit numerical
boundary, not a claim that a “double” format makes every Babl conversion exact.

Native GIMP construction is recorded in `mypaint-precision-native-inventory.json`:
all18 precision enums ×RGB/Gray ×alpha/no-alpha ×built-in/non-default profile
(144 cells) keep the requested effective precision. Non-default profiles are
GIMP Adobe RGB and D50 Gray Lab-TRC. Independent backend tests additionally build
LCMS D65 Adobe-primary gamma2.2 RGB and D50 gamma1.8 Gray profiles. Analytic
forward-TRC tolerance is5e-6; inverse/sample and independent LCMS color comparison
tolerance is3e-4. Observed neutral RGB inverse error was about1.9e-5; colored Gray
Babl versus LCMS error was about2.5e-5. Those color-transform tolerances are
separate from exact integer/native-byte and double-LSB compositing assertions.
No arbitrary ICC-class or profile-name whitelist acceptance is inferred from
this bounded matrix. Native GIMP/Babl determine profile constructibility.

All72 canonical storage/model cells pass the independent backend test. The
native transaction matrix includes incremental/nonincremental cancellation,
two-segment atomic commit, complete failure rollback, native Undo/Redo and
sample-callback cancellation; a separate case changes the profile after hover
before a fresh paint transaction. Profile/format changes use native operation
boundaries; mutating a target's storage inside a raster callback is not a new
supported operation.

The actual GTK group covers all18 precision enums across representative RGB,
Gray, alpha/no-alpha, incremental/nonincremental and built-in/custom profile
combinations. It injects a GDK canvas press, processes native motion, invokes real
XCF Save with a pending stroke, delivers a GDK release, reloads XCF, and compares
native pixels, precision, channel encoding, base type, profile, Undo and Redo.
The first harness omitted GDK release when Save had already cleared tool-active
state; that failed on the next scene and is retained as test-harness evidence.
The corrected paired event sequence settles native UI updates and always sends
the release. This did not require a production workaround.

## Final focused evidence

- `mypaint-precision-backend-runtime.json`: seven normal and ASan/UBSan groups;
  all72 formats, all68 newly admitted format cells' erase/lock/nonincremental
  restoration, exact u32/double LSBs and63,488 finite-half roundtrips/ties.
- `mypaint-precision-background-runtime.json`: post-review setter correction,
  with all eight backend groups rerun normally and under
  ASan/UBSan/float-cast-overflow. The additional group covers all four legacy
  formats' normalized-domain clamping, oversized finite color-conversion
  failure for linear RGB/Gray, unchanged-background erasure, session recovery,
  and finite wide same-domain HDR values beyond float range. Its own source
  snapshot records this delta; earlier native/GTK source archives remain intact.
  The refreshed normal native 144-cell matrix and profile-boundary case also
  pass. A single-Surface-object native sanitizer relink failed because a newly
  built shared Filter owner references symbols absent from the predecessor's
  private Filter archive. The failure is retained in the post-review build log;
  unrelated evolving Filter code was not added to the precision proof. Native
  Session/GTK sanitizer results remain explicitly the pre-review immutable
  snapshot. The final combined-tree aggregate must rebuild coherently.
- `mypaint-precision-native-runtime.json`:144 native GIMP format/profile cells
  in both incremental/nonincremental modes, callback cancellation and profile
  boundary, plus the full16-group GTK suite in normal and ASan/UBSan/vptr.
  The GTK precision group exercises all18 precision enums with native XCF
  pixels/format/profile and Undo/Redo preserved.
- Each native focused build instruments38 sources and recompiles32 bridge
  sources for matching RTTI metadata. Remaining application/dependencies are
  not instrumented; leak detection is disabled. Builder source drift was empty.
- `mypaint-precision-regressions.jsonl`: eleven regression suites passed.
  Existing RGBA129, RGB385, Gray193 and pipe2817 references remain immutable and
  exact; Gray safe extension, generic batch, session, hover and preview passed.
- `mypaint-precision-tested-sources.tar.gz` and matching hashes preserve101
  focused source/header inputs. Four subsequently edited unrelated Filter/XCF
  RTTI-only sources were recovered from the recorded HEAD by exact SHA256 match;
  no working-tree source was rolled back. Final aggregate integration still
  runs the project's standard regression targets against its final combined
  build, as it does for the other concurrently developed features.

The retained projection test failure distinguished native pixels from stale
asynchronous projection data. The test now asserts native pixels first and uses
GimpPickable's explicit projection flush; it no longer assumes that DrawTool's
outline timer has finished projection work. No production projection workaround
was introduced. An initial callback probe overconstrained the first quantized
dab after an aborted controller; the final test uses a fresh controller and a
bounded pressure/motion sequence before requiring cancellation. A Gray-white
background expectation also initially assumed exact RGB→Y color conversion;
its observed1.4e-8 destination difference is handled with3e-7 Babl color tolerance,
while scalar integer/alpha/LSB assertions retain exact or native-quantum bounds.

Native test-directory teardown's existing save warning and the retained
engine's internal negative-step-time diagnostic are not filtered from logs.
The actual XCF Save calls and reloads succeed independently. The earlier
intermittent empty-shell teardown observation is still separately recorded in
`mypaint-active-pipe-release.md`; this precision work does not erase it or
claim a lifecycle fix. Indexed images, arbitrary ICC-class exhaustiveness,
physical tablets, other platforms, performance acceptance and all-application
sanitizer coverage remain outside this focused precision proof.
