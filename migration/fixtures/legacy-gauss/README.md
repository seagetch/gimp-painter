# Legacy plug-in-gauss byte fixtures

Captured on 2026-10-02 by running the real pinned gimp-painter 2.8.23 executable
at `afa43fae3e920210146abed514f136fd49f671b5`. The Gaussian source, legacy
executable, and plug-ins were not modified or rebuilt for this capture.

## Provenance and scope

`capture-report.json` records source/executable hashes, all 104 argument sets,
raw and encoded output hashes, script/log hashes, exit status, and completion
marker verification. `capture.scm`, `capture.log`, `cases.json`, and
`capture.exit-code` preserve the actual PDB capture. Absolute paths in the
script describe this capture environment. `fixtures.tsv` uses local relative
paths for independent C++ byte comparisons.

The input PNGs are profile-free, straight RGBA8. Each was independently loaded
and exported before filtering; all nine loaded RGBA buffers equal their
original inputs byte-for-byte, including hidden RGB under zero alpha.
`file-png-save2` used compression 9, `svtrans=1`, and disabled metadata options.
PNG files were decoded with Pillow 12.3.0 using `convert('RGBA').tobytes()`;
no numerical color transform was applied.

- Dimensions 5x4, 1x1, 1x5, 5x1, and 67x66 at both methods and radii 25/25,
  2/2, 2.5/7.25, 0/25, 25/0, 0.25/0.75, 1/5, and 2/3
- Extra 9x8 constant-color, full-range opaque-color, fully transparent, and
  alpha-0/1 images at both methods and radii 25/25, 2/2, and 0.75/3.25
- Mixed-alpha inputs include 0, 1, 63, 127, 191, 254, and 255
- 25/25/method0 is the supplied `data/layer-presets/test2.json` operation
- Every one of the 104 full output buffers matches the replacement exactly

This proves the whole-drawable PDB execution mapping on unselected RGBA8,
including legacy shadow merging. It does not prove FilterLayer scheduling,
selected-area compositing, arbitrary precision/color spaces, grayscale, UI
responsiveness, or arbitrary legacy plug-in support.

## Arithmetic preserved

The replacement uses the historical IIR constants and recurrence, truncated
byte transfer, and integer RLE weights. Vertical runs before horizontal, and
each pass separately rounds alpha premultiplication and separation. An input
radius is converted through `abs(radius) + 1` to the historical sigma; it is
not passed directly to a GEGL Gaussian. If either radius is at most 1, the
legacy entry point switches both axes to RLE, even if method0 requested IIR.
A nonpositive axis is skipped. Both nonpositive axes are a PDB calling error.

The encoded-RLE branch and denominator omit the positive endpoint weight,
while full RLE includes it. That endpoint can become 0 or 1 after floating
point evaluation, so a generic symmetric normalized convolution is not an
exact substitute. The port preserves both branches and their run threshold.

The final legacy REPLACE_INTEN shadow merge retains original RGB where the
new alpha is zero. That happens only once, after the two passes; intermediate
shadow rows are not merged back into the original drawable between passes.

Invalid inputs and undefined legacy arithmetic are rejected. The port checks
finite radii, supported methods, signed legacy byte/index limits, RLE `i*i`
and accumulator overflow, and finite IIR coefficients/results. It does not
silently replace an unsupported radius with a smaller one or another kernel.

## Verification

From the repository root:

```sh
c++ -std=c++14 -O2 -Wall -Wextra -Werror -pthread -Iapp/painter \
  app/painter/filter-gauss.cpp app/painter/tests/test-filter-gauss.cpp \
  -o /tmp/painter-filter-gauss
/tmp/painter-filter-gauss migration/fixtures/legacy-gauss/fixtures.tsv
```

This passed all 104 full-byte comparisons plus unit tests for captured small
IIR/RLE goldens, automatic RLE fallback, disabled axes, alpha filtering/hidden
RGB, argument validation, numerical limits, aliasing, and pre-start/running
cancellation without partial publication. Seventeen captured output hashes
are embedded in the test so the default unit suite also checks executable
parity when the optional manifest is absent.

The same suite passed with
`-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer`,
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, and
`UBSAN_OPTIONS=halt_on_error=1`. No leak-check claim is made.

## Repeating the capture

Build the pinned reference with `migration/baseline/legacy/README.md` and use
an initialized legacy profile. Relocate the absolute fixture-directory prefix
in a copy of `capture.scm`, keeping the original capture artifacts untouched.
The actual command used here, after sourcing the existing build environment:

```sh
source /workspace/shared/gimp-legacy-build/env.sh
export HOME=/workspace/shared/gimp-legacy-build/runtime-home
export GIMP2_DIRECTORY=/workspace/shared/gimp-legacy-build/runtime-home/profile
/workspace/shared/gimp-legacy-build/prefix/bin/gimp-2.8 \
  --no-interface --no-data --no-fonts --no-splash --new-instance \
  --batch-interpreter=plug-in-script-fu-eval \
  -b '(load "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/capture.scm")' \
  -b '(gimp-quit 0)' > capture.log 2>&1
```

The log retains the reference application's existing GValue/cache-size
warnings. They did not prevent the successful PDB calls and captured exports.
