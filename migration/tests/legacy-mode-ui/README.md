# Original 13.009 mode-selector integration checkpoint

The test uses the **real layer dock's GimpLayerModeComboBox**, not a replacement
widget. With a selected native layer it selects all seven Painter entries via
GtkComboBox's normal active-value/changed-signal path and verifies:

- each historical raw ID 23–29 maps to the registered distinct Painter enum;
- the visible model label and selected integer value agree;
- the selected GimpLayer's mode changes to that same enum.

This is **widget-signal integration**, not synthesized mouse/keyboard input.
The final PNG is the rendered GIMP desktop on authenticated 1080×1920 Xvfb,
with `painter-canvas-ui=TRUE` and actual oncanvas visibility asserted. The
selected mode shown is Replace (Painter legacy). Source layers are generated
color/alpha patterns, not imported user artwork. The full desktop screenshot
is captured directly through GDK; it is not a mockup or composite illustration.

The initial test expected `Anti-Erase` but the actual registered label is
`Anti erase (Painter legacy)`; that test expectation was corrected, not the
production UI. The seven registered labels and values now pass. This evidence
does not close the original predecessor dependencies, group compositing,
artwork comparisons or high-precision extension. No blocked Painter popup-owner
or layer-child retirement scope is changed or tested.

Run with the native build environment and shared build lock:

```
GIMP_TEST_MODE_SCREENSHOT=/tmp/modes.png python3 tools/run-painter-xvfb.py \
  meson test -C BUILD painter-mode-ui --print-errorlogs
```

Evidence reports deliberately omit the inherited process environment.

Native and focused ASan/UBSan both pass. The focused sanitizer instruments only
GimpLayerModeComboBox, GimpLayerModeBox and this C test; remaining dependencies
are not instrumented and LSan is disabled. The dock is opened through the normal
factory raise path (not the factory constructor, which leaves it unparented).
