# Popup placement recovery, 2026-11-11

This is a new reconstruction on published commit 74819d68427a0d68647b13310e5ed6399b88044b, not a recovery of the unavailable 93bfc08ab0 tree or its four original logs. No old test result is claimed for this patch.

The toolbar popup's `gimp_painter_popup_show_over` flips above an anchor when it does not fit below. A popup may fit the monitor while fitting neither space; the flipped coordinate then lies outside the monitor. Preserve bottom-left preference and natural GTK requisition, but clamp final coordinates in the same way as the common-layer `show_at` path. Oversized windows start at the monitor origin; their contents are not shrunk and full visibility is not claimed.

## New validation results

- Full configured GIMP build: 3715/3715 targets completed successfully. Tail and full-log SHA-256 are retained here; full log is in the recovery archive.
- `tools/test-popup-placement-recovery.sh`: nine native GTK cases at each of GDK_SCALE=1 and 2 (18 passes). Old published code fails the neither-side-fit regression at both scales; a window-size measurement mutant also fails both scales. Includes destroy-on-map, repeated open/Escape and outside cancellation.
- Real `painter-compact-options` application: `/ink-native-popup/actual-blob` and all six `/ink-native-popup` cases pass at both scales. Screenshots inspected. Scale 1 used physical 1080x1920; scale 2 used 2160x3840 (same logical portrait area). The application is not maximized in these captures.
- Known failure retained: scale 2 on physical 1080x1920 fails before opening the popup because toolbar anchor x=-54 is outside the main layout. This patch does not fix oversized editors/toolbars or claim complete 2x usability.
- Initial test shutdown failed because recent-files storage used a read-only home. Temporary XDG data/config/cache directories resolve the harness problem; the original failure log is retained.
- Nonzero monitor origins and tablet hardware were not exercised natively. Oversized contents are pinned, not resized.

## Reproduction

Source the prepared dependency environment and build GIMP with runtime CI tests disabled. Run the standalone script locally. For the real application tests, pass an absolute configured build path:

```sh
SCREEN=1080x1920x24 SCALES=1 bash migration/tests/popup-placement-recovery/run-native.sh /absolute/build
SCREEN=2160x3840x24 SCALES=2 bash migration/tests/popup-placement-recovery/run-native.sh /absolute/build
```

`SCREEN=1080x1920x24 SCALES=2` reproduces the separate narrow-screen pre-open failure. Xvfb uses authenticated TCP because Unix sockets are unavailable in the validation environment; authentication files are temporary and are not published.

No change to ordinary build-only CI; these local diagnostic tests are not added to its workflow.
