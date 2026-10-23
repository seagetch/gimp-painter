# Native GUI test registration fixtures

The native Meson registrations own fixture settings that individual tests need.
Callers still provide a working authenticated display and a fresh writable
HOME/XDG/TMP profile, following `native-build-workflow.md`. Never use an existing
user profile. Create the ordinary `gimp/3.0` cache and temporary subdirectories
before tests perform their capacity checks.

## Registration changes

- `painter-layer-ui`: a 180-second timeout for all 63 cases. The original
  60-second run stopped after 53 cases; a complete authenticated Xvfb run took
  83.69 seconds. The registration now owns the previously verified 180-second
  budget. Assertions and cases remain unchanged. Other registrations retain
  their existing timeouts, including the 600-second filter-layer budget.
- `painter-save-continuation`: `GSETTINGS_BACKEND=memory`, consistent with the
  existing isolated native-test harnesses. Save/dialog continuation tests do
  not require a persistent dconf service or a session bus.
- `painter-profile-native`: `GIMP3_DATADIR` points to the build tree's `data`
  directory, and `depends: [gimp_tags_default]` schedules the real generated
  default tags before the test. The first-run installer copies these tags.
  No empty or fabricated data replaces the generated resources.

`data/tags/meson.build` defines the tags target before `app/tests/meson.build`
is evaluated. This explicit test dependency also covers selecting Profile
directly without first compiling every default target.

## Validation

The 2026-10-11 validation used the existing standard native build with
`headless-tests=disabled`, `painter-http=disabled`, and no sanitizer. After the
source change, ordinary `meson setup --reconfigure`, `meson compile`, and one
`meson test` selection were used. No generated build metadata or object files
were patched. Meson introspection verified all three settings and the tags
dependency.

Before invoking Meson, the runner explicitly removed inherited
`GSETTINGS_BACKEND` and `GIMP3_DATADIR`. Neither a per-target override nor a
timeout multiplier was supplied. With the general isolated profile and display
already prepared, the selection was:

```sh
env -u GSETTINGS_BACKEND -u GIMP3_DATADIR meson test -C "$build" \
  --num-processes 1 --print-errorlogs --logbase gui-registered \
  painter-fill-brush-ui painter-layer-ui painter-canvas-ui \
  painter-compact-options painter-navigation-ui painter-navigation-events \
  device-dialog-events painter-perspective-ui painter-perspective-events \
  layer-presets-ui painter-mypaint-tool painter-mypaint-editor \
  painter-smudge-ui painter-save-continuation painter-profile-native \
  painter-paper-ui
```

The command retains Meson's normal prerequisite build step. All 16 registrations
passed in that single selection: 252/252 TAP cases, zero failures, skips, or
timeouts. Layer completed in 80.38 seconds within its registered 180-second
budget. Case counts, timeouts, source hash, and historical failed attempts are
recorded in `../tests/native-gui-fixtures-20261011.json`.

The executor's supported display transport was the unmodified Xvfb binary with
an inherited loopback-only listener at fd 3 (`LISTEN_PID`/`LISTEN_FDS`). All
automatic transports were disabled. The run verified authenticated success,
rejection with an empty authority file, one listener on 127.0.0.1 only, and
server/temporary-authentication cleanup. See `native-build-workflow.md` for the
transport contract. This transport is a validation-environment fixture, not a
new application or Meson dependency.

## Scope and retained history

The earlier runs remain historical evidence in
`../tests/native-loopback-gui-20261011.json`: the original profile run had 1 OK
and 15 FAIL; writable-profile isolation produced 13 OK, 2 FAIL, and 1 TIMEOUT;
focused fixture-adjusted retries passed. They are not relabeled as an original
unmodified all-16 pass.

This work validates registered GUI behavior against build-generated data.
It does not establish installed-prefix startup, persistent dconf integration,
sanitizer acceptance, or complete migration acceptance. The separately reported
brush-transform signed-overflow issue remains open and is outside these fixture
changes.
