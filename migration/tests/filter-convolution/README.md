# Convolution route acceptance

Read `acceptance.json` for the bounded task result and exact validation scope.
The old fixture is `../../fixtures/legacy-convolution.tar.gz`; verify/extract it
with `python3 tools/check_convolution_evidence.py --extract EMPTY_DIRECTORY`.
The derived `cases.tsv` is a consumer convenience generated after all archive,
provenance and pixel-buffer hashes are verified. It is not an extra old capture.

With the restored Debian13 environment active, normal checks use the default
build, `meson test --suite painter`, and the final affected-target run of
`gimp-filter-layer`, `painter-filter-process`, `painter-filter-wire` and
`painter-filter-context-native`. The wrapper supplies verified immutable
fixtures. The broad regression includes unrelated environment-gated skips;
these are reported explicitly and are not a release acceptance claim.

The isolated helper's Convolution test compares296 successful genuine old raw
ROIs and final merges, plus8 expected geometry rejections. The41 additional helper
requests comprise36 native double/TRC/Gray/HDR cases, one safe one-row/right-edge
CLEAR case and four numeric boundary inputs matching the separate old evidence.
Two small supplemental archives contain8 genuine old calls, including the four
ignored twelfth-argument int/string cases. The modern precision and safe-border
checks are analytic extensions, not extra old oracle outputs. The live test driver
checks12 genuine old FilterLayer scenes, invalid typed shapes/cache retention,
34 native precision/model/TRC/profile combinations, native arithmetic failure,
explicit cancel/replacement/close, native HALF/FLOAT storage overflow/recovery,
and typed-array Save/reopen/reedit/Undo/Redo including the ignored twelfth slot.
The owner-context test adds154 explicit capture/merge cases. These are
programmatic APIs, not GTK array-editor acceptance.

`run_filter_process_sanitizers.py` builds instrumented copies from the normal
compile database and links a private helper/plugin overlay. It never replaces
the ordinary installation. Its report distinguishes instrumented sources from
RTTI-only and uninstrumented GIMP/dependencies; LeakSanitizer is disabled.
`test-convolution-kernel.c` additionally checks400,000 source-derived float
arithmetic cases, native HDR/double precision and invalid arithmetic.

`gegl-comparison.json` is an intentionally mismatching backend comparison, not
a failing migration test. Reproduce in a fresh output directory with
`run-gegl-comparison.sh OUTPUT VERIFIED_FIXTURES_JSON` after loading the pinned
GEGL0.4.62 environment. The settings, orientation, loaded module hashes, actual
old expected hashes and differing bytes are all recorded.

The installed smoke is owned by `tools/check_installed_convolution.py`. Inputs
come from `gimp-filter-layer -p /gimp-filter-layer/convolution_save_reopen` with
`GIMP_PAINTER_CONVOLUTION_EXPORT` set to an output directory. It exports129×97
U8 and Double XCF/raw pixels; the Double result retains bits below float
precision. The relocated smoke observes both helper and bundled plugin for
each rerun and verifies two Save/reopen cycles, exact typed arrays, pixels and
GIMP3/GTK3/GEGL0.4 runtime ABI. No legacy runtime is shipped.

The first instrumented live run found a test-only stack-backed static-GBytes
literal. Its corrected literal is static storage. The unchanged production and
helper corpus receipts were retained; `rerun-live-after-fixture-fix.py` verified
that only this declared include changed, replayed the exact original driver
compile/link commands, and reran all50 selected live cases. The combined report
keeps both source epochs explicit. Fresh normal exported XCFs were regenerated
only after that fix; the installed smoke does not reuse the earlier inputs.

The sealed `installed-inputs.tar.gz` contains the corrected synthetic XCFs,
source/expected pixels and manifest (seven members, hashes in the companion
manifest). Extract into a fresh directory, then run
`python3 tools/check_installed_convolution.py --bundle BUNDLE --inputs DIR --output NEW_DIR`.
`installed-development-failure.json` records the earlier stale/unrelinked host
setup, excluded from acceptance. The packaging recipe now completes the normal
default Ninja build before prototype installation, and its35 recipe tests pass.
