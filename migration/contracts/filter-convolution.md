# Isolated Convolution Matrix typed-array route

WBS16.003/isolated-convolution-route admits the literal old `plug-in-convmatrix`
definition into the independent Filter scheduler and bounded process bridge.
It does not execute arbitrary saved plug-in names or scripts, launch work from
GEGL evaluation, or replace FilterLayer with a layer effect. The completed
GeglBuffer remains the GEGL source; scheduling, checkpoints, dependencies,
cancellation and generation publication stay outside operator processing.

## Saved definition and current execution

The pinned afa43fae old source registers eleven arguments: run-mode INT32,
image IMAGE, drawable DRAWABLE, matrix-count INT32, matrix FLOATARRAY,
alpha-alg INT32, divisor FLOAT, offset FLOAT, channel-count INT32,
channels INT32ARRAY, border INT32. The typed owner representation preserves
all eleven values (including stale first-three IDs), double coefficients and
scalar bit patterns, five signed int32 flags and opaque source bytes.
Execution requires G_TYPE_INT context/count/flag/border slots, exactly25 doubles
(200 bytes), exactly5 int32s (20 bytes), counts25/5 and G_TYPE_DOUBLE scalars.
The old handler also accepted an ignored twelfth argument despite registering
eleven; actual old int/string cases confirm this. The extra typed value is
retained through Save/reopen but never substituted into native options. Other
unknown lengths remain unsupported. Raw byte lengths and nonnull storage are
checked before indexing. Malformed,
nonfinite and unsupported definitions retain their model and completed cache
with failed execution; a missing array is never inferred to mean identity.

The old writer could omit FLOATARRAY coefficients (tag7, zero payload), and
INT32ARRAY records were not generally recoverable. Preserving such opaque old
records is separate from new typed-array Save/reopen. Coefficients never saved
by the old writer cannot be recovered. Programmatic edit/Undo/Redo is tested;
a GTK array parameter editor remains open.

The private helper queries only the bundled `convolution-matrix` executable
and validates its single hidden `plug-in-painter-convmatrix` registration,
complete nine-argument GIMP3 signature and constraints. A GimpImageProcedure
receives typed arrays through GimpProcedureConfig and works on current
GimpImage/GimpDrawable and GeglBuffer objects. It has no menu, interactive or
last-values execution. The child owns every GObject and shadow lease; the parent
receives no child object identity. The requested old counts are not duplicated
in the current array signature because the boxed arrays carry byte lengths.

## Numerical contracts

The actual old noninteractive check_config() unconditionally disables alpha
weighting, even when alpha-alg is1. Without alpha it also forces EXTEND. The
new route preserves those observed behaviors and keeps the original arguments
unchanged. Channel flags are [gray,red,green,blue,alpha], with every nonzero
integer enabled. Coefficients are x-major: matrix[x*5+y]; accumulation visits
y then x. Autoset is an old dialog feature, not an extra PDB parameter.

Nonlinear U8 uses old float narrowing, float multiply/add/divide/offset,
ROUND and byte clamp. Arbitrary finite doubles are range-checked before float
narrowing. A float-zero divisor, nonfinite values/intermediates, and results outside the
actual safe integer-conversion domain fail safely. Pixel-level checks permit
large finite coefficients/divisors when the actual source keeps calculations
finite; a worst-case255-pixel bound would incorrectly reject such old calls. Width/height below3
remain rejected. Current GEGL0.4.62 convolution was measured first with explicit
matrix orientation, effective border, normalize=false and alpha-weight=false:
144/228 genuine-old full-frame cases differ (maximum120/255). Its forced linear
float representation also loses27/36 finer-than-float double identity samples.
It is therefore not the compatibility executor.

Other native precisions use normalized binary64 samples in the drawable's own
RGB/Gray Babl space and linear/nonlinear/perceptual encoding. Gray is replicated
and extracted without conversion to RGB luminance. The private child uses the
same component encoding and does no ICC conversion; original space stays owner
local. Original double coefficients/divisor/offset are not narrowed. Offset is
in old byte units, so the native addition is offset/255. Colors retain finite
negative and >1 values; alpha is constrained to[0,1]. Nonfinite input or computed
results fail before publication. HALF/FLOAT storage conversion is checked by
reading back each bounded private staged chunk; conversion to infinity fails
with the previous completed cache retained. In-range HDR remains supported.
The live owner always requests raw shadow and
merges in double, with double selection masks, active components, own-alpha lock,
zero-alpha hidden-color retention and stable HDR interpolation. Finite mask
coverage clamps to[0,1]; nonfinite coverage is rejected. The ordinary lower-stack
GEGL compositing remains authoritative input and can itself introduce precision
or profile-roundtrip effects; the new route does not claim to remove them.
Standalone raw_shadow=false double helper merging is not a double-precision
acceptance claim: the private GIMP's ordinary merge path may use float.

The old partial-right CLEAR read can overrun its requested drawable row and
produce different bytes on identical successful calls. A one-row ROI can also
write an uninitialized pre-ROI shadow row. The new GEGL sampler uses explicit
zero padding and initializes unwritten shadow pixels. These safe deterministic
cases have analytic tests and are deliberately excluded from exact-old parity
claims; undefined reads are not reproduced.

## Transport, storage and lifetime

GPF5 / --filter-worker-v5 retains the24-byte frame header and expands the fixed
request to344 bytes. Metadata integers and binary64 parameters are little endian.
The first88 bytes preserve prior route fields. sample_mode is at88 (0=U8,
1/2/3=double linear/nonlinear/perceptual), alpha-alg at92, border at96, native
pixel byte-order marker at100, divisor/offset at104/112,25 doubles at120,
5 int32 flags at320, reserved zero at340. Byte-order marker0 is required for
U8; double uses1=little/2=big and must match the same host executing the helper.
Double pixel payloads are host-native IEEE754, explicitly not portable LE
rasters. Old versions, wrong markers, noncanonical unused route fields and
misaligned4/32-byte pixel frames fail closed before publication.

The256MiB child/runtime memory reserve is retained. In addition, admission
reserves the owner input plus final mask (3/5 samples per Gray/RGB pixel),
child selection mask (one sample per pixel), five bridge/child carrier rasters,
and six bounded convolution scanlines. Logical spill reservations are
conservative accounting, not physical disk or total-RSS guarantees. Worker
crash/cancel, terminal framing, EOF, reap and private-profile cleanup all precede
outcome publication. Existing priority150 dispatcher, bounded owner quanta,
generation checks and completed-cache swap remain unchanged.

## Evidence and remaining gates

The compact `legacy-convolution.tar.gz` archive records304 genuine old calls:
296 successes (284 direct PDB and12 live FilterLayer) plus8 expected tiny-layer
rejections. A fresh-profile repeat matched304/304 final outputs. Two separate
small archives add8 genuine old calls: two finite3e38 coefficient/divisor
identities, two defined near-INT_MAX offsets, and four ignored twelfth-argument
int/string calls. These supplement, rather than rewrite, the sealed304 corpus. Direct helper
comparisons check all296 successful raw ROIs and final old byte merges. Twelve
real live scenes additionally exercise actual current owner scheduling and late
selection changes. Diagnostic old undefined cases are preserved separately.

Native sample/TRC/Gray/profile coverage, exact fine-double output, arrays and
malformed definitions, cancellation/replacement/close, Save/reopen/reedit/Undo,
wire, selectors, focused sanitizers and relocated runtime results are recorded
in `../tests/filter-convolution/acceptance.json`. Each result distinguishes
normal, instrumented and actually installed execution. Tests run on Linux;
Windows/macOS, GTK numeric-array editing, owner progress forwarding, no-swap
execution, arbitrary-size/global-latency and other procedures remain open.
