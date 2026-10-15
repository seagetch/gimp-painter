# Original rotation operations (02.012 evidence)

Pinned source: afa43fae3e920210146abed514f136fd49f671b5. Captured 2026-10-15.

## Result and limits

The unchanged original GTK canvas event handler ran 238 synthetic event/setup
states and 132 original statusbar button/setup states (370 total), repeated in
a fresh profile and fresh authenticated Xvfb server. All 370 state rows match
byte-for-byte across runs. A complementary extracted-source/Cairo capture has
1,232 states; independently compiled -O0 and -O2 runs match byte-for-byte.
Sixteen offline evidence checks pass. None of these results establish GIMP 3
parity or physical mouse/tablet behavior.

Both full-GUI runs write the complete capture and then **segfault during original
GUI teardown** after the harness calls gimp_exit(gimp, TRUE). The exact crashing
instruction has not been diagnosed. The runner records command_exit=-11.
This is NOT a successful application exit and is not hidden or converted to a
zero exit. The offline test checks this known observation, not acceptance of
normal shutdown. GUI startup also emits existing GTK/GObject warnings and
criticals; complete raw logs are included. This package supports bounded
rotation callback/event/coordinate evidence, not a clean original UI lifecycle.

## Measured behaviors

- Shift+middle begins rotation; Shift+Ctrl+Alt+middle also begins rotation, not
  zoom. Pointer grab is observed through the real GimpDisplayShell fields.
- Ctrl key press alone leaves the current angle unchanged. A subsequent motion
  with Ctrl applies snapping, including stationary synthetic motion. Shift
  release does not end the drag. Ctrl release alone leaves the snapped angle;
  the next motion restores the unsnapped angle from the unchanged start anchor.
  Middle release clears rotating, anchor and grab.
- Drag snapping is 15 degrees, normalized before half-up integer rounding:
  +7.5 -> 15; -7.5 becomes 352.5 and snaps to 0. Samples immediately above and
  below both boundaries are retained. This is distinct from toolbar stepping.
- Actual statusbar buttons step by 30 degrees using C round() and signed fmod.
  Unflipped initial +15 gives Left=-30, Right=60; initial -15 gives Left=-60,
  Right=30. Horizontal mirroring reverses button direction. Negative stored
  angles are valid observations and were not normalized by the exporter.
- The original flip toggle changes mirrored while retaining the angle. Reset
  clears angle/mirror and calls the original scale API with factor 1. The source
  capture records this request; the GUI package makes no independent scale-state
  assertion because its table does not include the scale fields.
- A bottom-center to right/top/left drag produces stored 270/180/90 degrees
  unflipped and 90/180/270 mirrored. Fractional event coordinates are passed to
  the original gint parameters, retaining the old truncation behavior.
- Actual display coordinate helpers are called after every GUI event: device
  probe (integer viewport center + (40,20)), inverse-rotation/mirror coordinates,
  and forward round trip. These helpers operate on display-space coordinates,
  not zoom/offset-adjusted image pixels. All recorded round trips agree within
  1e-10. The actual GUI viewport is 1015x674. The complementary source/Cairo
  capture covers both 100x112 and 101x113 integer-midpoint behavior.

## Exactly what was executed

capture-rotation.c replaces only app/tests/test-ui.c in an isolated copy of the
previous original build. It initializes the original GUI test lifecycle, creates
a transparent 640x480 image using the original test utility, realizes its canvas,
and supplies explicitly synthetic GdkEvent structures directly to the original
public gimp_display_shell_canvas_tool_events callback. Motion/button events use
the actual default GDK core-pointer device. This does not pass through physical
hardware, XTest or the top-level GTK queue. Key state masks are explicitly supplied.

The test finds the existing statusbar buttons by their stock-image IDs and uses
gtk_button_clicked / gtk_toggle_button_set_active. Their original signal handlers
run unchanged. Setup deliberately assigns initial angle/mirror values, including
negative and half-step probes; those setup assignments are labeled separately.
canvas.png is a final realized-canvas observation, not visual rotation equivalence
proof or real artwork. Its checkerboard is the deliberate transparent test image.

All implementation files for rotation, event dispatch, statusbar operations and
coordinate transforms are unchanged from the pin. source-overlay.patch contains
the complete inherited compiler/GTK compatibility changes and previous filter
capture hook, plus the test-ui entry point; do not apply its constituents twice.
The display archive, core archive, widget archive and GUI test utility archive
are byte-identical to the previous isolated original build. The installed
original app remains untouched, SHA256
85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1.

The complementary source capture verifies complete original files against Git,
extracts eight unchanged function bodies, supplies small synthetic structs and
GUI side-effect counters, and uses real Cairo image-surface matrices. It is
explicitly NOT GUI execution. Its source-driver supplies modifier states only;
GUI key press/release timing claims come exclusively from the real GUI capture.

## Reproduction

See RECONSTRUCTION.md for the complete packaged build dependency inventory and
source/baseline-overlay ordering. Use the original dependency/configuration baseline and replay-20261015 recipe.
The source/build was copied from legacy-filter-positive-20261015/gimp. Build
scripts source that sibling's build/env.sh; paths need relocation on another
machine. A fresh build first builds original libraries using the baseline recipe.

1. Apply source-overlay.patch to the clean pin (git apply --check passed).
2. bash legacy-rotation-operations-20261015/build.sh
3. bash legacy-rotation-operations-20261015/run_gui_fresh.sh replay-gui-primary
4. bash legacy-rotation-operations-20261015/run_gui_fresh.sh replay-gui-repeat
5. bash legacy-rotation-operations-20261015/replay_source.sh
6. python3 legacy-rotation-operations-20261015/test_rotation_capture.py
7. python3 legacy-rotation-operations-20261015/collect-provenance.py
8. python3 legacy-rotation-operations-20261015/package.py

The GUI scripts deliberately return nonzero because of the recorded teardown
crash. Run the later checks separately, not behind && on the GUI result.
run_authenticated_display.py is the unchanged existing supported runner. It
creates one loopback-only listener with a temporary xauth cookie, verifies
unauthenticated clients fail, and removes the server, temporary HOME/XDG directories and authorization on exit.
The separately configured GIMP2_DIRECTORY at output/profile persists. The actual
run_gui.sh is sealed historical provenance and can reuse that profile; do not use
it to claim a fresh-profile rerun. The proposed run_gui_fresh.sh requires a new
output directory, atomically refuses any existing path, and never deletes prior
evidence. It has guard tests but was not used for the original captures. No
security settings, ptrace, preload, injection or interposition were used.
An initial ordinary Unix-socket xvfb-run failed to open the display. A supported
escalated execution attempt failed in sandbox setup before running; no denied
path was read and no retry or permissions change followed. The existing
loopback runner then succeeded under ordinary sandbox execution.

## Remaining acceptance boundaries

Physical input, tablet devices, OS-specific input, event compression under a live
queue, focus/device interruption, transformed pan/zoom integration, clean old GUI
shutdown and modern parity remain unproven. Vertical flip is not an old feature.
No task ledger or acceptance contract is changed by this exporter. The parent
must independently review the bounded evidence and shutdown caveat before
marking task 02.012 complete.
