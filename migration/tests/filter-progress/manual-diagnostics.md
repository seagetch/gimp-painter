# Actual app result and unresolved GTK diagnostics

The 2026-10-04 07:44:04–07:53:04 UTC session used the final app/helper hashes
recorded in `acceptance.json`, the actual build-tree app, a private profile and a
synthetic 3072×3072 RGB U8 nonlinear image. The active Retinex editor visibly
showed `Retinex: filtering`, a partially filled bar and enabled Cancel Update.
Clicking it produced `Filter execution cancelled` and hid progress controls.
The visible canvas crop `[311,150,870,708]` before rerun and after cancellation
had the same SHA256
`265ce6c436829df0922c9b18d9250d6177ee22b6108d2c4e2a31a92d2b4f9dd9`.
This is a captured visible-region comparison, not a manual full-resolution
buffer/export equality claim. Zoom In changed 18.2% to 25%. Quit discarded only
the generated scratch image, returned exit 0 and released the build lock.

The `manual-app-stderr.log` member of `logs.tar.gz` contains these two diagnostic forms:

- `gdk_window_get_window_type: assertion 'GDK_IS_WINDOW (window)' failed`
- `gtk_accessible_get_widget: assertion 'GTK_IS_ACCESSIBLE (accessible)' failed`

| Raw lines | Printed local times | Correlated UTC operation |
|---|---|---|
| 419 / 421 | 03:46:25.521 / .560 | 07:46:25 New Filter Layer through accessibility menu activation |
| 437 / 439 | 03:47:54.643 / .681 | 07:47:54 First Edit Filter Layer opening |
| 443 / 445 | 03:49:07.244 / .279 | 07:49:07 Reopen editor after baseline screenshot |
| 449 / 451 | 03:50:09.255 / .291 | 07:50:09 Reopen after entry accessibility activation closed the editor |
| 461 / 463 | 03:51:05.047 / .085 | 07:51:05 Reopen active editor after scales change |
| 474 / 476 | 03:52:25.037 / .044 | 07:52:25 Open Quit GIMP prompt with Ctrl+Q |

The six pairs correlate with editor opening and Quit, not the observed Cancel
Update click. This is timing correlation, not a demonstrated cause. No stack
trace was collected and neither the environment nor this progress change is
claimed as the cause. The unresolved gate is assigned to the next narrow
reproduction/stack investigation after this progress checkpoint.

Line 423, `03:47:05.435`, is a separate `Gtk-WARNING: no trigger event for menu
popup` during Retinex combo activation. Other raw startup messages identify
missing icons/resources, `build-installed-filter/etc/templaterc` and
`controllerrc`, splash and plug-in localization directories. They were not
filtered, fixed, or treated as successful diagnostics in this scope.

At 07:59:29 UTC, the same terminal was directly observed with
`TZ=America/Toronto`, local `03:59:29`, offset `-0400`, zone `EDT`. Re-sourcing the
same two launcher environment scripts left the values unchanged. The raw later
observation is in `timezone-evidence.txt`. The terminated app's inheritance of
that timezone is an inference consistent with its four-hour offset; its
historical process environment was not captured.

## Reproduce the synthetic session

`generate-fixture.py` is the exact deterministic image recipe, requiring Pillow.
Copy it into an owned run directory and run there; it writes only the adjacent
`progress-fixture.png`. The fixture SHA256 is recorded in `acceptance.json`.
`launch-observed.sh` preserves the exact launcher used, including workspace
paths, environment, full-session build lock, binary hashes and stdout/stderr
capture. Adjust its checkout/run paths for another workspace.

Create empty `profile`, `cache`, `tmp` and `data` directories in the run directory.
The observed data overlay linked top-level source `data/*`; it additionally
linked `themes` and `menus` from the checkout, and overrode `images`, `patterns`,
`icons`, `brushes` with the same-named `gimp-data/*` directories. Missing-resource
conditions above are part of this observation; do not silently repair them when
trying to reproduce its diagnostics.

Open the fixture; create Retinex with scale 240, scales 3, uniform distribution,
dynamic 1.2. Wait for the initial cache. Edit scales to 8, accept, immediately
reopen the editor, observe progress, cancel the update, close the editor and
zoom. Finally open Quit and discard the generated unsaved image. Accessibility
activation of an editable entry triggered activate instead of focus once; the
successful numeric edit used a screenshot-grounded coordinate click and native
keystrokes. Do not conflate a CUA closed-window observation error with the app's
own critical log.

The complete private profile, fixture and full-window before/after/zoom captures
remain under `/workspace/scratch/5b5281e79681/filter-progress-validation/manual/`.
This public evidence includes the two focused editor captures and complete raw
app logs. The manual flow covered Retinex only; automatic tests covered all four
isolated routes and ordinary native kernels. No global statusbar claim is made.
