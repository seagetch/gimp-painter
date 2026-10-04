# Retinex route acceptance

This directory records only WBS16.003/isolated-retinex-route. The source contract
is `../../contracts/filter-retinex.md`; the task starts from accepted commit
`ea840a5ab8b23491e46a4581d80d225272024538`. The historical worktree was read as
source material and was not edited or adopted wholesale.

`acceptance.json` gives the final bounded result, exact source identities and
remaining gates. Individual records retain their executed source/executable
hashes. These records are fresh current-code checks; the old-runtime provenance
in `../../fixtures/retinex-evidence.tar.gz` is unchanged historical evidence.
That compact archive links each retained member to its source archive and member
hash. Its manifest uses a `members` object keyed by archive path, with per-member
size/SHA256 and original source fields; derived indexes/TSVs have `derived_from`.

- `normal.json`: nine registered targets,124 actual native cases,320 Blinds,
  196 SmallTiles and174 Retinex genuine-old helper comparisons. The extra six
  padded-ROI compositions, two generated raw-shadow cases and four RGB-opacity
  rejection cases are analytic assertions, not additional old captures
- `current-pdb.json` and `current-pdb-observations.json`:32 public calls/signatures
  against the retained unmodified modern baseline,174 hidden calls and rejection
  of both interactive modes. Old zero-alpha hidden-color repair is explicit
- `allocations.json`: exact production functions extracted by the retained
  checker;174 hidden/32 public kernel fixtures,1,160 individual failures across
  src/dst/in/out/w1/w2, and34 analytic geometry/storage/recurrence rejections.
  Host/configuration stubs make this a focused source test, not a native PDB run
- `sanitizers.json`: the isolated helper, bundled procedures, typed storage,
  scheduler, owner context and selected native cases under ASan/UBSan/
  float-cast-overflow. It records the instrumented and RTTI-only source sets;
  dependencies and other GIMP units remain ordinary, and LeakSanitizer is off
- `selectors-normal.json` and `selectors-sanitizers.json`:22 literal path/layout
  checks each, including all three routes, missing/nonexecutable files, relocation
  and the private instrumentation overlay
- `package.json` and `installed-variant0.json` / `installed-variant1.json`: a fresh
  dirty Linux prototype, two genuine old unselected scenes, observed relocated
  Retinex helper/plug-in jobs, full raster equality and two Save/reopen cycles per
  variant. The real parameter edit after reopen is in the native test suite.
  Sealed runtime entries must stay unchanged; additional Python bytecode is
  reported separately, so there is no write-free-directory claim

Reproduction uses the configured Debian build environment and the shared
`GIMP_PAINTER_BUILD_LOCK`. Run the nine targets listed in `normal.json`,
`tools/capture_retinex.py`, `tools/check_retinex_allocations.py`, and
`migration/tests/run_filter_process_sanitizers.py`. Then converge the ordinary
default build, check `tools/verify_build_executables.py`, create a fresh prototype
with `tools/package-linux-runtime.py`, and run
`tools/check_installed_retinex.py --variant 0` and `--variant 1`, each with
`--relocate` and a fresh output path. Each checker retains its own complete
command and hash evidence; duplicated raw logs and large source archives are
not committed here.

The source remains a bounded U8 nonlinear RGB route: native RGB3/RGBA4,
scale16..256, nscales0..8, modes0/1/2, finite cvar0..4, selected dimensions each
at least16. Gray, higher precision, arbitrary-size/global-latency guarantees,
owner UI progress, disabled-swap alternatives, Windows/macOS process gates,
other procedure families, XCF multipart and whole-app/release acceptance remain
open. Conservative1GiB memory admission and configurable8GiB default logical
spill limits remain in force.
