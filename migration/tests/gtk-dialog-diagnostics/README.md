# GTK dialog diagnostic classification

Task `30.001/native-dialog-diagnostics` establishes the cause of the two
assertions preserved by `../filter-progress/manual-diagnostics.md`. This is a
diagnosis and native-input acceptance checkpoint, **not a GTK dependency fix**.
The AT-SPI defect remains open as `34.003/gtk-atk-menu-guards`.

## Result and cause

The unchanged GIMP binary reproduced both assertions when an **unmapped menu
item** was activated through its AT-SPI action. A standalone GTK/ATK program
containing no GIMP code reproduced the same pair, at the same library offsets.
Direct GTK menu activation opened the same control dialog without adding
either assertion. Actual native pointer/keyboard input in GIMP likewise
completed the Retinex create/edit/update-cancel/Quit flow with zero occurrences
of these two assertions. Other device diagnostics remained; see below.

1. `gdk_window_get_window_type: GDK_IS_WINDOW (window)` comes from Debian's
   `016_no_offscreen_widgets_grabbing.patch` in `gtk/gtkmain.c:gtk_grab_add`.
   It tests the toplevel widget, then passes its possibly NULL GdkWindow to
   `gdk_window_get_window_type`. Selecting an unmapped menu through GTK's
   accessibility action reaches that grab before invoking the application
   action. The standalone probe directly records `GtkWindow has_window=0`.
2. `gtk_accessible_get_widget: GTK_IS_ACCESSIBLE (accessible)` comes from
   `gtk/a11y/gtkmenuitemaccessible.c:ensure_menus_unposted`. After activation of
   an originally unmapped item, its ancestor loop converts every AtkObject
   parent to GtkAccessible. The probe records an ancestor of type
   `GtkToplevelAccessible`, which derives directly from AtkObject and fails
   that test. This is a valid-object type mismatch; the evidence does not
   indicate a dangling dialog or Painter BindingStore failure.

The app's bounded stacks are:

```
AT-SPI / D-Bus / libatk-bridge
  gtk_menu_item_accessible_do_action [GTK +0x403d81]
  gtk_menu_shell_real_select_item   [GTK +0x21cf9c]
  gtk_grab_add+0x2e
  gdk_window_get_window_type

AT-SPI / D-Bus / libatk-bridge
  ensure_menus_unposted, inlined in do_action [GTK +0x403dc8]
  gtk_accessible_get_widget
```

No dialog constructor/destructor or Painter C++ owner frame lies between the
AT-SPI bridge and either assertion. The six common GIMP dialog/helper files
listed in `acceptance.json` are unchanged from the earliest locally available
snapshot `3e30c05` to the tested commit. The actual imported GIMP upstream base
is `95f6410f25c5186686db7a489d79c1e79187cd41`; that object is absent from this
shallow checkout, so this is not an exact import-to-HEAD comparison.

## Dependency provenance and remediation

The GTK runtime is locked Debian `3.24.49-3`. Its library byte-matches the
official archived `.deb`; it was not patched by this repository. Package,
library, source and application hashes are recorded in `acceptance.json`.

Primary source references:

- [GTK 3.24.49 menu accessibility source](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/a11y/gtkmenuitemaccessible.c)
- [GTK 3.24.52 same source](https://raw.githubusercontent.com/GNOME/gtk/3.24.52/gtk/a11y/gtkmenuitemaccessible.c)
- [GTK 3.24.49 application accessible type](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/a11y/gtktoplevelaccessible.c)
- [Debian 3.24.52-1 offscreen-grab patch](https://sources.debian.org/patches/gtk%2B3.0/3.24.52-1/016_no_offscreen_widgets_grabbing.patch/)
- [Historical GNOME bug 771242](https://bugzilla.gnome.org/show_bug.cgi?id=771242#c44)

The menu-accessibility file is byte-identical in upstream tags 3.24.49,
3.24.52 and the observed `gtk-3-24` branch. Debian 3.24.52-1 retains the unsafe
offscreen check. The historical ComboBox fix reordered popup/selection in
`gtkcombobox.c`; it does not repair this ATK path. A generic upgrade is therefore
not a demonstrated remedy.

The archive's later `upstream-status.txt` supplies pinned commits and source
hashes and supersedes the earlier analysis note's unavailable-tag limitation.
The exact Debian 3.24.49-3 source patch was not fetched; its extracted library
was directly disassembled and matched to the published later Debian patch.

The narrow candidate fix belongs in the dependency: check for a GdkWindow
before inspecting its type while preserving offscreen-grab behavior, and
check `GTK_IS_ACCESSIBLE` before the ancestor-to-widget conversion while
preserving ancestor traversal. It needs reproducible source/patch hashes,
both fixed-mode and ordinary-input tests, and integration into the packaged
runtime. That work remains open. Disabling accessibility, hiding diagnostics,
changing Painter lifetime rules, or treating assistive-technology users as
out of scope would not fix it.

This project currently extracts pinned Debian binary packages with
`tools/prepare-linux-build-deps.py`; it has no GTK source-build/patch stage.
Applying the candidate requires adding that bounded dependency build and
retaining its source/license and runtime manifest provenance. No system or
project GTK library was replaced in this checkpoint.

## Run the independent reproducer

Source the selected build dependency environment, then run on a real GTK
display. The script builds only its own small program into the supplied output
directory. It preserves complete stdout/stderr and binary/source hashes.

```
source ../gimp-build-restoration/env.sh
source tools/linux-debian13-env.sh
bash migration/tests/gtk-dialog-diagnostics/run-repro.sh \
  /tmp/gtk-known-defect --expect-known-defect
bash migration/tests/gtk-dialog-diagnostics/run-repro.sh \
  /tmp/gtk-fixed-check --check-fixed
```

On the unchanged locked GTK runtime, the first mode exits 0 only when the
hidden ATK action produces exactly one of each target assertion, opens its
dialog, and the direct GTK control opens another dialog with no added
Gtk/Gdk criticals, including teardown. Other GLib domains remain in raw stderr
but are not counted by this focused predicate. The second mode correctly exits
**1**, because the dependency is
not fixed. On a corrected dependency it must exit 0. An unavailable display
returns 77; this is not a pass. Both modes forward every diagnostic unchanged
to GLib's default handler. The direct GTK control is an API control, not a
claim of physical input; the separate actual-app session supplies that check.

The temporary `critical-stack.c` interposer is preserved only in the evidence
archive. It intercepts the two GLib assertion-report sites, writes at most 32
symbol frames, then forwards the original diagnostic. It captures no locals,
core, memory or environment dump. No debugger was installed on the native
host and no ptrace operation was attempted. The final app session and final
reproducer were run without this instrumentation.

## Actual application acceptance

The three sessions reused the exact committed application/helper binaries and
copied the prior private profile/data overlay and deterministic 3072x3072 RGB
nonlinear fixture. Missing-resource/localization conditions were preserved.
The observed UTC operation journal and launchers are in `logs.tar.gz`; the
raw app times are four hours behind UTC, consistent with the same terminal's
previously verified America/Toronto EDT setting; no process environment was
dumped. Journal operation times are observer timestamps to the second, not process tracing.
Launch/exit times come directly from each launcher.

- Baseline, 08:22:01–08:24:22 UTC: accessibility New Filter and Quit each
  produced the target pair; normal exit 0
- Instrumented, 08:25:09–08:30:27 UTC: same pairs and exact stacks for both
  actions; native visible-menu Edit and global Ctrl+Q added no target pair;
  normal exit 0
- Native, 08:33:00–08:42:02 UTC: visible-menu New Filter, Retinex default
  240/3/uniform/1.2, completed view, repeated Edit, 3-to-8 scales update,
  partial progress, a second 240-to-256 scale update, successful Cancel Update
  with `Filter execution cancelled`, editor close, Ctrl+Q prompt, verified
  Cancel Quit, save to a new private scratch XCF, and Ctrl+Q normal exit 0

The first cancellation attempt happened after its update completed, so it is
not counted as a cancellation. The second was visibly successful. A proposed
coordinate Cancel Quit was not executed; the live dialog's separate Cancel
button was then verified and activated. The
final synthetic image was saved to a new path before exit. No final discard
acceptance is claimed. One Cancel Quit used its verified accessible **button**;
all native-session menu actions used visible menus and ordinary pointer/keys.

This checks dialog/function behavior, not full-resolution pixel equality or
all input devices/platforms. No screenshot files are published for this
checkpoint; the states above were directly observed through the native tool.
No application or dependency lifetime code changed, so no new sanitizer
coverage is claimed. The previous focused lifetime results remain separate.

## Other diagnostics remain open

All three app sessions additionally produced the same 16-critical device
cluster on the first New Filter opening. In the native session it is at raw
04:33:51.915 (08:33:51.915 UTC), lines 92–121: two
`gimp_device_info_get_device: GIMP_IS_DEVICE_INFO (info)` assertions and 14
GDK device/type/source/state/position/axis assertions. It occurs with native
pointer input too. No stack was captured for those assertions and no cause,
environment blame, or product-fix claim is made. It is tracked separately as
`34.003/dialog-device-diagnostics`.

Startup missing resources/icons/localization and other full raw diagnostics
are preserved. Zero target assertions does not mean a clean whole-app log.
The older editor task's lost logs cannot be retroactively classified; the
earlier progress session had no stack, so only its matching assertion forms
are linked to this reproducible mechanism, not every historical occurrence.
