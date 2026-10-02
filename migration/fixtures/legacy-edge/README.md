# Legacy plug-in-edge byte fixtures

Captured on 2026-10-02 from the real pinned gimp-painter 2.8.23 executable at
`afa43fae3e920210146abed514f136fd49f671b5`. This package is separate from the
sealed `legacy-runtime` XCF/scheduler fixture package.

## Scope and provenance

`capture-report.json` records the original executable hashes, parameters,
input/output SHA256s, script/log hashes, exit status, and all 76 completion
markers. `capture.scm`, `capture.log`, `cases.json`, and `capture-fixtures.tsv`
are unchanged capture artifacts; their absolute paths describe the original
capture environment. The report's `fixtures_tsv_sha256` refers to
`capture-fixtures.tsv`. `fixtures.tsv` is the same matrix with a commented
header and local relative paths for the independent C++ regression test.

Inputs are tightly packed, straight RGBA8 with varied RGB and alpha values
0, 1, 63, 127, 191, 254, and 255. Input PNGs have no color profiles. Each input
was loaded and exported before filtering; all five decoded images matched
the supplied raw bytes exactly, including RGB under zero alpha. Output PNG
export used `file-png-save2` with `svtrans=1` and metadata options disabled.
All `.rgba` files are lossless decoded PNG bytes, without numerical color
transforms. Both encoded and decoded output hashes are retained.

- 5x4, 1x1, 1x5, and 5x1: all six detectors and all three borders at amount 1.75
- 67x66: Sobel and Laplace with wrap and black borders at amount 2, crossing
  legacy 64-pixel tile boundaries
- All 76 outputs compare byte-for-byte with the CPU compatibility executor
- This exercises the whole-drawable PDB plug-in path, without a selection;
  it is not evidence for FilterLayer scheduling, selected-area blending,
  other image precisions, grayscale conversion, or arbitrary filter plug-ins

## Observed transparent-pixel behavior

The detector calculates each RGB channel independently and samples stored RGB
even from transparent neighbors. The plug-in then merges its shadow at full
opacity through `GIMP_REPLACE_MODE`. The active legacy
`paint-funcs.c:replace_inten_pixels` preserves the original destination RGB
when the resulting alpha is zero. The compatibility executor reproduces both
behaviors. Before this shadow-merge detail was included, 2,636 pixels differed
across the 76 cases, all and only at fully transparent pixels. No nonzero-alpha
pixels differed. The committed expectations are the observed executable bytes,
not regenerated output from the replacement implementation.

## Independent regression test

From the repository root:

```sh
c++ -std=c++14 -O2 -Wall -Wextra -Werror -pthread -Iapp/painter \
  app/painter/filter-edge.cpp app/painter/tests/test-filter-edge.cpp \
  -o /tmp/painter-filter-edge
/tmp/painter-filter-edge migration/fixtures/legacy-edge/fixtures.tsv
```

This passed all 76 full-byte comparisons and the standalone unit tests.
The unit tests also cover source-equivalent minimum-amount clamping,
fractional truncation, saturation, negative Laplace responses, dimension and
buffer validation, invalid/nonfinite parameters, aliasing, and cancellation
without publishing partial output. Eighteen small runtime golden hashes are
embedded, so the unit suite still provides executable-derived evidence when
invoked without the optional fixture manifest.

The same tests passed with
`-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer`,
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, and
`UBSAN_OPTIONS=halt_on_error=1`. LeakSanitizer was not available under the
execution environment's ptrace restrictions; a leak check is not claimed.

Finite amounts so large that legacy float-to-int conversion was undefined
are safely saturated before conversion by the compatibility executor. No
legacy numerical parity is claimed for those undefined conversions.

To repeat the capture, build the pinned legacy reference using
`migration/baseline/legacy/README.md`, relocate the two original input/output
directory prefixes in a copy of `capture.scm`, and run it with the legacy
`plug-in-script-fu-eval` batch interpreter. Keep the original artifacts intact.
