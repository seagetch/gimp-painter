# Recovered Linux legacy reference (2026-10-01)

The original missing-dependency probe is retained in `probe-2026-10-01.json`.
Recovery has now produced a real, installed gimp-painter **2.8.23** executable
from pinned source `afa43fae3e920210146abed514f136fd49f671b5`.
`build-report.json` records source, package, compiler, patch and binary hashes.
`build-logs.tar.gz` retains successful commands' output and the failed attempts;
failed harness setup attempts are not acceptance evidence.

## Rebuild recipe

On Debian13 amd64, acquire the signed snapshot packages with the existing local
extractor, using **this directory's lock**, which also includes legacy GTK2,
gettext/autopoint and gtk-doc tooling:

```sh
python3 tools/prepare-linux-build-deps.py --directory /path/to/deps \
  --lock migration/baseline/legacy/debian13-package-lock.json
```

Use separate clean checkouts of the exact three commits in `build-report.json`.
Their upstream repositories are seagetch/gimp-painter and GNOME/babl, GNOME/gegl.
Then run:

```sh
tools/build_legacy_reference.sh /path/to/deps /path/to/legacy-build \
  /path/to/gimp-legacy /path/to/babl-legacy /path/to/gegl-legacy
```

The recipe packages the commands actually used during recovery, including the
same optional dependency exclusions. It was syntax-checked; the individual
bootstrap/build/install stages executed successfully in this environment.
A second pristine end-to-end replay has not yet been measured. This is a
source-build recipe and dependency lock, not a claim of bit-identical binaries
across directories or machines. Host compiler bytes were checked against the
signed downloaded compiler packages and matched exactly.

`prepare_legacy_build_env.py` makes local relocation wrappers for Debian's
hard-coded autotools data paths. It changes no system configuration and runs
no package maintainer scripts. Its unit tests cover deterministic hashes,
unchanged input tools, missing inputs and unsafe paths.

## Every adaptation is explicit

| Adaptation | Reason and boundary |
| --- | --- |
| `gimp-autoconf72-quoting.patch` | Quote a nested Linux-input header/declaration m4 check so Autoconf2.72 generates valid shell |
| `gimp-glib-atomic-qualifier.patch` | Remove obsolete volatile on the g_once-managed type-ID scalar; synchronization remains g_once |
| `gimp-overlay-declaration.patch` | Include existing overlay/rotation declarations before their existing C calls |
| `gimp-backtrace-declaration.patch` | Include execinfo declarations under the already detected HAVE_EXECINFO_H |
| `gimp-preset-type-deduction.patch` | Use auto for the exact return pointer type rather than illegally naming a protected nested type |
| `gegl-gtkdoc-regeneration.patch` | Record gtk-doc's build-macro regeneration; no GEGL rendering source changed |
| C++ `-include type_traits` | Load the standard header before legacy extern-C include blocks and modern GLib's typeof header |
| C `-Wno-error=incompatible-pointer-types` | Retain GCC14's diagnostic as a warning for the existing derived/base-pointer call; no expression changed |
| GEGL `-fcommon` | Restore historical common-symbol behavior for gcut duplicate globals |

Neither XCF reader/writer, compositing math, brush logic nor scheduler decisions
were changed in the reference executable. Capture instrumentation is a separate
patch in `../../fixtures/legacy-runtime/`, with a separate harness binary hash.

## Observed tests and limits

- babl0.1.46: 23/23 tests passed with explicit isolated loader paths
- GEGL0.3.34 simple suite: 28 passed, two skipped, one failed. The failed
  `test-gegl-tile` replaces data on an allocated tile, then reaches a mismatched
  deallocation path. The original source is retained
- Legacy application headless startup, controlled brush stroke, XCF save/reopen
  and PNG export succeeded. The normal layer's 75% opacity reopens at the old
  8-bit value 191/255, as asserted by the capture
- Both normal-in-group and group-source CloneLayer fixtures saved/reopened with
  their custom type and source reference. Additional pixel/reference/geometry
  observations are preserved in the runtime fixture package
- The original `test-xcf` runner aborts on a startup GValue critical because
  GLib tests make critical messages fatal. The capture harness uses normal
  application critical-message behavior and retains the warnings in its log;
  it does not count the original suite as passed
- A real FilterLayer plug-in-edge run completed and saved its output. Reopening
  crashes in both the harness and the uninstrumented old application. This is a
  genuine negative reader fixture, not a successful FilterLayer roundtrip
- GUI startup was not verified: Xvfb could not create a local listening socket.
  The successful captures use the application's supported `--no-interface`
  path. No tablet interaction or UI responsiveness measurement is claimed

Startup GValue and obsolete GEGL cache-size warnings and headless GUI-registry
warnings remain visible. A successful controlled headless capture does not
remove those compatibility risks or close all section02 runtime gates.
