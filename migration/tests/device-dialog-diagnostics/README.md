# Device readiness during first dialog use

Task `34.003/dialog-device-diagnostics` fixes the 16-critical cluster first
recorded by `../gtk-dialog-diagnostics`. It is independent of the two still-open
GTK window/accessibility defects tracked by `34.003/gtk-atk-menu-guards`.

## Cause and narrow correction

On fresh no-splash startup, GIMP deliberately postpones device discovery until
its first focus-in notification. GTK can deliver canvas focus loss when a
native dialog opens before that initialization. The canvas handler continued
into coordinate/state sampling with the manager's NULL current device. One
such event emitted twelve coordinate assertions followed by four state
assertions. Continuing also exposed the caller to invalid position data after
GDK rejected the missing device.

The unchanged baseline binary reproduced that cluster through ordinary visible
Layer / New Filter pointer input. A temporary assertion-only interposer captured
both first-failure stacks, with a maximum of 32 symbol frames each:

```
normal main loop -> gtk_main_do_event -> gtk_widget_send_focus_change
  -> gimp_display_shell_canvas_tool_events
  -> gimp_display_shell_get_event_coords
  -> gimp_device_info_get_event_coords / gimp_device_info_get_event_state
  -> gimp_device_info_get_device_coords / gimp_device_info_get_device_state
  -> gimp_device_info_get_device (assertion)
```

There is no initialization, device restore, Painter dialog owner, or BindingStore
frame in either stack. Focus-out is inferred from this stack and the source
ordering; the event's `in` value was not dumped. The direct regression supplies
an explicit focus-out while proving that the real manager is uninitialized.
This is a missing current device during startup, not evidence that the XTEST
pointer is stale or that disabling a mouse is wrong. The subsequent
`set device 'Virtual core XTEST pointer' to mode: disabled` line is expected
initialization and remains present after the fix.

The nine-line change in `gimp_display_shell_canvas_tool_events` checks for a
current device after normal shell event/focus handling and device selection,
before any sampling, transform, or tool dispatch. It leaves the event unhandled
when no device is ready. The first focus-in still initializes the manager in
its original order. `gimp_has_focused_once` alone cannot establish readiness:
its flag is set before initialization signal handlers finish.

No default sample is manufactured, no diagnostics are suppressed, no ignored
slave is enabled, and no new ownership state is introduced. Ready-device
coordinates, modifier fallback, pressure/axis curves, disabled slave-to-master
routing, settings restore, and the existing sampler contracts are unchanged.
The fix uses existing typed GObject interfaces and does not create a separate
C++ owner or side store.

## Upstream and GDK contracts

The observed GNOME `gimp-3-0` branch resolves to the migration's original pin,
`95f6410f25c5186686db7a489d79c1e79187cd41`. The local device manager, devices and
coordinate helper files are byte-identical to that source. Lazy initialization
was introduced by [fc49a7a73d05](https://github.com/GNOME/gimp/commit/fc49a7a73d05a1935a15152788874b773dc293c8)
to preserve Wayland tablet-pad discovery after first surface focus. This
intentional deferral is retained.

Primary contracts:

- [Manager discovery and current-device assignment](https://github.com/GNOME/gimp/blob/95f6410f25c5186686db7a489d79c1e79187cd41/app/widgets/gimpdevicemanager.c#L366)
- [Settings restore after first focus](https://github.com/GNOME/gimp/blob/95f6410f25c5186686db7a489d79c1e79187cd41/app/widgets/gimpdevices.c#L349)
- [Coordinate and pressure mapping](https://github.com/GNOME/gimp/blob/95f6410f25c5186686db7a489d79c1e79187cd41/app/widgets/gimpdeviceinfo-coords.c#L35)
- [GTK device mode assignment](https://github.com/GNOME/gtk/blob/2776f156e2405727faea5d7ffa5bd035dc3945c0/gdk/gdkdevice.c#L912)

GTK's `set_mode` updates the mode and emits its property notification; it does
not pump events. The captured stack contains no such call. Runtime GTK remains
the locked Debian 3.24.49-3 build, unchanged by this task. Source copies and
hashes in the archive distinguish the observed GTK branch contract comparison
from the actual runtime dependency.

## Regression and native acceptance

`app/tests/test-device-dialog-events.c` uses the real GIMP GUI/device manager.
During setup it holds canvas callbacks until the initial draw, then restores
the production connections before every regression input. It explicitly asserts
that the canvas has been drawn and ordinary focus events are admitted, preventing
the earlier pre-draw guard from masking this defect and avoiding dependence on
window-manager focus timing.
It checks:

- No current device before first focus; explicit GTK canvas focus-out
- A focus-out reentry from the `focused-once` emission hook, when the focus flag
  is already true but the manager still has no current device
- Normal first focus-in, pointer identity and device establishment
- Exact mouse coordinates, modifiers and normal no-pressure-axis default
- Disabled slave mouse routing through the master (one available in this run)
- Repeated focus loss/restoration and harmless Filter Cancel/close/reopen

A private negative binary links the same new test against only the baseline
shell-event translation unit. It fails at the exact first GimpDeviceInfo
critical (SIGTRAP, subprocess -5). This is a controlled source substitution,
not a claim that the entire negative binary is the old application.
The normal and focused sanitizer versions both pass. Final receipts are under
`regression-final/`; the earlier passing fixture receipts remain separately
under `regression/`. Existing navigation tests
also pass in both modes: rotation/modifier transitions, arbitrary buttons,
radial zoom, and picker grab cleanup. The eight explicitly listed C units are
instrumented with ASan, UBSan and float-cast-overflow checks; other GIMP/C++ and
dependencies are ordinary, and LeakSanitizer is disabled.

The final uninstrumented native application session uses the same synthetic
3072x3072 RGB fixture and the same source of copied profile/data as the baseline.
It opens New Filter via the visible Layer menu, cancels with Escape, reopens,
cancels with the native pointer button, and exits normally through visible
File / Quit. The unchanged one-layer image needed no discard or new save.
Baseline: 16 actual critical messages. Fixed candidate: zero critical messages
through exit 0. Startup resource/icon/localization notices remain; this is not
a full-log-clean or full-migration claim. Raw logs and the UTC operation journal
are preserved. Journal times are observation times; the baseline GLib timestamps
are EDT (UTC minus four hours), consistent with prior recorded native time-zone
evidence. Two baseline Ctrl+Q attempts produced no visible action; successful
Quit used the visible menu instead.

The first harness attempt failed its own hook-count check because it had not
waited for the shell's initial draw. Its raw log is retained separately, as is
the shell-only invocation whose observed exit was display-unavailable 77
(the original raw log contains only the TAP header; a separate observation
record identifies that limitation). Neither is
counted as a successful regression. The corrected native regression and its
baseline-negative control establish actual reachability of the corrected path.

## Reproduce

After configuring the selected build, source its dependencies and run:

```
source ../gimp-build-restoration/env.sh
source tools/linux-debian13-env.sh
python3 migration/tests/device-dialog-diagnostics/run-regression.py \
  --build-phase --output /tmp/device-dialog-check
```

On the native GTK display, with the same environment:

```
python3 migration/tests/device-dialog-diagnostics/run-regression.py \
  --run-phase --output /tmp/device-dialog-check
```

Use a directory visible to both execution surfaces when build and display run
in separate namespaces. Both phases serialize against
`/workspace/shared/gimp-painter-build.lock`; private objects/archives never
replace the normal build. The baseline source is pinned and can be selected
with `--baseline`. The runner rejects unavailable displays and unexpected
negative-test outcomes. `regression.json` records commands, source/binary hashes,
scope, exact exits, and times. Starting a reused output clears any prior passed
status before validation; a wrong-digest probe confirms that a rejected run
stays incomplete and never starts the test binary. `logs.tar.gz` is deterministic, with each member
verified by `log-integrity.json`.

Remaining work includes the independent GTK AT-SPI dependency patch, real tablet
pressure/tilt, hot-unplug, Wayland and other-platform testing, and the original
aggregate/release gates. A detached non-NULL device-info object is not covered
by this startup guard. No general tablet or hot-plug fix is claimed.
