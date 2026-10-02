# Painter rotation input and modifier routing

The pinned old route is `app/display/gimpdisplayshell-tool-events.c` at
`afa43fae3e920210146abed514f136fd49f671b5`. Default middle-button initiation
checks the physical Shift bit first (rotation), then Ctrl (zoom), otherwise pan.
Additional modifier bits do not disable the old default route. An inherited
rotation checks the Ctrl bit on every motion, even after Shift release. It keeps
one start-relative angle, so snapping never overwrites the unsnapped reference.

The arithmetic retains the integer viewport midpoint, `atan2(rx, ry)`, positive
normalization before integer half-step rounding, and 15-degree wrap to zero.
GIMP 3's existing rotate-to path handles transforms, viewport preservation,
overlays, invalidation and angle actions. Its matrix order is `Rotate * Flip`;
the old order was `Flip * Rotate`. Consequently the legacy logical angle is
negated at the modern boundary for one-axis reflections. Comparing raw angles
without this conversion would reverse the displayed rotation under mirroring.
Vertical reflection is a modern extension; the same matrix parity applies, but
no old vertical-reflection oracle is claimed.

Only the old normal arrow-key press route swaps Left/Right under horizontal
reflection. The wants-all-key-events early route, key release and other keys are
unchanged. A local event copy avoids modifying the shared incoming GDK event.

## Explicit customization

An explicit rotating or step-rotating binding retains its chosen constraint;
Ctrl switching applies only to the inherited painter route. Drag lifetime tracks
the initiating button number, including buttons without GDK state-mask bits.
Modifier release cannot end a held button drag. A different button release does
not finish it, and the existing active-grab guard rejects second button presses.
Picker completion and ordinary stop clear the new fields before the next action.

Reading the modifier list for Preferences does not create persistent overrides.
New middle-button customizations record a `painter-defaults` marker in the
existing modifiers configuration. Only exact edited combinations override the
inherited bit-mask behavior; explicit NONE overrides survive serialization. This
prevents changing one binding from dropping every additional-modifier default.
Existing GIMP 3 exact-match configurations, without the marker, retain their
original exact-match semantics. Button 3 and other explicit bindings remain
outside painter default inheritance.

## Verification and remaining gates

- `legacy-navigation` contains 5,184 executions of the unchanged old start/update
  function bodies, with GTK grab/cursor/expose replaced by no-op stubs. It is
  source-arithmetic evidence, not a real old GUI or tablet capture.
- Five portable groups test that oracle, equivalent Cairo matrices, midpoint and
  0/360 rounding, repeated stationary Ctrl transitions, extra modifier bits,
  mirrored key routing and explicit rotation constraints. Normal and focused
  ASan/UBSan pass; dependency libraries are uninstrumented and LSan is disabled.
- The separate native GTK test uses the real default GdkDevice for read-only
  Preferences, inheritance/disable save-reload, legacy exact configurations and
  explicit mappings on buttons 2, 3, 4, 5 and 8. Its actual display-run evidence
  is recorded separately from the source-arithmetic tests.

Whole physical drag event sequences, interruption/focus/device-switch cleanup,
mouse/tablet equivalence, macOS/Windows, radial legacy drag zoom and transformed
pan integration remain explicit gates. The initial rotation increment left zoom on the current GIMP route; the
radial-default follow-on below supersedes that temporary state. Touchpad gesture rotation is
unchanged. These tests do not establish all of section 25.

## Follow-on: inherited radial drag zoom

The inherited Ctrl+middle route now stores
`max(scale_x, scale_y) * 300 - initial_radius` and requests
`(current_radius + anchor) / 300` about the integer viewport center. It uses the
old retain-centering/best-guess focus policy. Explicitly customized zoom bindings
continue using the current GIMP drag-zoom preference; the inherited default is
not silently replaced by exponential vertical movement. Modifier release while
the initiating button remains held does not reset the anchor.

A separate `legacy-zoom` source-arithmetic fixture runs the unchanged old
start/update bodies for 2,400 anisotropic-scale, mirror, odd/even-center and
inward/outward/stationary cases. Normal and ASan/UBSan now pass six portable
navigation groups. The scale API's clamping and full display event sequences
remain separate integration tests; the oracle records raw requested factors,
including negative values rather than pretending they are valid displayed zoom.
Transformed pan and physical mouse/tablet comparisons remain unfinished.

## Native display event route

Four full-GUI tests now dispatch synthetic GDK button, motion and key events
through the real GimpDisplayShell canvas handler on the native cloud display:

- Default Shift/Ctrl/additional-modifier rotation with horizontal reflection,
  stationary Ctrl release and fixed-anchor angle restoration
- Explicit step rotation on buttons 4, 5 and 8, modifier release while held,
  rejected second-button press and ignored unrelated-button release
- Inherited radial zoom through the real display scale API and anchor cleanup
- Layer-picker modifier release before button release, without a stale grab

The last case first failed: state was cleared but the pointer remained grabbed.
The picker branch now uses the ordinary stop-scrolling cleanup. All four normal
and all four focused ASan/UBSan cases pass. The builder instruments the math,
modifier manager, rotate/event adapter and test translation units, not all GTK or
core dependencies. The harness initializes GIMP's normal first-focus lifecycle
before synthesizing input; bypassing that setup had initially left its deferred
device manager uninitialized. Neither failure was hidden by relaxing assertions.

Reports and original diagnostics are `migration/tests/navigation-events-*`.
Synthetic native events extend the earlier pure arithmetic/configuration proof;
physical tablet input, full focus/device transitions and transformed pan remain
separate gates. Existing GUI test-profile diagnostics are recorded in the result
manifest rather than suppressed.
