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

The configuration disables optional auto-detected plug-ins for a lean baseline;
this does not assert that those plug-ins are implemented or compatible.
Generated build directories, binaries and package cache stay outside Git
history. If the container's apt sandbox refuses to change to `_apt`, use
`-o APT::Sandbox::User=root` and a writable `Dir::Cache::archives` directory.

The baseline batch fixture at `migration/tests/baseline-smoke.py` creates an
image, fills a layer, saves an XCF and reopens it. In this execution container
the batch prints `BASELINE_CREATE_FILL_SAVE_REOPEN_OK` and saves the file,
but the process returns status 1 at shutdown. A D-Bus session cannot be
started here (`Failed to open socket: Operation not permitted`). Accordingly,
the exit part of WBS 03.004 is still open; a full runtime success is not claimed.
