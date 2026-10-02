Gimp Painter 3.0 migration: Debian 13 x86_64 runtime prototype
==========================================================

This is a locally prepared migration snapshot, not a completed port or release.
Read build-manifest.json for its exact source/build hashes and validation status.
A dirty prototype does not have a complete corresponding source archive and
must not be redistributed. The final source and aggregate gate are required.

Requirements: Debian 13 x86_64, glibc 2.41, Python 3.13, an X11 or Wayland GTK3
session, standard system fonts/fontconfig data, and the host runtime libraries
listed by SONAME in build-manifest.json. The package bundles its GIMP libraries,
plug-ins, translations, 177 Painter brushes, layer presets, GEGL/babl operations,
GTK3/GObject runtime closure and Python GI module. It is not an AppImage and does
not include a Linux kernel, display server, glibc or Python interpreter.

Extract the whole archive into any user-writable directory, then run:
  ./gimp-painter-linux-x86_64/AppRun --new-instance
For a command-line version check:
  ./gimp-painter-linux-x86_64/AppRun --version
For batch mode:
  ./gimp-painter-linux-x86_64/AppRun --console [GIMP options]

Do not source the developer's environment or run the binaries directly.
AppRun sets relative library/resource paths. The original GIMP install and
profile are not replaced. Defaults use ~/.config/gimp-painter-prototype/3.0
and ~/.cache/gimp-painter-prototype. Override GIMP_PAINTER_PROFILE and
GIMP_PAINTER_CACHE for isolated smoke testing or a second copy.

The underlying build is currently debugoptimized with optional auto-detected
features disabled. Optional formats may be absent. HTTP build availability is
recorded in the manifest; the service is not started by the launcher. No system
installation, desktop registration, persistent service or network change occurs.

Validation is evidence-specific. Linux drawing/save/reopen cannot establish
Windows/macOS, real tablet input, Wayland, production performance or complete
legacy compatibility. See the platform gate report for all unexecuted gates.

Licenses are in licenses/, with retained source notices within installed assets.
When source_archive_complete is true, matching GIMP source and pinned gimp-data
source archives accompany the local bundle. Otherwise this is incomplete.
Debian binary package versions/checksums and corresponding source package/version
indexes are in build-manifest.json. Dependency source archives are not bundled;
the recorded source indexes must be verified and applicable source-distribution
obligations satisfied before any external distribution. No license conclusion
is implied for inherited legacy assets lacking a verified original notice.
