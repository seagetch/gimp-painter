# Unmodified GIMP 3.0 app baseline tests

After building GIMP 3.0.9 on the `gimp-3-0-port` upstream base, run:

```sh
source tools/linux-build-env.sh
python3 tools/verify_build_executables.py build \
  --repair-permissions --report migration/baseline/build-artifact-check.json
meson test -C build --no-rebuild --print-errorlogs --suite gimp:app
```

On 2026-09-27 the executable inspection passed for all 121 built ELF targets;
two optional, non-default test executables were not built. The app suite
reported 4 successes out of 5: `core`, `gimpidtable`, `xcf`, and `app-config`
passed. `save-and-export` failed with exit status 1. Its Python batch loaded
`gimp-data/images/logo/gimp64x64.png`, then raised `AttributeError: 'NoneType'
object has no attribute 'equal'` at line 71 of
`app/tests/test-save-and-export.py`, where it expected
`image.get_imported_file()` to be non-null. The log also reported a `script-fu`
segmentation fault in that run. This suite ran before any painter-specific C
or C++ code was ported; keep these two observations as baseline failures
until their causes are determined. The separate direct headless XCF smoke
test passed as documented in `linux-build.md`.

An earlier test attempt exposed a zero-filled `build/app/tests/xcf` binary and
several ELF targets missing execute permission. Regenerating `xcf` and setting
execute permission for the affected ELF files allowed all four passing tests
to run. A subsequent Meson rebuild again produced valid ELF files without
execute bits; `--repair-permissions` restores these bits only after confirming
the ELF magic. The executable check is part of `tools/build-linux-baseline.sh`;
invalid contents still fail the build.

The complete Meson output is regenerated locally at `build/meson-logs/testlog.txt`.
No painter migration regression can be inferred from this unmodified baseline.
