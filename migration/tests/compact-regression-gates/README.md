# Compact UI regression gates

## Scope

Three test-fixture/oracle corrections, with **no production changes**. Apply after
the default-oncanvas bundle (SHA256
`f2703350319822db3e45e6e076e5954b904e4798cee864aac62eb970979d45eb`).
The existing DND, native-session, and Perspective damage bundles were not edited.
No GitHub writes or commits were made in this isolated task.

1. **Rectangle native unit popups:** replace a fixed-delay assumption with a
   bounded two-second wait for the *same exact parent GTK grab*. Keep the
   existing popup state, label format, modal/visible parent, native menu
   identity, menu width, numeric-value, repeated-opening, dismissal, and lifetime
   assertions. No grab, focus, widget, or model is forced by this wait.
2. **MyPaint dock/canvas transfer:** establish the real image shell before
   selecting MyPaint and snapshotting the native dock parent. First-focus device
   initialization normally restores the device's selected tool, and creation
   of the initial shell normally attaches the native dock UI. Keep every prior
   transfer/preference assertion and add exact active-tool identity.
3. **Perspective toolbar:** task `08.018/perspective-options-registration`
   deliberately restored the empty `GimpPerspectiveGuideOptions` subtype. Assert
   its exact registration metadata, exact instance type, and exact base parent,
   instead of incorrectly requiring a base-class instance. All empty-content,
   orientation, spacing, nonhomogeneity, independent-instance, native-parent,
   unsupported-tool, and destruction assertions remain in force. The optional
   screenshot hook only observes mapped widgets and the real root framebuffer.

## Causal evidence

`results.json` contains allowlisted statuses, test names, and assertion failures,
not raw Meson environment dumps. `source-seal.json` records source/binary hashes.
All runtime cases use the extracted official dependency environment, GTK
3.24.49, GEGL 0.4.62, and authenticated loopback-only **1080×1920×24 Xvfb**.
Final production oncanvas default is TRUE; explicit FALSE persistence remains
covered. FALSE controls rebuild only the isolated boolean default and restore
it before the final seal.

### Popup timing

The unchanged fixed100ms check fails with TRUE at RectSelect/size-unit after
Escape; a second instrumented TRUE run fails at Crop/fixed-unit after Return.
With the original FALSE default it passes once, then fails on repeat at
EllipseSelect/position-unit after Return. Thus the row/default difference is
not evidence of a default-exclusive production regression. Every attempt is
recorded, including the passing original FALSE run.

In the diagnostic wait run, the *same* expected popover grab returns naturally
33,266µs and 36,468µs after the initial fixed-delay check. Neither call repairs
focus or grab ownership. The bounded final oracle passes both TRUE and FALSE,
including all six tool instances, four unit rows, and native popup lifecycle
cases. Injecting removal of an already-restored parent grab still fails the
unchanged `grab == popover` assertion after the bound.

This is consistent with GTK's native menu unmapping and asynchronous host-focus
restoration: modal popovers release their GTK grab on host focus-out and regain
it on focus-in. That source explanation is an inference, not a claim that the
diagnostic directly traced every server focus event. No animation-delay theory
is asserted; the test disables GTK animations.

Primary source references: [GTK popover focus callbacks](https://github.com/GNOME/gtk/blob/3.24.49/gtk/gtkpopover.c),
[GTK native menu popdown](https://github.com/GNOME/gtk/blob/3.24.49/gtk/gtkmenu.c).

### Transfer initialization

The original focused test logs `focused=0, MyPaint` before shell creation and
`focused=1, paintbrush` afterward, while the native MyPaint editor still exists.
Initializing focus alone exposes another stale assumption: the native parent
changes during initial image/dock setup. Completing shell setup first fixes
both, without changing production device or docking behavior. TRUE and FALSE
focused transfer cases pass. Restoring the old order while retaining the new
identity assertion fails at active-tool identity, before a missing-menu error.

### Perspective subtype

The full TRUE group after the first two corrections passes 34 cases and fails at the obsolete exact
base-type assertion. The same focused assertion fails with original FALSE.
The other 27 cases were then run individually: 26 pass and the second
Perspective case fails at that same assertion. Those runs are diagnostic
coverage, **not** a claim that the original aggregate passed.

With the exact registered-subtype oracle both Perspective cases pass for TRUE
and FALSE. A metadata substitution is rejected by the new exact registration
assertion; adding an unexpected label to a horizontal view is rejected by the
unchanged empty-content assertion. Neither fault injection is in final source.

## Images

- `true-popup-red.png`: genuine native failure state, visible popover with lost
  grab; framebuffer pixels alone cannot prove grab ownership, so see assertions.
- `true-popup-final.png`: real native popup in the final passing focused run,
  captured by an external read-only GDK root observer. No resize/layout changes.
  This run has the first two corrections; its popup implementation is identical
  to the final three-correction aggregate. `images.json` records exact producing
  binary hashes for every image.
- `true-perspective-canvas-final.png`: real mapped canvas toolbar/native dock
  state in the passing Perspective switching case. The legacy generic session
  intentionally contains floating docks; this is not a new default-layout
  acceptance screenshot or a replacement for the dedicated native-session test.

## Reproduction

Configure the repository's pinned dependency environment and an isolated native
build/profile, with `SOURCE`, `BUILD`, `OUT`, `GIMP_TESTING_ABS_TOP_SRCDIR`,
`GIMP_TESTING_ABS_TOP_BUILDDIR`, and plugin paths set as for GUI tests:

```sh
ninja -C "$BUILD" app/tests/painter-compact-options menus/image-menu.ui
python3 "$SOURCE/tools/run-painter-xvfb.py" \
  "$BUILD/app/tests/painter-compact-options"
python3 "$SOURCE/tools/run-painter-xvfb.py" \
  "$BUILD/app/tests/painter-compact-options" \
  -p /painter-compact-options/canvas-dock-tool-transfer-preference
GIMP_TEST_PERSPECTIVE_TOOLBAR_SCREENSHOT="$OUT/perspective.png" \
  python3 "$SOURCE/tools/run-painter-xvfb.py" \
  "$BUILD/app/tests/painter-compact-options" \
  -p /painter-compact-options/perspective-guide-toolbar-canvas-switching
```

Negative-control patches and diagnostic instrumentation are evidence-only, not
part of `code.patch`. Preserve the failed controls as failures. No skip, broad
is-a replacement, fabricated widget, forced grab, or changed production default
is used to obtain a passing final gate. This is native scoped regression
qualification, not a new ASan or full-repository migration qualification.

## Final result

The exact final TRUE binary passes the complete **62/62 compact tests, zero
skips**, including the original explicit-FALSE preference persistence and dock
transfer tests. After the FALSE and injected-negative controls, restoration and
rebuild produce a byte-identical binary to this aggregate-tested executable.
Wrapper exit 250 on assertion-abort cases is retained as a failure, not relabeled
as a pass. Names containing `green` in exploratory historical logs are merely
attempt labels; `exit_status`, failed assertions, and the final cases are the
actual verdicts.

### Registered Meson gate

The registered `gimp:app / painter-compact-options` test also passes **62/62,
zero skips**, exit 0, with the same final binary (136.10 seconds). The initial
automatic headless setup attempts failed before entering the test: that legacy
wrapper tries its own 1280×1024 display and a D-Bus socket prohibited in this
sandbox. A first reconfigure command omitted its required explicit source path
and did not change the setup; both pre-test failures are retained in
`registered-results.json`.

The supported `headless-tests=disabled` build configuration disables only that
nested wrapper. The repository's authenticated portrait Xvfb wrapper then runs
Meson's registered test with `--no-rebuild`. No executable or assertion changed.
The original `headless-tests=auto` configuration was restored afterward, and
the final binary remained byte-identical. For this sandbox, use:

```sh
meson setup --reconfigure "$BUILD" "$SOURCE" -Dheadless-tests=disabled
python3 "$SOURCE/tools/run-painter-xvfb.py" meson test -C "$BUILD" \
  --no-rebuild --num-processes 1 painter-compact-options
meson setup --reconfigure "$BUILD" "$SOURCE" -Dheadless-tests=auto
```
