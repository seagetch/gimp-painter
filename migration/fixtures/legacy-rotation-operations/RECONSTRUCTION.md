# Replay dependencies and reconstruction

## Offline evidence checks

No build, display, dependency extraction, or network is required to run:
python3 test_rotation_capture.py

The package contains all TSVs, raw GUI logs/results, extracted original source
files, source driver and hashes required by those 16 checks. The manifest covers
every exported file except itself. These checks preserve both -11 process exits.

## Existing isolated-build replay

The historical workspace has the following layout (absolute prefixes are in
recorded-env, dependencies.txt and the build log):

- legacy-rotation-operations-20261015/: this package plus gimp/ build tree
- legacy-filter-positive-20261015/deps/: extracted locked dependencies, env.sh
- legacy-filter-positive-20261015/build/: generated env.sh and installed prefix
- legacy-filter-positive-20261015/gimp/: prior build used for archive comparison

Scripts build.sh, replay_source.sh and run_gui_fresh.sh source the sibling
build/env.sh. That environment in turn sources deps/env.sh and sets loader,
compiler, pkg-config and babl paths. Only the templates are packaged; the
installed libraries and compiled objects are not embedded. Dependency and build
environments must be regenerated at another location, not blindly copied.

run_gui.sh is the unchanged historical capture script. Use run_gui_fresh.sh for
new runs. The new helper requires one simple unused output-directory name and
refuses existing files/directories/symlinks atomically; it never removes them.
The existing captures were separate initially empty output directories. Output
profile/ is retained as GIMP2_DIRECTORY; only the runner's temporary HOME/XDG,
authorization, and server are removed. Do not equate retained output/profile
with a fresh profile on reusing the historical wrapper.

## Full source reconstruction inputs

The following inputs are required but not embedded as compiled binaries:

- GIMP-Painter Git tree, afa43fae3e920210146abed514f136fd49f671b5
- babl Git tree, e92ced2c54250af1dddb58f76ff1678d74a30dcf
- GEGL Git tree, 29254dcd875d271043aba57d0e6ebc48ec63c013
- amd64 Debian 13 host tools: Python 3, C/C++ compiler, make, apt-get, dpkg-deb,
  pkg-config, Debian archive keyring; X display validation also uses xauth,
  xdpyinfo and ss. See the locked package list for dependency versions.
- Package archives from reconstruction/migration/baseline/legacy/
  debian13-package-lock.json. Its signed snapshot references, package versions
  and SHA-256 values are retained. Prior replay cache-verification.json records
  all 560 checked archives. Download/cache access is needed if absent locally.

reconstruction/ includes the complete dependency-extraction helper, original
build recipe, tool relocation helper, every baseline patch, package lock,
previous replay report, cache checks and generated-tool provenance. Their
original repository-relative layout is preserved so the build recipe can find
its patch directory. inputs.json records their hashes. Historical environment
files are included for comparison only.

Use prepare-linux-build-deps.py --help and pass its explicit --lock pointing to
the LEGACY package lock above (its generic default is a different lock). Then
run the packaged build recipe with its five documented arguments:

bash reconstruction/tools/build_legacy_reference.sh DEPS BUILD GIMP BABL GEGL

This is the prior independently exercised baseline recipe, not a claim that this
revision reran an entire dependency build. It checks exact Git pins, builds and
installs the original app, and preserves the known GEGL test failure. Preserve
that installed original executable as the independent reader/reference.

The recipe applies baseline GIMP compatibility patches. The complete
source-overlay.patch already contains those changes plus inherited filter
observation and this test entry point. Do not apply it on top of baseline changes.
In a NEW copy of the built original GIMP tree, first reverse the already-applied
baseline gimp-*.patch files (git apply -R --check followed by git apply -R for
each, in reverse order). Verify tracked source now matches the pin. Then apply
source-overlay.patch with git apply --check followed by git apply. Do not change
the independent installed original or its source copy. Build changed objects
with the same generated environment and original flags before linking test-ui;
a full make -j4 in this new source tree is appropriate. Do not run make install
for the capture tree, because it would replace the independent reference.

Copy this package's helpers beside the new gimp/ tree. Either preserve the sibling
layout above or explicitly relocate helper environment paths to your regenerated
build/env.sh. build.sh relinks test-ui with the recorded extra original archives
and flags. run_gui_fresh.sh then creates a new profile/display/output. Expect
capture completion followed by the documented nonzero teardown result; compare
that actual outcome rather than assuming success. The exact crash instruction
was not diagnosed.

## Provenance/export-only dependencies

prepare_source_capture.py requires a Git-capable pinned gimp/ tree and verifies
whole source bytes before extracting bodies. replay_source.sh needs cc and Cairo
headers/libraries. collect-provenance.py additionally compares prior original
archives in the sibling gimp/ tree. package.py requires the isolated binaries,
original reference executable, raw primary/repeat outputs and this package's
reconstruction directory. These are requirements for regenerating provenance;
they are not requirements for the offline evidence checks.

Build-tree object/archive byte identity is specific to the recorded build.
Another fresh build may differ in paths/tool output while retaining the pinned
source and measured behavior. Report newly produced hashes without replacing
historical values or claiming historical binary identity.
