# Debian 13 local-dependency build replay

This run is separate from the historical Ubuntu baseline. It uses the same GIMP
3.0 destination tree and pinned `gimp-data` gitlink, with dependencies from the
signed Debian snapshot `20260925T181338Z`. The package lock records the version,
architecture, filename and SHA-256 of every extracted archive. No package is
installed into the host OS, and no package maintainer script runs.

## Reproduction

An amd64 Debian 13 host needs Python 3, GCC/G++, `/usr/bin/apt-get`, `dpkg-deb`,
`pkg-config`, Git and the Debian archive keyring. All generated files are local.
The dependency cache is approximately 275 MB; extracted files approximately
1.2 GB. The full lock includes GTK2/autotools dependencies for legacy comparison.

From the repository:

```sh
python3 tools/prepare-linux-build-deps.py
source tools/linux-debian13-env.sh

git submodule update --init --depth=1 gimp-data
# The checked-out gitlink must remain a31e5dea4f572fb5f33b06f5c4b86db5f30643dc.
meson setup build-debian13 \
  --prefix="$(dirname "$PWD")/.build-prefix-debian13" \
  -Dauto_features=disabled -Dlibunwind=false
meson compile -C build-debian13 -j 4
python3 tools/verify_build_executables.py build-debian13 \
  --repair-permissions --report build-debian13/executable-check.json
meson install --no-rebuild -C build-debian13
```

For a different cache directory, supply `--directory /absolute/path` to the
Python helper and export `GIMP_DEPS_DIRECTORY=/absolute/path` before sourcing
the environment. `GIMP_BUILD_PREFIX` similarly selects an install prefix; give
the same prefix to Meson. `--offline` validates and extracts only previously
downloaded archives, failing on a missing or mismatching archive. It does not
silently fall back to another source. Downloads always use signed APT indexes;
the helper does not disable signature verification.

The local run used `/workspace/shared/gimp-build-deps` as its cache. A second,
separate extraction directory tested the helper's offline replay. Prefer a new
build directory for a clean compile and set `CCACHE_DISABLE=1`; an existing
incremental directory does not establish clean-build reproducibility.

## Dependency relocation

The helper applies these local packaging adaptations, without changing GIMP:

- Relocate `.pc` paths to the extracted prefix. `pkgconf --define-prefix` alone
  incorrectly derives `/usr/lib` for Debian's multiarch metadata layout
- Expose the local include/library roots via `CPATH` and `LIBRARY_PATH`, so
  Meson probes without pkg-config dependencies and direct compiler searches
  find the same headers/libraries as dependency-based compilation
- Relocate Debian's GI architecture-wrapper tool/data paths, retaining the host
  Python shebang
- Process extracted Python package `.pth` files with a small `sitecustomize`,
  enabling setuptools' distutils shim on Python 3.13
- Select the extracted gettext ITS rule directories through `GETTEXTDATADIR`
  and `GETTEXTDATADIRS`, so metainfo translation merging can find its rules
- Pass LCMS's two static plug-in archives to `g-ir-scanner` as `--extra-library`
  instead of `-l`. Both archives stay on the scanner's link command; they are
  excluded only from the list of shared libraries to `dlopen`. The LCMS `.pc`
  file and GIMP's actual library/executable linker flags remain unchanged

The LCMS workaround applies only when the locked static archives exist and no
matching `.so` files exist. Four unit tests cover relocation idempotence,
preserved link-only arguments, the shared-library guard and Python bootstrap:

```sh
python3 migration/tests/test_linux_build_deps.py
```

`tools/linux-debian13-env.sh` additionally selects the extracted modern babl and
GEGL operation directories at runtime. Legacy builds should use the generated
cache `env.sh` with their own legacy prefix and operation paths instead.

## Isolated smoke and standard tests

Use a fresh profile within the workspace, never the user's existing profile:

```sh
source tools/linux-debian13-env.sh
run_dir="$(dirname "$PWD")/.gimp-debian13-check"
mkdir -p "$run_dir"/{home,config,cache,data}
HOME="$run_dir/home" XDG_CONFIG_HOME="$run_dir/config" \
XDG_CACHE_HOME="$run_dir/cache" XDG_DATA_HOME="$run_dir/data" \
GIMP3_DIRECTORY="$run_dir/gimp" \
GIMP_BASELINE_XCF="$run_dir/baseline-smoke.xcf" \
  ../.build-prefix-debian13/bin/gimp-console-3.0 -n -c \
  --batch-interpreter=python-fu-eval -b - --quit \
  < migration/tests/baseline-smoke.py

HOME="$run_dir/home" XDG_CONFIG_HOME="$run_dir/config" \
XDG_CACHE_HOME="$run_dir/cache" XDG_DATA_HOME="$run_dir/data" \
  meson test -C build-debian13 --no-rebuild --print-errorlogs --suite gimp:app
```

The smoke test checks create/fill/save/reopen and layer identity, not brush
replay, UI interaction, or painter-specific compatibility. Keep those claims
separate. The historical Ubuntu app-suite failures are documented in
`../standard-tests.md`; compare newly observed failures before attributing them
to painter code.

## Recorded outcome, 2026-10-01

The unchanged baseline configured, compiled and installed successfully. All
122 default-built ELF executables passed validation; none required permission
repair. Two optional non-default executables were not built. The installed
console smoke returned 0 and printed `BASELINE_CREATE_FILL_SAVE_REOPEN_OK`.
The XCF was 1,087 bytes; its hash is in `build-environment.json`.

The app suite again passed `core`, `gimpidtable`, `xcf` and `app-config`.
`save-and-export` failed with the same historical `AttributeError` at batch
line 71 (`image.get_imported_file()` was `None`), and its run again reported a
`script-fu` segmentation fault. These are observations from the unmodified
baseline, not painter regressions. This does not establish their root cause.

The installed smoke also repeated the nonfatal GLib `g_file_test` startup
critical and reported 112 unsupported fonts. App tests reported a
writable-data-folder/search-path mismatch. Tests were headless; GUI interaction
was not checked.

The build directory was initially new, then incrementally resumed after local
dependency fixes. A second fresh dependency extraction passed the helper's
offline replay and a real GObject C++ compile/link/run check. A completely fresh
GIMP compile using the finalized recipe has **not** been run. Full GIMP
sanitizers, painter compatibility and stroke replay remain outside this result.

`build-environment.json` records versions, commands, hashes and stage outcomes.
`build-final-pass.log`, `smoke.log`, `app-test-result.log` and
`build-artifact-check.json` preserve targeted evidence. Full local logs remain
at the paths in the JSON. Complete Meson test logs include inherited environment
variables and are intentionally not copied into Git. The app suite also writes
an untracked `app/tests/gimpdir/pluginrc`; the generated file was moved outside
the checkout after testing.
