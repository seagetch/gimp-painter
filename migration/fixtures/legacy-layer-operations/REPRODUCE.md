# Validate the old seal, or create a new capture

These are different workflows. The nine Python tests against the committed
fixture validate the existing sealed 2026-10-15 evidence. They do not build or
execute GIMP. Running `run.sh` creates new measurements; running `package.py`
exports those measurements and replaces the destination manifest. A test pass
on a newly exported package must never be presented as validation of unchanged
old artifacts or as a bit-identical independent build.

## A. Validate existing sealed evidence only

From the maintained repository (or the delivered directory containing migration):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s migration/tests -p 'test_legacy_layer_operations_fixtures.py' -v
```

Requires Python 3 standard library only. It checks file hashes, raw byte sizes,
log-derived observations, primary process exit-code files and completion markers,
repeat logs/hash equality, pointer and core-ID relationships, notification
routing, and the measured reader successes/failures. It does not infer reopened
reference/pixel parity: installed-reader checks only assert image dimensions and
report root-layer counts.

## B. New measurement using this workspace's existing isolated builds

The actual captured environment is `/workspace/scratch/5b5281e79681`. Required
layout (source/build and installed prefix are not bundled in this 4.8 MiB fixture):

```
/workspace/scratch/5b5281e79681/
  recovery-ui-20261015/source/       maintained repository, read only here
  legacy-filter-positive-20261015/
    deps/env.sh                    extracted signed dependency environment
    deps/root/                     extracted dependency libraries/tools
    build/env.sh                   generated absolute-path environment
    build/prefix/                  babl, GEGL, installed original GIMP/plugins
  legacy-composite-20261015/gimp/   existing configured built original checkout
  layer-operations-replay/          NEW directory, created by commands below
    gimp/                          separate copied original source/build
    capture-layer-operations.c
    build.sh, run.sh, independent.sh, package.py, collect-provenance.py
    captures/{clone,filter}/        created by run.sh
    repeat-captures/{clone,filter}/ created by run.sh repeat-captures
    deliverables/migration/fixtures/legacy-layer-operations/
    deliverables/migration/tests/test_legacy_layer_operations_fixtures.py
```

Run from Bash. Choose a nonexistent OUT so none of the original evidence is
modified. The exact source exporter is included as `package.py`; it uses only
Python's pathlib, json, hashlib, shutil, subprocess and re modules, plus Git.
The provenance helper also uses tempfile. Bash scripts require Bash, GNU Make,
sha256sum, timeout, Git, and the dependency tools/runtime in the sibling prefix.
No Python package installation is required.

```sh
W=/workspace/scratch/5b5281e79681
REPO="$W/recovery-ui-20261015/source"
F="$W/legacy-layer-operations-20261015/deliverables/migration/fixtures/legacy-layer-operations"
TEST="$W/legacy-layer-operations-20261015/deliverables/migration/tests/test_legacy_layer_operations_fixtures.py"
OUT="$W/layer-operations-replay"
test ! -e "$OUT"
mkdir -p "$OUT/deliverables/migration/tests"
cp -a --reflink=auto "$W/legacy-composite-20261015/gimp" "$OUT/gimp"
for file in capture-layer-operations.c build.sh run.sh independent.sh package.py collect-provenance.py README.md source-notes.md REPRODUCE.md; do
  cp "$F/$file" "$OUT/$file"
done
cp "$TEST" "$OUT/deliverables/migration/tests/"
cp "$OUT/capture-layer-operations.c" "$OUT/gimp/app/tests/test-xcf.c"
# The copied core already has the identical FilterLayer observer; do not apply
# the complete overlay on top of that modified tree.
bash "$OUT/build.sh"
bash "$OUT/run.sh"
bash "$OUT/run.sh" repeat-captures
bash "$OUT/independent.sh"
python3 "$OUT/collect-provenance.py"
python3 "$OUT/package.py"
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s "$OUT/deliverables/migration/tests" \
  -p 'test_legacy_layer_operations_fixtures.py' -v > "$OUT/validation.log" 2>&1
# Include the NEW validation log, and reseal the NEW package.
python3 "$OUT/package.py"
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s "$OUT/deliverables/migration/tests" \
  -p 'test_legacy_layer_operations_fixtures.py' -v
```

`run.sh` creates isolated HOME/GIMP2_DIRECTORY profiles. `independent.sh` skips
an already-recorded exit-code file, so always use a fresh OUT for a new experiment.
Do not run this exporter inside the checked-in fixture folder: its base is a
workspace containing the source/build and measurements. It writes under its own
`deliverables/`, not the maintained repository. The old capture's exporter
metadata records the 2026-10-15 experiment and copied composite build; on another
date or with another starting build, review/update those provenance fields before
publishing new evidence. No cross-machine binary identity is promised.

## C. Reconstruct prerequisites in a new workspace

This costs a full legacy dependency/application build; it is not part of the
fast fixture test. Use Debian 13 amd64 and the maintained repository's exact
signed-snapshot lock and baseline build tools. This recipe creates the sibling
name expected by the scripts. Unlike copying old env.sh elsewhere, generating
these environments at their new absolute paths makes their loader/tool paths
correct. Run with a fresh W, and set REPO to a checkout containing the migration
baseline/tools and this fixture.

```sh
W=/absolute/path/to/new-workspace
REPO=/absolute/path/to/maintained-repository
F="$REPO/migration/fixtures/legacy-layer-operations"
OUT="$W/layer-operations-replay"
mkdir -p "$W/legacy-filter-positive-20261015"
P="$W/legacy-filter-positive-20261015"
git clone https://github.com/seagetch/gimp-painter.git "$P/gimp"
git -C "$P/gimp" checkout --detach afa43fae3e920210146abed514f136fd49f671b5
git clone https://gitlab.gnome.org/GNOME/babl.git "$P/babl"
git -C "$P/babl" checkout --detach e92ced2c54250af1dddb58f76ff1678d74a30dcf
git clone https://gitlab.gnome.org/GNOME/gegl.git "$P/gegl"
git -C "$P/gegl" checkout --detach 29254dcd875d271043aba57d0e6ebc48ec63c013
python3 "$REPO/tools/prepare-linux-build-deps.py" --directory "$P/deps" \
  --lock "$REPO/migration/baseline/legacy/debian13-package-lock.json"
bash "$REPO/tools/build_legacy_reference.sh" \
  "$P/deps" "$P/build" "$P/gimp" "$P/babl" "$P/gegl"
# The installed original reader is now preserved. Never make install from OUT.
test ! -e "$OUT"
mkdir -p "$OUT/deliverables/migration/tests"
cp -a --reflink=auto "$P/gimp" "$OUT/gimp"
for file in capture-layer-operations.c build.sh run.sh independent.sh package.py collect-provenance.py README.md source-notes.md REPRODUCE.md; do
  cp "$F/$file" "$OUT/$file"
done
cp "$REPO/migration/tests/test_legacy_layer_operations_fixtures.py" \
  "$OUT/deliverables/migration/tests/"
# Remove only tracked baseline overlays in the NEW disposable copied tree.
# Generated configuration, archives, and the separately installed original stay.
git -C "$OUT/gimp" diff --name-only -z HEAD | \
  xargs -0 -r git -C "$OUT/gimp" restore --source=HEAD --worktree --
git -C "$OUT/gimp" apply --check "$F/source-overlay.patch"
git -C "$OUT/gimp" apply "$F/source-overlay.patch"
source "$P/build/env.sh"
make -C "$OUT/gimp/app/core" -j4 > "$OUT/core-build.log" 2>&1
bash "$OUT/build.sh"
```

Then use B's run, independent-reader, provenance, export, and validation commands
with this OUT. The complete overlay supplies both the baseline compatibility
changes and capture instrumentation; applying it to an already-patched tree
without the isolated restore would incorrectly apply those changes twice.

The baseline recipe was independently replayed in 2026-10-15; this package's
new-workspace assembly instructions are documented but not an additional clean
full-build execution claim. Actual measured capture reused the composite core
archives; `archive-provenance.json` seals that fact. A fresh rebuild may produce
a different installed-reader hash due to paths/toolchain. The historical test's
`reference_install_untouched` condition deliberately expects the captured reader
hash and may fail for a new build; record the new reader's pre/post hashes and
review that provenance before adapting a new experiment's assertion. Do not
silently call such a failure an unchanged-binary pass.
