# Legacy Sharpen raster foundation

This bounded unit implements the raw cropped-ROI shadow calculation for
`plug-in-sharpen`. It does not yet enable a FilterLayer procedure route.

## Pinned semantics

The reference is `plug-ins/common/sharpen.c` at
`afa43fae3e920210146abed514f136fd49f671b5`, SHA-256
`6b16f3d3ee6722157bad2772d1cfdee8e3db8e7ef7fb350ae2cbee8ee832312b`.
The two integer rounding stages are retained independently. Signed arithmetic
right shifts are expressed as portable floor division in the new C++14 kernel.
The legacy PDB INT32 percentage domain is not replaced with the dialog's 1–99
range. Values at least 99 share factor one; negative percentages retain the
legacy arithmetic. Values below `100 - INT32_MAX` are rejected because the old
signed subtraction is undefined.

The kernel accepts straight, tightly packed RGBA8 over an already-cropped
rectangle. Gray values may be replicated into RGB without color conversion.
Alpha is copied unchanged, including at transparent centers; hidden RGB still
participates in filtering. First/last ROI rows and columns are copied.
Selection weights, active components, profile conversion and final shadow
merging belong to the later owner adapter.

## Geometry and storage

Width must be at least two and at most `INT32_MAX / 4`; height must be at least
three and at most `INT32_MAX`. Extent must fit signed 64-bit storage addressing.
Legacy width-one row copies reach out of bounds, while heights one/two leave
shadow rows unwritten. This foundation rejects those cases rather than claiming
an invented legacy-equivalent result.

Source and destination require independent backing storage and exact
`width * height * 4` extents. Direct object aliasing is rejected. The caller
keeps input and options immutable. Fixed 1024-pixel strips plus one-pixel halos
bound working memory and individual reads; no image-sized or scanline-sized
scratch allocation is required. The source is never written.

Cancellation returns false; storage errors propagate. Both can leave partial
private destination bytes. A caller may publish only after true and its own
generation/lifetime checks. Progress reaches one only after destination flush
and the final cancellation check.

## Acceptance

The default native build and six targeted registered tests passed. The new
`painter-filter-sharpen` target uses a separately compiled C11 oracle retaining
byte-identical LUT, row-ring and all four native channel-filter bodies. A runner
mechanically verifies these blocks against the pinned Git source before builds.
This is source-extracted arithmetic evidence, not an actual old-GIMP capture.

Normal C++14/no-RTTI and focused AddressSanitizer/UndefinedBehaviorSanitizer
builds each passed 1,232 native Gray/Gray-alpha/RGB/RGBA comparisons across the
oracle, transactional vector wrapper and real disk-backed storage; 2,048
uniform-field checks; and 292 cancellation/storage-failure checks covering every
read, write and flush ordinal for both storage routes. Tests additionally cover
rounding, clamp, hidden colors, cropped borders, aliases, malformed geometry,
progress and source immutability. Leak detection was explicitly disabled in the
focused sanitizer run; no leak-check result is claimed.

The native regression set also includes filter-raster, filter-raster-kernels,
filter-native-kernels, filter-kernel-progress and filter-context. All six targets
passed with no skips, crashes or unexpected runtime diagnostics. The default
build retains inherited upstream ignored-return-qualifier warnings. The new
header also passed repeated standalone C++14 inclusion with warnings as errors.

Source seals were unchanged across final standalone and native validation.
Independent review found no blocking correctness issue within this scope.
The sanitized receipt is `migration/tests/legacy-sharpen-raster.json`.
Declared extents above 4 GiB are validated with pre-cancellation; this is not
an actual high-offset Sharpen execution result.

The end-user route, owner-context integration, installed runtime, full aggregate,
other platforms and whole-port acceptance remain separate gates.
