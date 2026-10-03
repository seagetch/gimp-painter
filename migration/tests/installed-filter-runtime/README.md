# Installed and relocated Blinds acceptance

WBS **34.005/installed-filter-smoke** passed on 2026-10-03. This is a fresh
Linux prototype's installed Filter acceptance, not a clean final release
candidate, complete Meson aggregate, GUI drawing test or whole-port sign-off.

## What actually ran

- Fresh task-local Meson configuration and default build: 1001 configured
  targets; 208 executable outputs checked, none missing or invalid. Two
  non-default optional executables were not built
- Six registered native Filter targets passed with no skip or timeout: all
  94 FilterLayer cases, wire validation, 15 process cases, 37 scheduler cases,
  11 spool cases, 320 genuine old Blinds comparisons and 12 native identities
- Twelve executable-selector cases passed both normally and with
  ASan/UBSan/float-cast-overflow. They cover build/test locations, moved install,
  symlink launch, missing/nonexecutable helper, missing plug-in, invalid selector
  and the compile-time instrumentation overlay
- The updated bridge runner passed four unit targets and 11 native cases,
  including its real instrumented helper and Blinds. Its 23 instrumented and
  41 RTTI-only source units had 1711 unchanged recorded inputs. Other GIMP and
  dependencies are ordinary; LeakSanitizer is off
- All 35 package recipe tests passed, including saved-capsule decoding and
  restoring an intentionally removed executable after a checker exception

The staged runtime contains 5270 sealed files, 419 ELF objects, 177 Painter
brushes and eight presets. Its dependencies resolve to bundled files except
three recorded baseline host libraries: libc, libm and libresolv. The runtime
was copied to a fresh path containing spaces and Japanese characters and run
from outside the source/build directory with an explicit minimal environment.
The original build remained accessible throughout the tests.

The relocated host launched its own helper; that helper queried and ran its own
Blinds plug-in. Explicitly invalidating the fixture's lower layer produced all
4096×4096 RGBA bytes with the genuine old-PDB SHA-256
`9a5228c92a2a65af74aeaee80517edb1aec7f2e310f968be169289af2c1e3be1`.
Save/reopen retained those bytes. This positive case took 6.03 seconds.

With the installed helper removed, no Filter helper or child ran. With Blinds
removed, only the installed helper ran and reported that exact missing path.
Both fresh-profile cases reached actual saved state FAILED, retained the
complete previous cache at generation 3 while the request was generation 4,
preserved the original procedure/definition/arguments, and saved/reopened the
previous pixels. They took 7.38 and 7.63 seconds. No wrong-installation process,
observed process survivor or recorded private helper profile remained. The
removed executables were restored, and all 5270 runtime files were rechecked
without a changed byte, mode or link.

## Identity and reproducibility

`acceptance.json` records source/input hashes, dependency locks, build and
installed executable hashes, exact normal results and instrumentation scope.
The three adjacent JSON/log pairs preserve process observation, actual XCF
hashes and the original diagnostics. Large builds, packages, source archives and
raw Meson logs remain local; no inherited process environment is committed.

The source baseline was published commit
`123fa97e742e6e73dfed3740df042fc84f86e6f5`. The complete staged source tree before
adding this acceptance record and WBS bookkeeping was
`a7475e55a47c05be5d2c31d7c72966efc00d5373`. Its task-file hashes are recorded and
unchanged after validation. This tree is a local source snapshot, not a
validation commit or publication. Generated Git-version text still identifies
the baseline. No normal binaries from the dirty main checkout were used.
The package manifest honestly remains a dirty prototype with separate source
and binary hashes and no source archive or full-aggregate correspondence claim.
All 4301 package-recorded source files remained unchanged after runtime checks;
focused instrumentation also left the ordinary binaries unchanged.

Executed commands, from the isolated source checkout (output paths must be new):

```sh
source /workspace/scratch/5b5281e79681/gimp-build-restoration/env.sh
export GIMP_BUILD_PREFIX=/workspace/scratch/5b5281e79681/.prefix-installed-filter
source tools/linux-debian13-env.sh
flock "$GIMP_PAINTER_BUILD_LOCK" meson setup build-installed-filter \
  --prefix="$GIMP_BUILD_PREFIX" -Dauto_features=disabled -Dlibunwind=false \
  -Dpainter-http=enabled
flock "$GIMP_PAINTER_BUILD_LOCK" meson compile -C build-installed-filter -j 4
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/verify_build_executables.py build-installed-filter
flock "$GIMP_PAINTER_BUILD_LOCK" python3 migration/tests/test_filter_executable_paths.py \
  --output ../installed-filter-validation/selectors-normal
flock "$GIMP_PAINTER_BUILD_LOCK" python3 migration/tests/test_filter_executable_paths.py \
  --sanitize --output ../installed-filter-validation/selectors-sanitizers
flock "$GIMP_PAINTER_BUILD_LOCK" meson test -C build-installed-filter --no-rebuild \
  --num-processes 1 --print-errorlogs painter-filter-procedure painter-filter-process \
  painter-filter-wire painter-filter-scheduler painter-filter-spool gimp-filter-layer
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/package-linux-runtime.py \
  --build build-installed-filter --deps "$GIMP_DEPS_DIRECTORY" --output ../installed-filter-package
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/test-linux-runtime.py \
  ../installed-filter-package/gimp-painter-linux-x86_64 --output ../installed-filter-relocated
python3 migration/tests/run_filter_process_sanitizers.py --build build-installed-filter \
  --output ../installed-filter-validation/bridge-sanitizers --run --lock "$GIMP_PAINTER_BUILD_LOCK"
```

The first setup used the same options. The compile regenerated the final Meson
layout changes before compiling all ordinary outputs. The declared Meson 0.61
minimum is retained: relative install edges are computed with the existing
Python dependency, rather than Meson's newer `fs.relative_to()`.

## Remaining gates

The ptrace probe was denied, so no syscall audit is claimed. The Filter route
uses actual process executable identities, not a file-access trace. Existing
Script-Fu resource warnings and two stale contexts at ordinary host shutdown
are preserved in the logs; native tests also retain configuration/localization
warnings. This is not a whole-application warning-free or leak-free result.
Other procedures, broader contexts/editing, native GUI drawing, physical tablet,
X11/Wayland equivalence, Windows/macOS, release build, full current-source
aggregate and distribution license/source delivery remain separate open gates.
Historical prototype and failed aggregate records are unchanged.
