# Phase D: schema editors and ordinary XCF

`acceptance.json` records the exact scope, source/binary identities and remaining
gates. `raw-evidence.tar.gz` is a deterministic, allowlisted evidence bundle;
its `manifest.json` hashes every other member. It excludes binaries, profiles,
environment dumps and redundant full-source archives.

The native suite contains ten new groups in
`app/tests/test-filter-schema-persistence{,-failures}.inc`, included by the real
`painter-layer-ui` test. Ordinary successful Save/Open uses registered application
file entry points. Generated modern scenes are not historical fixtures.

The complete ten-group normal and focused ASan/UBSan runs passed. A subsequent
test-only review added actual external-origin ID assertions and a full-buffer
comparison of the newly completed cache after active Save/Open. Both changed
groups passed again in both modes. `reviewed-test-only-delta.patch` identifies
that exact difference; unchanged groups were not needlessly rerun. No production
source changed between these runs. Six existing XCF regressions passed on the
same final production sources. The genuine historical Edge input is included in
those existing tests, and all 2626 tracked fixture blobs retain their base IDs.

## Replay

Use the repository's documented Debian 13 environment and hold
`/workspace/shared/gimp-painter-build.lock` for shared builds/runs. Build
`app/tests/painter-layer-ui` and `app/tests/painter-xcf-roundtrip`, then build the
focused instrumented executable with:

```
python3 -B migration/tests/build_painter_ui_sanitizers.py build-installed-filter \
  --persistence --report /tmp/painter-persistence-build.json
```

The archived `run-native.py` loads the relevant Meson test environment without
logging it, runs in `dbus-run-session`, selects exact case paths and rejects
skips, missing cases, changed source hashes and changed executables. `run-final.sh`
records the ten-group runs; `run-reviewed.sh` selects the final two strengthened
groups. Run on an available native GTK display: setting DISPLAY alone in an
unconnected execution namespace can return skip 77 and is not a pass.
The recorded absolute checkout/build/output roots must be adapted when restoring
elsewhere. `run-regressions.py` lists the six unchanged XCF cases explicitly.

`compile-minimum.py` compiles the two changed production units using a separately
pinned actual GLib 2.70.0 compile database and checks every observed GLib header
path/hash. It writes outside that frozen minimum tree. This is compile-only;
no new full minimum-environment build or runtime result is claimed.

## Failed attempts and limits

- `before-fix-native.log` reproduces the real ordinary Save refusal for an
  imported live reference whose recorded type/ID differs from its actual target
- `matrix2-native.log` records a test setup mistake: the generated unknown field
  was initially put in the loader's temporary restoration dictionary instead of
  its authoritative input capsule. The fixture setup was corrected, with no
  additional production change; subsequent runs pass
- Preliminary source lists omitted part of the changed writer/header/test set.
  `reference-production-identities.json` discloses this; final ten-group and
  reviewed two-group receipts capture the complete relevant set before/after runs
- Compiler C90/declaration/unused/maybe-uninitialized warnings, plug-in locale and
  test-profile data-folder diagnostics remain in raw logs. No warning-free claim

The focused sanitizer build has 29 instrumented units and 45 additional RTTI-only
units; remaining GIMP and dependencies are uninstrumented, and leak detection is
disabled. New large cases cross the 1 MiB multipart boundary. Prior >256/512 MiB
resource evidence is reused without rerunning or treating it as new results.
Final-close faults explicitly abandon the replacement stream and do not simulate
an operating-system rename failure. Phase E full-app/relocation/OOM and broader
platform, AT-SPI, image-history atomicity and migration gates remain open.
