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

## Original 25.001: source-defined start acceptance matrix

The added cases in `app/tests/test-painter-navigation-events.c` target only the
original **25.001 button/modifier/tool-state** criterion. Native execution of
this source checkpoint is **pending**; the original row remains unchecked.
No production change or new child WBS row is part of this increment. Dependencies
02.015 and 03.009 are already DONE. The four preceding event tests and their
historical receipts remain unchanged.

### Expectations fixed before execution

All old line references below name the retained full
`migration/fixtures/legacy-rotation-operations/original-sources/app/display/gimpdisplayshell-tool-events.c`
at `afa43fae3e920210146abed514f136fd49f671b5` (SHA-256
`7b81d99ab288247d1452ee47053731a4c71eb8c40fa54c734f87023d31757c84`).

| Source-defined input or precondition | Expected modern start case | Old source |
| --- | --- | --- |
| Middle with no modifiers, Alt, Super, or Meta | Inherited pan; four separate rows | 829–842 |
| Middle with Ctrl, Ctrl+Alt, or Ctrl+Super+Meta | Inherited zoom; three separate rows | 833–835 |
| Middle with Shift, Shift+Super, or Shift+Meta | Inherited rotation; three separate rows | 831–832 |
| Middle with Shift+Ctrl or Shift+Ctrl+Alt | Rotation wins over zoom; modern STEP_ROTATING, with no immediate snap or angle change | 831–832, 1813–1839 |
| Application busy | Reject without replacing navigation, anchors, or grab | 747–748 |
| Middle while primary or tertiary input mask is held | Reject; each held mask tested separately | 750–761 |
| Unbound button 8 with Shift | No inherited navigation; focus/modifier update is still allowed | 777–781, 808–842 |
| Existing inherited pan, zoom, or rotation | Reject a new Shift+middle at a different position and preserve the original navigation/grab | 747–748 plus grabbed start paths |
| Legitimate primary tool drag | Reject Shift+middle while its actual tool control and actual pointer grab remain occupied; release the original drag normally | 747–761, 808–827 |
| Every rejected setup after its ordinary cleanup | Accept a subsequent Shift+middle with a new anchor and grab | Same start and stop lifecycles |

The modifier table has twelve fixed rows with literal expected actions, never
computed by the production routing helper. It is run with explicitly selected
real `gimp-paintbrush-tool` and real `gimp-move-tool`, plus a supplemental
recording tool. Super/Meta rows check extra-bit tolerance; the backend may filter
these bits for action lookup, so this is not independent-binding/platform proof.

Accepted starts check the same tool object, registered tool information, tool
ID and control object; control remains idle, focus points to the display, and
the idle modifier state contains the supplied modifiers plus button 2. Primary
button/active-modifier fields remain unchanged. Initial angle and scale remain
unchanged; initiating button 2, inherited flag, scroll origin, fixed rotation
anchor and zoom start anchor are established. The simple unmirrored
bottom-center ray gives an independently known zero bearing and radius; the
zoom start reference is `max(scale_x, scale_y) * 300 - radius`. This does not
expand the test into drag/snap arithmetic. Both the shell's seat/pointer/source fields
and GDK's actual device-grab query must confirm the grab. The synthetic seat
master is the grab pointer; its source field is NULL by the intentional
`gimp_devices_get_from_event()` master-device contract, not a physical device.

Move's real modifier callbacks may update its options; Paintbrush may toggle
its internal color-pick mode. Neither is a tool replacement or control
activation. Middle release clears navigation/anchors/grab but does not itself
clear the idle tool's last modifier state. Rejected input is not required to
freeze all application state: device/display selection precedes some guards,
and unbound input still updates focus/modifiers. Navigation and grab snapshots
are exact, including both anchors and all scroll coordinates.

The legitimate primary-drag sequence is run once with the real Paintbrush and
once with the recorder, through the public canvas press/release handler. It
requires active tool control **and** a real native grab before the rejection
probe. Even release of the rejected middle press must leave that original drag
occupied. Normal primary release precedes accepted reentry. The recorder also
proves that all twelve accepted navigation routes dispatch no primary press or
release callback, while its legitimate primary sequence dispatches exactly one
of each. It supplements, and does not replace, the real-tool tests.

There is **no blanket active-tool-control veto** in the old middle-button route.
No production guard is added, and no artificial active-tool-without-grab
lifecycle is constructed. Tool-specific flags used by primary-button handling
are not invented as rotation entry conditions.

### Direct use of retained positive observations

The native test reads the existing `gui-captures/rotation.tsv` and verifies its
SHA-256 `bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a`.
It uses unmirrored sequence **2** (Shift+middle) and **222**
(Shift+Ctrl+Alt+middle), each with both real tools. Their preceding setup row
supplies the initial angle; recorded button/modifiers, handled state, exclusive
rotation classification, angle/anchor and grabbed state are checked directly.
Coordinates retain the recorded offset from the integer viewport center.
This narrowly reuses two already-captured positives; it adds no mirror-motion,
angle-rounding or toolbar acceptance.

The retained full fixture contains 22 Shift+middle and 2 Shift+Ctrl+Alt+middle
starts, but no tool identities, tool-control/focus/modifier fields or negative
start traces. Those modern expectations are **pinned-source-derived**, not a
new paired old/new tool-state replay. Both old GUI captures still terminate
with **command_exit=-11 during teardown** after completing their rows. No old
executable or capture is run for this increment. Synthetic native events do not
establish physical tablet/mouse behavior, focus interruption, all platforms, or
all of section 25.

### Finite execution inventory and native handoff

The executable has **11 registered GTest cases**: the original four plus these
seven new registrations:

- `/navigation-events/start/paintbrush`: 12 real-tool modifier starts
- `/navigation-events/start/move`: 12 real-tool modifier starts
- `/navigation-events/start/retained-old`: 2 retained starts × 2 real tools = 4
- `/navigation-events/start/reject-paintbrush`: 7 rejected probes, 7 accepted
  reentries, and 3 accepted occupied-navigation setups
- `/navigation-events/start/reject-move`: the same 7/7/3 finite matrix
- `/navigation-events/start/primary-paintbrush`: 1 legitimate primary setup,
  1 rejected middle start and 1 accepted navigation reentry
- `/navigation-events/start/recording-tool`: 12 modifier starts, 1 legitimate
  primary setup, 1 rejected middle start and 1 accepted navigation reentry

Thus the new cases contain **62 accepted middle starts** (including 6 occupied
setups and 16 reentries), **16 rejected navigation probes**, and **2 legitimate
primary setups**. These are subcases, not 80 separately registered GTests. The
unchanged four original tests retain their own existing sequences.

Required validation is a full default modern build, then the registered
`painter-navigation-events`, `painter-navigation-ui`, and `painter-navigation`
targets, followed by focused event ASan/UBSan from the same frozen source. In the
configured build environment, under the shared build lock, the owner runs:

```
ninja -C "$BUILD"
meson test -C "$BUILD" --no-rebuild --print-errorlogs \
  painter-navigation-events painter-navigation-ui painter-navigation
python3 "$SOURCE/migration/tests/build_navigation_event_sanitizers.py" \
  "$BUILD" --report "$EVIDENCE/navigation-start-sanitizer-build.json"
env GIMP_TESTING_ABS_TOP_SRCDIR="$SOURCE" \
  GIMP_TESTING_ABS_TOP_BUILDDIR="$BUILD" \
  GIMP_TESTING_PLUGINDIRS="$BUILD/plug-ins/common" UI_TEST=yes \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  "$BUILD/app/tests/painter-navigation-events-asan"
```

Use an authenticated native display with correct `DISPLAY`/`XAUTHORITY` and the
configured runtime dependency environment. `$SOURCE` must match the configured
compile database; transfer/commit the checkpoint there before rebuilding. The
retained fixture path must exist under `GIMP_TESTING_ABS_TOP_SRCDIR`. The actual
Paintbrush primary setup also requires the test profile's brush and enabled
paint-dynamics resources. Native runs must report all 11 cases, not only the
seven new ones; a display skip is not a pass. The sanitizer builder instruments
five event/navigation/test C units; underlying core, GTK and dependencies remain
uninstrumented and LSan is disabled. Preserve all historical receipts: do not
use the older runner that overwrites `navigation-sanitizers.json`.

The parent integration owns native acceptance, exact-source result receipts,
any final original 25.001 ledger decision, and the later strict Clone source
freeze. A source checkpoint or offline fixture integrity pass alone does not
close 25.001.
