# Linux baseline build (Ubuntu 24.04, x86_64)

The local installation used the Ubuntu snapshot dated 2026-08-28. System
dependencies were installed with the following command after `apt-get update`:

```sh
apt-get install --no-install-recommends meson ninja-build pkg-config \
  libgtk-3-dev libgegl-dev libbabl-dev libgexiv2-dev libappstream-dev \
  libarchive-dev libmypaint-dev mypaint-brushes libtiff-dev libjpeg-dev \
  libpng-dev liblzma-dev libxmu-dev libxfixes-dev libexiv2-dev \
  libjson-glib-dev libgirepository1.0-dev gobject-introspection \
  xsltproc libxml2-utils gettext librsvg2-dev libpoppler-glib-dev \
  poppler-data python3-gi xvfb xauth dbus-x11
```

The distro babl 0.1.108 and GEGL 0.4.48 are below the checked-out GIMP
3.0 branch's minimum versions 0.1.114 and 0.4.62. The build script installs
those exact source tags into a private sibling `.build-prefix`, then builds
the `gimp-data` submodule at its pinned gitlink and configures GIMP. Run
`tools/build-linux-baseline.sh` from the repository. It checks commit IDs
before reusing previously cloned dependencies, builds and installs GIMP into
the local prefix. For subsequent commands, run
`source tools/linux-build-env.sh` to set pkg-config, runtime library, GI
typelib and plug-in Python search paths.

The script verifies every default Meson executable and restores missing
execute bits only on valid ELF outputs. Any missing or non-ELF executable
stops the build before installation.

The configuration disables optional auto-detected plug-ins for a lean baseline;
this does not assert that those plug-ins are implemented or compatible.
Generated build directories, binaries and package cache stay outside Git
history. If the container's apt sandbox refuses to change to `_apt`, use
`-o APT::Sandbox::User=root` and a writable `Dir::Cache::archives` directory.

The baseline batch fixture at `migration/tests/baseline-smoke.py` creates an
image, fills a layer, saves an XCF and reopens it. Run the console binary
directly for this non-UI test; `xvfb-run` yielded exit status 1 in this
container although the batch itself succeeded. Direct headless execution
prints `BASELINE_CREATE_FILL_SAVE_REOPEN_OK` and returns status 0. The XCF
is preserved as `migration/fixtures/gimp3-baseline.xcf`. The container
cannot create a D-Bus session socket, so GUI tests still need a suitable
environment. A nonfatal GLib `g_file_test` critical occurs during startup;
its source has not been established and is tracked separately from the
successful batch exit.

The original app test suite and its one baseline failure are recorded in
`standard-tests.md`; the Meson executable inspection results are in
`build-artifact-check.json`.

To reproduce GIMP compilation without a previous GIMP build directory or a
compiler cache, use a **new** build directory and the installed, pinned babl
and GEGL prefix. Do not reuse an existing `build-clean` directory:

```sh
source tools/linux-build-env.sh
test ! -e build-clean
export CCACHE_DISABLE=1
meson setup build-clean --prefix="$(dirname "$PWD")/.build-prefix" \
  -Dauto_features=disabled -Dlibunwind=false
meson compile -C build-clean -j 2
python3 tools/verify_build_executables.py build-clean
```

This records the cache-free clean build procedure; the validated installed
baseline above was compiled in `build`, not `build-clean`.

```sh
source tools/linux-build-env.sh
GIMP3_DIRECTORY="$(dirname "$PWD")/.gimp-baseline-check" \
GIMP_BASELINE_XCF="$(dirname "$PWD")/baseline-smoke-check.xcf" \
  ../.build-prefix/bin/gimp-console-3.0 -n -c \
  --batch-interpreter=python-fu-eval -b - --quit \
  < migration/tests/baseline-smoke.py
```
