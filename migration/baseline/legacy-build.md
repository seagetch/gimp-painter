# Legacy build: reference, observation and recovery

Reference: `afa43fae3e920210146abed514f136fd49f671b5` from
<https://github.com/seagetch/gimp-painter.git>, branch `gimp-2-8`.
The separate clean worktree used on 2026-10-01 is `../gimp-painter-legacy`.
Never switch the migration checkout back to the legacy branch to build it.

## Actual observation on 2026-10-01

`legacy/probe-2026-10-01.json` records the Debian 13 x86_64 environment, source
checksums, detected tool versions, pkg-config failures and actual bootstrap log.
The command `NOCONFIGURE=1 sh ./autogen.sh` exited 1 because libtool, gtk-doc,
autoconf, automake, intltool and xsltproc were missing. GTK2 / GEGL0.3 development
modules were not found. No configure/build/run/painting success is claimed.
A later dependency restoration requires a new probe; this dated observation
must not be rewritten to suggest it succeeded.

Reproduce the check (outputs only the caller-selected report; installs nothing):

```sh
git worktree add --detach ../gimp-painter-legacy afa43fae3e920210146abed514f136fd49f671b5
python3 tools/probe_legacy_build.py --source ../gimp-painter-legacy \
  --output /tmp/legacy-probe.json --bootstrap
```

## Available source pins are not a complete environment lock

`configure.ac` declares C++14, babl >=0.1.12, GEGL0.3 >=0.3.0,
GLib >=2.30.2, GTK2 >=2.24.10 and json-glib >=1.0. Additional dependencies
are inventoried in `../inventory/legacy-dependencies.tsv`. These are minima,
not exact reproducible dependency revisions.

The pinned source's Flatpak application manifest records:

| Dependency | Exact source revision | Declared tag |
| --- | --- | --- |
| babl | `e92ced2c54250af1dddb58f76ff1678d74a30dcf` | BABL_0_1_46 |
| GEGL | `29254dcd875d271043aba57d0e6ebc48ec63c013` | GEGL_0_3_34 |

Both URLs and pins are retained in `../inventory/legacy-flatpak-pins.tsv`.
However, GNOME Platform/SDK `3.28` has no immutable runtime commit in that
manifest, the gimp source is a moving branch, and base/dependency archives have
not all been retrieved and verified. The original manifest therefore cannot
close 02.001. A modern GEGL0.4 development package cannot satisfy `gegl-0.3`.

## Recovery procedure (independent child of 02.015)

1. **Restore tools and old ABI dependencies in an isolated prefix/container.**
   Use official distribution snapshots or official upstream releases; preserve
   checksums, signature verification, complete resolved package versions and
   base-image/runtime digest. Keep GEGL0.3 separate from modern GEGL0.4 and
   record the exact compiler, SDK, GTK2, GLib and babl actually used. Coordinate
   with modern-build work to avoid concurrent package-manager writes. A local
   dependency prefix is preferable to mutating the host system.
2. **Re-run the probe before building.** Bootstrap can use explicit
   `AUTOMAKE=automake ACLOCAL=aclocal` overrides if only newer supported tools
   are available; record the resolved versions. Do not claim the original
   automake-1.11 command ran in that case. Run `./autogen.sh --disable-gtk-doc`
   only with a recorded option change, and save configure, build and link logs.
   Keep optional-feature decisions visible, especially Script-Fu, the PDB
   procedures used by FilterLayer, and PNG export for baseline capture.
3. **Preserve compatibility patches.** Build failures from modern C++/GLib or
   other toolchains require a separately hashed patch series. Keep the pristine
   source revision and patch diff, and distinguish build-only adaptations from
   any change to rendering, file parsing or timing. If the latter are necessary,
   record an unresolved reference-fidelity risk rather than silently accepting
   changed behavior as the old baseline.
4. **Use an existing legacy executable when available.** Verify its source
   revision, build provenance, binary hash, dependency versions and feature set
   before trusting measurements. Do not substitute stock GIMP2.8 or a different
   gimp-painter revision solely because the version number looks similar.
   Run with an isolated empty user profile; never modify the user's old works
   in place. Record whether missing provenance limits a result to exploratory.
5. **Confirm startup, drawing and editable save/reopen.** Save startup logs,
   profile location, capture actions, one-stroke PNG/XCF results and hashes.
   Then capture the actual normal/CloneLayer/FilterLayer/composite fixtures,
   operation expectations and asynchronous traces in 02.003–02.013. An XCF
   manually encoded by a script tests a parser but is not a legacy writer run.
6. **Continue independent source extraction while runtime is unavailable.**
   `tools/freeze_legacy_assets.py` verifies source assets and
   `fixtures/strokes/FORMAT.md` defines the input/provenance contract. Source
   reader/writer formulas may inform implementation, with static evidence
   labels. Runtime compatibility tests remain incomplete and their dependent
   completion gates remain closed.

## Completion gates and current limits

- 02.001: still needs a verified complete isolated build/dependency lock
- 02.002: still needs a legacy executable and successful reproducible build log
- 02.003–02.007: no legacy startup/drawing/XCF capture results yet
- 02.008: independently checkable source-asset lock
- 02.009: independently checkable recording format; recording/replay adapter
  and measured output remain 02.010
- 02.010–02.013: no measured stroke/layer/rotation/scheduler traces yet
- 02.014: source rights/provenance register exists; runtime fixture permissions
  and provenance remain coupled to captures
- 02.015: this recovery procedure is available; the aggregate section gate
  stays open until the required runtime and provenance work is completed

## Recovery update

The later 2026-10-01 recovery successfully built and installed the old source,
then ran headless drawing and normal/CloneLayer save/reopen captures. See
`legacy/README.md` and `legacy/build-report.json` for the pinned dependency lock,
explicit adaptations, real outputs and remaining failures. The initial dated
probe above is historical; it is no longer the current dependency availability
status. FilterLayer execution/save succeeded, but the reference reader crashes
on its output, so successful FilterLayer roundtrip and later runtime gates
remain unverified.
