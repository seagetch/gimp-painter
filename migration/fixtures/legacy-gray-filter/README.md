# Native Gray Edge/Gauss executable fixtures

Captured on 2026-10-02 with the existing pinned gimp-painter 2.8.23 executable
from `afa43fae3e920210146abed514f136fd49f671b5`. Neither the old plug-ins nor
the executable was patched or rebuilt. No old XCF reader was invoked.

`capture-report.json` records executable/source/script/log hashes, every case,
input and output hashes, exit status, completion markers, and negative indexed
probes. `capture.scm` and `capture.log` are the actual PDB execution evidence.
`fixtures.tsv` uses relative paths for portable byte comparisons. Raw `.y`
files hold one encoded Gray byte per pixel; `.ya` files hold Gray and alpha.
PNG files are native `L`/`LA`, profile-free, with no RGB-to-Gray conversion.
All nine independently loaded/exported inputs matched their original bytes,
including nonzero Gray under zero alpha. PNG export used `svtrans=1`.

## Captured cases

All 77 complete output buffers are compared byte-for-byte:

- All six Edge detectors and all three borders on mixed-alpha 5x4 input
- Degenerate 1x1, 1x5 and 5x1 input, Sobel/Laplace with every border
- 67x66 input crossing old tile boundaries
- Both Gaussian IIR/RLE methods, 25/25, 2.5/7.25, disabled-axis and small-radius
  fallback cases; full option sets are in `fixtures.tsv`
- Full-range opaque Gray, alpha 0/1, fully transparent hidden Gray, and plain
  Gray without alpha

Both old PDB procedures reject indexed images. The log proves TYPE=4 and
BASE=2 before each expected procedure failure. These are negative fixtures;
indexed compatibility is not inferred from grayscale support.

The pure executor test replicates native Y into the three independent byte
channels, executes the exact existing kernel, and checks equality of all three
channels plus every expected Y/alpha byte. This is channel algebra, not a Babl
luminance or ICC transform. The real FilterLayer adapter samples/imports native
`Y'A u8` in the drawable's own space and uses that same algebra. Separate actual
GIMP tests exercise both Gaussian methods and Edge using the opaque fixture,
default/custom linear/custom Lab Gray profiles, and an opaque source without
alpha. Profiles change interpretation of encoded bytes, not old kernel math.
The captures themselves are unprofiled, not an old-ICC capture.

These fixtures do not prove all lower-stack composition, selected-area merging,
high precision, arbitrary plug-ins, or UI responsiveness.

## Verification

```sh
meson test -C build-debian13 --logbase filter-gray painter-filter-gray
python3 migration/tests/run_filter_gray_sanitizers.py build-debian13 \
  --report migration/tests/filter-gray-sanitizers.json
```

The latter independently instruments both kernels and the corpus test with
ASan, UBSan and float-cast-overflow; system libraries remain uninstrumented.
Leak detection is disabled; this is not a leak-check claim.

To repeat into a NEW evidence directory, keeping this sealed capture unchanged:

```sh
source /workspace/shared/gimp-legacy-build/env.sh
python3 migration/tests/capture_gray_filters.py --capture \
  --output /tmp/painter-gray-reference-repeat
```

The helper refuses to overwrite a directory containing a capture report. Its
source/prefix arguments support a separately rebuilt matching pinned reference.
Absolute paths in the sealed Scheme script describe the original environment.
