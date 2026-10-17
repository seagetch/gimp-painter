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
original **25.001 button/modifier/tool-state** criterion. The bounded native
matrix passed on the exact source recorded below: **21 normal cases across three
targets and 11 focused event ASan/UBSan cases**. This evidence-only checkpoint
does not close the original row; parent closure requires the later strict Clone
freeze on the receipt-integrated source. No production change or new child WBS
row is part of this increment. Dependencies 02.015 and 03.009 are already DONE.
The four preceding event tests are rerun; their historical receipts remain
unchanged.

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

### Native GDK grab-observation timing

The first compiled native attempt (component run 002) passed the default build
and focused five-unit sanitizer build. Both normal and instrumented event runs
passed the original four cases, then failed the GDK no-grab assertion in the
first new Paintbrush/plain case. The shared assertion's log alone does not
identify whether that observation followed setup or the first release. UI 4/4
and arithmetic 6/6 passed; this failed event run is retained, not accepted.

The configured dependency is GTK 3.24.49. Its
[GDK X11 ungrab path](https://github.com/GNOME/gtk/blob/3.24.49/gdk/x11/gdkdisplay-x11.c#L1780-L1840)
marks the grab's ending serial and requests an asynchronous server roundtrip.
[The reply handler](https://github.com/GNOME/gtk/blob/3.24.49/gdk/x11/gdkasync.c#L667-L760)
then schedules the grab-update callback as an idle source.
[The public grab query](https://github.com/GNOME/gtk/blob/3.24.49/gdk/gdkdisplay.c#L1433-L1455)
reads that cached explicit-grab record rather than issuing a fresh server query.
Therefore a direct synthetic canvas call cannot require the record to disappear
before the roundtrip and idle callback have run.

The test keeps every immediate shell/action/anchor/grab-pointer cleanup assertion
before a deterministic `gdk_display_sync()` and ordinary main-loop drain, then
requires the same GDK no-grab assertion. It does not issue an ungrab, reset state,
sleep, poll until success, or discard input events. A release-phase message and
pre/post synchronization diagnostic make the observation visible. The existing
start-time grabbed assertion and all rejection snapshots remain unchanged.
This is a source-supported observation-timing correction, not a production
change. Run 003 passed both real-tool modifier matrices before encountering the
retained-fixture schema mismatch described below. The accepted frozen run 004
passes all 11 event cases normally and under the focused sanitizers, with the
pre/post synchronization diagnostics preserved.

### Finite execution inventory

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

### Accepted native component receipt (2026-10-17)

`migration/tests/rotation-start-component-receipt.json` is the publication
summary. `migration/tests/rotation-start-component-evidence.tar.xz` preserves the
unchanged public native receipt, commands, TAP output, source/fixture hashes,
runner bytes and integrity snapshots for `component-run-004`:

- Tested source A: `3634001805313a297e259aea0d3020b1f6805d04`
- Tested tree: `af466e90bbe5ac396c48360176d27fcfc3f2c932`
- Event source SHA-256: `22bf8d4fa9c4704619c4acf85147011a16a28a47303686fbd214ea544cf59646`
- Native receipt SHA-256: `355ce4bb641efecefea161015a4209ce1750787bb4ee2d935673f749c059fa09`
- External runner SHA-256: `30df9978f0877c9109b7160ae2c43310936eb762a482086b39ec612d6abaf53b`
- Pinned `gimp-data`: `a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`

The recorded command order was:

1. Full default `ninja -C /workspace/shared/gimp-clone-component-build -j4`:
   exit 0, 97 incremental steps. This is not a clean-from-empty build claim.
2. The unchanged `migration/tests/build_navigation_event_sanitizers.py` with
   that configured build and a separate private report: exit 0.
3. `meson test -C /workspace/shared/gimp-clone-component-build --no-rebuild
   --num-processes 1 --print-errorlogs --logbase <private-attempt>/meson
   painter-navigation-events painter-navigation-ui painter-navigation`: exit 0.
4. The builder's `app/tests/painter-navigation-events-asan`, with the actual
   environment of the exact registered event target and its recorded workdir:
   exit 0. Only ASan/UBSan options were overridden.

The exact argument arrays, workdirs and timestamps are in the unchanged receipt.
The original external runner is archived as `external-runner.py`; it uses the
accepted committed `tools/run-painter-xvfb.py` directly, a single authenticated
loopback-TCP 1080×1920 display, isolated writable profile state and the shared
build lock throughout builds, tests and all seals. It preserves the normal
Meson dependency-library environment rather than reconstructing it. The runner
constants retain `detect_leaks=0:halt_on_error=1:abort_on_error=1` and
`halt_on_error=1:print_stacktrace=1`; the committed public-log redactor removes
those environment-setting strings from the public report. No suppression file
or suppression option is added.

The three exact registered normal targets passed: `painter-navigation-events`
11/11, `painter-navigation-ui` 4/4 and `painter-navigation` 6/6, for 21 positive
TAP cases. Focused event ASan/UBSan passed the same 11/11 event registrations.
There were no skipped GTests, incomplete plans, crash diagnostics or sanitizer
errors. The five instrumented C units are the navigation arithmetic, modifier
manager, rotate adapter, canvas event adapter and event test. Other GIMP, GTK
and dependency units remain uninstrumented; LSan is disabled. The actual
Paintbrush primary setup uses the profile's brush and enabled paint dynamics.

All nine integrity checks are true: unchanged source; unchanged normal ELF;
normal ELF preserved by the focused builder; unchanged full build freshness;
unchanged compile database; unchanged full registered-test metadata; unchanged
sanitizer artifacts; preserved sanitizer copies matching originals; and all
361 compiled locale catalogs unchanged. Four clean source snapshots cover
before build, after default build, before tests and after tests. The three
normal ELF snapshots each contain 2,873 entries. Catalog content and installation
metadata are independently read after testing. The full registry is sealed for
identity and freshness; it was not run as a full aggregate.

Native logs retain missing application icons, ignored dummy plug-in entries,
unavailable session/cloud-provider D-Bus and GLib setenv/unsetenv thread-safety
debug messages. Empty diagnostic-classifier warning arrays do not mean a
warning-free application. Dummy plug-in discovery messages are not skipped
GTests. Default/focused build logs and every original diagnostic remain stored.

The compact archive's `compact-manifest.json` binds all 22 included original
public members, including `receipt.json`, by SHA-256. It separately names and
hashes seven omitted copied executable/object/thin-archive artifacts. Their
hashes remain in the unchanged native receipt and `sanitizer-artifacts.json`;
the compact archive does not contain those binary bytes. The complete public
package, including those copies, is retained separately as
`component-run-004-passed-public.tar.xz`, 28,283,800 bytes, SHA-256
`799ad7c7edca5992e095f7c792fa2e622eef455835acc399df30aff0b7b909a0`.
Neither package contains private profiles, raw environment objects or
Xauthority data. The copied thin archive is not a standalone rebuild package.

### Retained unsuccessful attempts

The new summary records each exact source/tree, original receipt SHA-256,
command exits and separately retained full public failure-archive SHA-256:

1. Run 001, source `6a5df001a61b2093eba7937b2aafd07668b8f094`, stopped in the
   default build on undeclared `gimp_tools_set_tool_options_gui_func`. No native
   tests ran. Adding the missing `widgets/gimpwidgets-utils.h` include fixed compilation.
2. Run 002, source `bfcb8b8ab720ca4a92978a647c4125d8c5994ffc`, reached the GDK
   no-grab assertion described above. Event normal/sanitizer each passed only
   the original four cases; UI 4/4 and arithmetic 6/6 passed. It remains failed.
3. Run 003, source `e946cf1716c8485535396648bb2358c39386d0f0`, passed six event
   cases normally and under sanitizers, then failed the retained parser's
   25-column assertion. The unchanged header and all 370 data rows have
   24 columns. The accepted source changes that schema-width assertion to 24,
   preserving fixture bytes, consumed field indices and behavioral assertions.
   UI 4/4 and arithmetic 6/6 passed; the event result remains failed.

No earlier receipt or failure is overwritten or relabeled. Both old GUI
captures' teardown `command_exit=-11` and the source-derived real-tool-state
limits remain as stated above; no old executable is run here.

### Integration boundary and remaining gates

This receipt establishes the bounded native start matrix only for tested
source A above. Its receipt-only descendant B is not relabeled as natively
tested for navigation. The parent must run the unchanged strict Clone gate
(226 normal / 62 focused sanitizer cases) on that receipt-integrated source
before deciding original 25.001 ledger closure. This compact component receipt
is not valid input for `check_clone_acceptance.py --current-evidence`.

Physical mouse/tablet equivalence, focus/device interruption, other platforms,
all of section 25, the historical full aggregate, packaging/release and skipped
old Edge/string-reader runtime remain outside this component acceptance.

## Original 25.004: deferred Ctrl timing regression

The `/navigation-events/rotation/ctrl-mid-drag` case is accepted by the bounded
component run recorded below, with ledger closure pending. It replays the single unmirrored stationary old
GUI sequence 22–30 with each real Paintbrush and Move tool. The retained
`legacy-rotation-operations/gui-captures/rotation.tsv` is checked against its
24-column header, the consumed row widths and SHA-256
`bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a`.
Literal event types, pre-event modifier masks, key/button details, center-relative
coordinates and old angle/anchor/held-state expectations are checked before
modern events run. No production arithmetic helper supplies expected angles.

The finite sequence starts at +7.5 degrees with Shift+middle. Free stationary
motion and Ctrl press alone remain +7.5; the next Ctrl motion snaps to 15.
Shift release and Ctrl-only motion retain 15; Ctrl release alone also retains
15; the next stationary motion restores +7.5. Key events must leave the entire
navigation snapshot unchanged. Every held step preserves the original anchors,
initiating button, inherited action, scroll coordinates, scales and actual grab.
Ordinary middle release and a subsequent accepted start/release use the existing
cleanup/reentry helpers. This is one registered case over two tools, not a new
recorder or general input matrix; all eleven preceding registrations are unchanged.

Timing follows the pinned old handler's motion dispatch (1145–1154), modifier
key branches (1436–1456 and 1517–1542), and rotation update (1842–1871). The
modern key branches update tool modifiers without recalculating navigation; its
motion adapter selects the current Ctrl constraint while retaining the start
anchor. The reverse transition tightens 25.005 timing evidence without closing
that row. Both original GUI captures retain teardown `command_exit=-11`; no old
runtime is executed or reclassified, and its skipped/unverified status remains.

The source checkpoint required exact-source acceptance of the three existing
normal targets (12 event + 4 UI + 6 arithmetic cases) and the five-C-unit focused
event ASan/UBSan target (12 cases). The following receipt records that bounded
run; physical input, other platforms, display-boundary acceptance and the
remaining original rows stay outside this component checkpoint.

### Accepted Ctrl-timing component receipt (2026-10-17)

`migration/tests/rotation-ctrl-component-receipt.json` and the matching
`rotation-ctrl-component-evidence.tar.xz` preserve the accepted
`component-run-001`, with distinct exact identities:

- Tested source A: `edf861423cd438f275332c0e7f3b899dc3126d51`
- Tested tree: `cdb92379f0c983dbfe6c0473993e4c58ea43e9b9`
- Event test SHA-256: `bc06069a04b1d8816279854485c75aa9c8f675ce409649612c1a6e6821936257`
- Native receipt SHA-256: `43e1de63ef396f2c7a9547793dfb5791426843f8470b775648b51c3ebba92ad7`
- Pinned `gimp-data`: `a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`

From 10:05:34 to 10:07:18 UTC, the recorded full default incremental build,
unchanged focused sanitizer builder, three exact registered normal Meson
targets and focused event executable all exited 0. Exact argument arrays,
workdirs, timestamps and runner bytes are retained. The normal results are
**12 event + 4 UI + 6 arithmetic = 22 positive TAP cases**; focused event
ASan/UBSan passed **12/12** with all five instrumented C units. The new timing
case replays rows 22–30 with each real tool; this is one registration over two
tools, not 18 new registered tests. No skipped GTest, incomplete TAP plan,
crash or sanitizer failure was accepted.

All nine integrity checks are true, with four clean identical source snapshots,
three unchanged 2,873-entry normal ELF snapshots, full build freshness, compile
database, registry, focused artifacts/copies and all 361 locale catalogs sealed.
The original 28 file hashes were checked. The compact archive keeps 22 original
public members plus its manifest; seven copied sanitizer binary/object/archive
members are hash/size-bound and retained in the separate complete public archive.
That archive is `component-run-001-passed-public.tar.xz`, 21,680,848 bytes,
SHA-256 `19032ddda16c72a031edaf5afb6c98f8cf61f7d21f710b8eed9c3605911fc441`.
No source, fixture, test or committed procedure changed in this publication.

The actual registered Meson environment was reused with only the recorded
ASan/UBSan overrides. LSan remains disabled, with no suppression files/options;
other GIMP, GTK and dependency units remain uninstrumented. Logs retain missing
icons, dummy plug-in discovery, unavailable D-Bus and GLib environment-thread
messages; diagnostic-classifier arrays do not establish a warning-free run.
Private profiles, raw environments and authentication data are excluded.

### Separately source-bound portrait GUI smoke

`migration/tests/rotation-portrait-gui-receipt.json` and the matching
`rotation-portrait-gui-evidence.tar.xz` preserve a bounded modern GUI smoke on
**older source** `22708f41d368c6c7c2e425d30ea2765b9d29158b`, tree
`0a48c7e042ae2f2b46e6cfe828926470e1ace9ab`. Its actual default build and install
passed; it is not a GUI run of component source A or this evidence descendant B.

Ordinary XTest input on an authenticated loopback-TCP 1080×1920 portrait display
produced the uninterrupted same-drag visible angles **28.43 → 30 → 30 → 28.43 →
28.43 degrees** through free rotation, Ctrl snap, Shift release, Ctrl release and
middle release. All 19 requests succeeded, with no geometry retry or idle input
release. Artwork and Painter controls remained visible. Normal Save/Open dialogs
saved and reopened the XCF with unchanged SHA-256
`01a1846108e57776bed5b132928f6524de81c7d936dfe2be4b7cb60616c5c931`.
The reopened view returned to zero rotation; the same-geometry visible artwork
region was byte-identical. This compares rendered pixels, not internal buffers
or all XCF semantics. A short post-reopen rotation again showed 28.43 degrees.

Application, driver, launcher and display exited 0, all held inputs were cleared,
and source/runtime/binary seals matched. Logs retain 112 ignored unsupported
fonts, unavailable session bus and 47 dconf warnings; no GIMP CRITICAL was logged.
The compact archive retains 111 original public files plus its manifest and binds
19 raw screenshots, the synthetic PNG and the saved XCF by hash/size without
copying those image/document bytes. The complete sanitized archive is
`gimp-modern-rotation-portrait-22708f41-evidence.tar.gz`, 6,730,062 bytes,
SHA-256 `05c56367dfe4cc4d416fc23a2610fb29d24d232513cb01774b78ccefbfd6f877`.
Private profile, raw environment and Xauthority/authentication material are excluded.

### Receipt-integration boundary

These source-bound results do not claim a native or GUI run on this receipt-only
integration descendant B. Adding tracked evidence invalidates the previous
strict Clone source seal; the parent must rerun the unchanged **226 normal / 62
focused sanitizer** Clone gate on B before deciding original 25.004 closure.
Neither compact archive is valid input to
`check_clone_acceptance.py --current-evidence`. The ledgers remain unchanged;
25.005 timing is supported without closing that row. Physical mouse/tablet,
focus/device interruptions, other platforms, full task 25, full port, historical
full aggregate and packaging/release acceptance remain outside this checkpoint.
Both retained old GUI captures still have teardown `command_exit=-11`; no old
runtime was executed or reclassified, and skipped/unverified old-runtime status
is preserved.

## Original 25.007: inherited zero-boundary component evidence

The bounded exact-source component run below passed **28 normal cases and 18
focused event ASan/UBSan cases on its first attempt**. The original
no-angle-jump/no-direction-reversal criterion remains unchanged, and **25.007
remains open pending the parent strict Clone rerun and final acceptance**.
The preceding 12 registered event cases and every retained fixture byte remain
unchanged. Six new event cases are bound to the new source A below; earlier
native receipts cannot attest to this changed implementation or test source.

### Narrow displayed-angle policy and lifecycle

The old retained `gimpdisplayshell-tool-events.c:1842–1871` assigns the normalized
angle without a zero epsilon, and its `gimpdisplayshell-rotate.c:45–62` constructs
the rotation without one. The modern shared transform builder previously changed
angles inside 0.001 degrees of zero to exactly zero. This creates a zero plateau
and discontinuities at its edges, including genuine old-display disagreement
at the interior samples below. It is not merely a 360-degree labeling issue.

A `rotate_angle_exact` field now belongs to the **current displayed angle**.
Only an inherited painter drag update selects it, through a private common
rotate-to implementation. The ordinary public rotate-to API always clears it,
including calls from relative rotation, actions/reset, the angle dialog and
native touch gestures. Explicit custom drag bindings also clear it when their
first motion replaces the angle; a press alone does not replace the prior view.
The ordinary 0.001-degree epsilon and its represented-value endpoint behavior
remain unchanged. The exception does not alter arithmetic or modifier routing.

The transform builder skips only that epsilon normalization for an exact angle;
it still constructs the real Cairo transform and inverse normally. Exact zero
still uses NULL transforms when unreflected. Release clears the input anchor
and grab but preserves the displayed-angle provenance. Scroll/scale, rotated,
scrolled and scaled signal rebuilds preserve it too. Flip transforms the existing
angle while keeping its provenance. Emptying the shell clears it alongside the
angle and reflections; a newly allocated shell starts with the ordinary false
value through normal GObject zero initialization. The direct shell-angle write
audit found only the common setter, epsilon normalization, flip and empty reset.
No matrix is patched in the test or after the setter.

### Four ordered source-derived crossings

Each case uses the real Paintbrush, an unmirrored zero-angle view, cleared custom
bindings and a normal Shift+middle press at `(cx+80,cy)`, with Ctrl also held for
snapped cases. The integer viewport midpoint and that ray give an immutable
**90-degree anchor**, independently of the navigation helper. Motion is at
`(cx+100000,cy+k)`, preserving the production integer coordinate path. These
out-of-canvas synthetic events are legal handler inputs, not a claim about a
physical mouse trajectory. The first observation positions the crossing; only
subsequent observations establish its ordered direction.

The old source implies the geometric angle `atan(k/100000) * 180/pi`, wrapped to
`[0,360)`. The test uses fixed analytical constants, not values read back from
the target or generated by a newly executed old oracle:

| Case | Ordered k values | Expected wrapped angles in degrees |
| --- | --- | --- |
| Free increasing | -2, -1, 0, 1, 2 | 359.9988540844099, 359.9994270422049, 0, 0.0005729577951117246, 0.0011459155901088576 |
| Free decreasing | 2, 1, 0, -1, -2 | Reverse of the preceding values |
| Snapped increasing | -25000, -2, -1, 0, 1, 2, 25000 | 345, 0, 0, 0, 0, 0, 15 |
| Snapped decreasing | 25000, 2, 1, 0, -1, -2, -25000 | 15, 0, 0, 0, 0, 0, 345 |

These four cases contain **24 ordered motion observations (5+5+7+7)** and four
ordinary starts/releases. Free `k=+/-1` is safely inside the ordinary epsilon;
`k=+/-2` is safely outside it. Snapped endpoints have raw magnitude
14.036243467926477 degrees, making entry to and exit from the zero plateau
non-vacuous. Nominal decimal epsilon endpoints are not used for these crossings.

The actual `gimp_display_shell_rotate_xy_f()` is observed at a common point and
two 100-pixel basis offsets. Subtraction cancels viewport translation. The free
x basis is analytically `(10000000,100*k)/sqrt(100000^2+k^2)`; the y basis is its
quarter-turn `(-vy,vx)`. The fixed free magnitudes are
`(99.999999995,0.00099999999995)` at `|k|=1` and
`(99.99999998,0.0019999999996)` at `|k|=2`. The snapped nonzero basis is
`(96.59258262890683,+/-25.881904510252074)`. Both columns, radii, determinant,
angle and actual transform/inverse presence are checked. Zero/NULL and the
90-degree anchor are exact assertions. Nonzero angle and signed-step tolerance
is 1e-10 degrees; basis-component tolerance is 1e-8 pixels, far below the original
inner-band mismatch. Successive actual x bases supply signed shortest steps
via `atan2(cross,dot)`: free steps must progress strictly in the requested
direction, while the snapped middle steps are exactly zero. The numerical label
359.x to 0.x is not mistaken for reversal. This finite matrix does not by itself
prove mathematical continuity for all input points or full translated-position,
overlay or physical-device equivalence.

Every held motion checks the unchanged start/zoom anchors, start coordinates,
initiating button, stored action, inherited flag, scale and grab identity, while
last coordinates update normally. The active real tool remains idle and GDK's
actual grab must persist. Ordinary release uses the existing immediate shell
cleanup followed by synchronized GDK no-grab assertions. Diagnostic output names
the case, k, expected source-derived angle, actual angle/basis and signed step.

### Bounded persistence and ordinary/custom isolation

Two additional registered cases supplement, and are not part of, the 24-motion
crossing count:

- Persistence uses both inner free samples, ordinary release, two direct rebuilds,
  an ordinary offset change, scale update and horizontal flip/unflip. Actual
  basis and angle must remain tiny and nonzero. Closing the only display then
  exercises normal shell-empty reset and requires false provenance, zero angle,
  no reflections and NULL transforms.
- Isolation first establishes a genuine inherited tiny angle before each ordinary
  replacement. Positive/negative inner ordinary rotate-to requests still collapse;
  public `rotate_to(0.001)` retains that represented value, while
  `rotate_to(359.999)` collapses because its represented distance to 360 is below
  the epsilon. An ordinary relative rotation also clears provenance. An explicit
  custom free-rotation binding starts from an inherited tiny angle and moves on
  the same ray, requiring the ordinary epsilon rather than stale exact policy,
  while retaining normal anchor/grab lifecycle.

This adds six registrations for **18 event cases total**. The completed component
gate used the unchanged default build and all **18 event + 4 UI + 6
arithmetic = 28 normal cases**, plus **18 focused event ASan/UBSan cases**.
The focused sanitizer instrumentation remains its five translation units (test,
navigation math, modifier manager, rotate adapter and event adapter); the shell
empty-reset translation unit is exercised normally but not newly instrumented.
The receipt records the actual source tree/inputs, runner and TAP identities,
including `gimpdisplayshell.c` and `.h` in the 12-input source seal. After receipt
integration, the parent owns the separate strict Clone gate. No ledger row or
strict acceptance receipt is changed by this checkpoint.

### Evidence distinction and retained limits

The analytical expectations above are pinned-old-**source-derived geometry**.
They are separate from the existing 5,184-row `legacy-navigation/rotation.tsv`
old-function execution oracle and the retained old GUI captures. The arithmetic
suite must still consume that unchanged oracle. The existing GUI traces cover
359.9 degrees, not this epsilon crossing; both old captures retain teardown
`command_exit=-11`. No old runtime, capture or oracle regeneration is permitted
or claimed. Physical mouse/tablet, interruption/focus/device transitions, other
platforms, whole-display/overlay equivalence, all task 25 and full-port acceptance
remain outside this bounded component checkpoint.


### Accepted exact-source native component receipt (2026-10-17)

`migration/tests/rotation-boundary-component-receipt.json` is the publication
summary. `migration/tests/rotation-boundary-component-evidence.tar.xz` preserves
22 unchanged original public metadata/log members, including the native receipt,
plus one compact packaging manifest, for **component-run-001**:

- Tested source A: `aa175058ab6e876fdbc12333e4bf53a48dec6f18`
- Tested tree: `5ec543192cfd943fe5c6d60f1d7153040cdda0b0`
- Native receipt SHA-256:
  `b38ff6991e414f46fa80654e4d086f5d7e68b45828bbfb6caab3abb27a0a8f1b`
- External runner SHA-256:
  `20facd6135b481f1e6d61173c295620805950f0ba2e9c4705b2c6f869476234a`
- Pinned gimp-data: `a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`
- First attempt: **28 normal (18 event + 4 UI + 6 arithmetic)** and **18 focused
  event ASan/UBSan**, with no skipped/failing TAP and four command exits of 0

The default build, focused sanitizer builder, three-target normal Meson command
and focused event executable all passed. Both original public normal and
sanitizer logs contain all **24 ordered crossing observations and 20 signed
steps**, plus the separate persistence and policy-isolation cases. The preceding
12 cases were rerun unchanged. These are synthetic native display-event results;
they are not a new GUI smoke or physical-device capture.

All nine native integrity checks passed: four equal clean source snapshots,
three equal 2,873-entry normal ELF snapshots, focused-builder preservation of
normal ELFs, full build freshness, compile database, registered tests, focused
artifacts/copies and all 361 locale catalogs. All 28 receipt-listed file hashes
were verified. All **29 original public files**, including the original receipt,
were byte-compared against the complete archive. The compact archive preserves
22 originals byte-for-byte and binds seven omitted sanitizer executable/object/
thin-archive copies by their exact hashes and sizes. Deterministic repacking
produced identical bytes.

The full public archive is `component-run-001-passed-public.tar.xz`,
**21,743,868 bytes**, SHA-256
`3791784ed75d4d893005b11390951999c9264681cacb78d4eef762076c73cdd3`.
It is retained separately and bound in both the compact manifest and publication
summary. The compact archive does not contain the seven copied binary artifacts
and is not a standalone rebuild package. No production source, executable test,
fixture, arithmetic oracle or committed procedure changed in this publication.

The parent's separate native audit reproduced all nine public privacy transforms
and verified reuse of the actual registered Meson event environment with only
the two exact ASan/UBSan option overrides. The focused instrumentation remains
exactly five C units; `gimpdisplayshell.c` shell-empty reset is exercised normally
but remains uninstrumented. LSan stays disabled; there are no suppression files
or suppression options. Ordinary icon, plug-in, D-Bus and GLib debug messages
remain in the logs; empty diagnostic-classifier arrays do not mean warning-free
execution. The incremental default build log retains 300 compiler-warning lines;
the focused sanitizer build log contains no `warning:` lines. Neither is a clean-
from-empty or warning-free application claim. Only completed public output is
packaged, excluding private profiles, raw environments and authentication material.

### Receipt-only descendant and pending acceptance

This run attests only to source A. The three-file receipt-integration descendant B
has no new native or GUI run from this component receipt. The existing strict
Clone receipt is deliberately unchanged and must fail source validation until
the parent reruns the unchanged **226 normal / 62 focused sanitizer** gate on the
receipt-integrated source and refreshes its acceptance. Original **25.007 remains
open** until that parent review and ledger decision. This compact component
archive is not valid input to `check_clone_acceptance.py --current-evidence`.

The existing portrait GUI receipt stays separately bound to historical source
`22708f41d368c6c7c2e425d30ea2765b9d29158b`; no new GUI evidence or acceptance is
inferred for A or B. Both retained old captures keep teardown `command_exit=-11`,
and old-runtime skipped/unverified status remains unchanged. Physical input,
focus/device interruptions, other platforms, whole-display/overlay equivalence,
all task 25, full-port, historical full-aggregate and release acceptance remain
outside this evidence-only integration.

## 25.008/mirrored-arrow-input: regression source and component evidence

This test-only increment preserves the source-defined asymmetric key route.
The retained full old handler, pinned above with SHA-256
`7b81d99ab288247d1452ee47053731a4c71eb8c40fa54c734f87023d31757c84`,
calls wants-all tools before translation (1363–1389), swaps only normal
Left/Right presses under horizontal reflection (1378–1389), and forwards those
presses to the tool (1391–1422). Releases use only the unchanged wants-all
callback (1493–1542); ordinary arrow releases have no tool callback. Production
already implements this behavior and is unchanged. The modern local key-event
copy additionally preserves the original event shared with other handlers;
the retained old route instead mutated that input in place.

### Finite independent expectations

`test-painter-navigation.c` explicitly checks all **20 helper combinations**:
five keys (Left, Right, Up, Down and `a`), both horizontal-reflection values and
press/release. Only the five previously missing unreflected-release assertions
are added to that existing six-group target.

Four new `/navigation-events/keys/` registrations dispatch the same matrix
through the public canvas handler: `idle-zero`, `idle-rotated`, `rotation-free`
and `rotation-snapped`. The existing 18 registrations and assertions remain.
Each new group has both reflection values, three recorder modes, Left and
Right, and separate press/release events: **24 events per group, 96 total**.
Vertical reflection is always false.

For each direction, the fixed callback sequences are:

| Recorder mode | Unreflected press | Reflected press | Release in either reflection | Handler press / release |
| --- | --- | --- | --- | --- |
| Normal | original key | opposite key | no callback | true / false |
| All-consume | original key | original key | original key | true / true |
| All-fallthrough | original key, original key | original key, opposite key | original key | true / false |

The recorder is a registered, manager-owned, focused tool with inactive control.
Its registration is idempotent so each new group can also run on its own.
All-fallthrough declines the first press and consumes the second, avoiding
configurable keyboard-controller actions. It declines release. The tests check
callback count, order, phase, tool/display identity, keyval and every public
`GdkEventKey` field. The allocated incoming event's fields, string content and
device/source identities must remain unchanged. Callback inspection never
queries allocation-private GDK metadata through the translated stack copy.
Each group requires exactly **16 press callbacks and 8 release callbacks**,
with no primary-tool button callback.

Idle contexts apply the reflection before selecting angle **0 or 37 degrees**.
Active contexts start at modern 37 degrees with public Shift+middle (free) or
Shift+Ctrl+middle (snapped), then move from the right ray to the down ray. Old
source lines 1827–1869 give start anchors **127 unreflected / 593 reflected**:
old angles 37/323 plus bearings 90/270. The down ray independently gives modern
**127 free / 120 snapped** after the established mirrored-angle conversion.
Key-event state retains button 2 and the applicable Shift/Ctrl bits.

Each key must preserve the navigation action, initiating button, inherited
routing, start and last scroll points, fixed rotation and zoom anchors, angle,
exact-angle provenance, scales, reflection and both shell/native grab state.
After all keys for each reflection, continued public motion to the left ray
must yield modern **217 free / 210 snapped**, from old angles 217/143 or
210/150. The original anchor still applies; middle release then requires the
existing full idle/grab cleanup. The two active groups thus contain four real
navigation drag lifecycles. No expected angle or callback key is obtained from
the production navigation helper.

### Historical source-only checkpoint

At the initial source checkpoint, this was **not a native test receipt or task
closure**. No build or executable target had been run for that checkpoint; the
parent still owned the default build, all **22 event + 4 UI + 6 arithmetic =
32 normal cases**, the full **22 focused event ASan/UBSan cases**, and required
repository/strict acceptance checks. The exact-source component run below now
supplies the 32/22 result, while strict acceptance remains pending for this
increment. The focused builder instruments five translation units, not all
GIMP or dependency code; LSan remains disabled. No ledger, acceptance receipt,
fixture or old-runtime oracle is changed by this evidence integration.

The expected routing and angles are pinned-old-source-derived. There is no
retained Left/Right runtime capture for this matrix, and old runtime remains
user-approved **SKIPPED/unverified**. This does not establish physical keyboard
or tablet behavior, every tool's arrow semantics, keyboard-controller fallback,
focus/overlay/device transitions, vertical-reflection legacy parity,
whole-display/overlay parity, transformed pan/zoom, original calculation-row
closure, all of section 25 or full-port acceptance.


### Accepted exact-source mirrored-arrow component receipt (2026-10-17)

The first attempt passed **32 normal (22 event + 4 UI + 6 arithmetic) cases and
22 focused event ASan/UBSan cases**. This is current component evidence on exact
source A, not task closure. The publication summary is
`migration/tests/mirrored-arrow-component-receipt.json`; the deterministic
`migration/tests/mirrored-arrow-component-evidence.tar.xz` retains 22 unchanged
original public metadata/log members plus one compact packaging manifest.

- Tested source A: `e5f73ed431f1852fdfb337fc402c5a72d16cd6d7`
- Tested tree: `33772929b55d176841d3dbd1a8a5c286ecc445a7`
- Native receipt SHA-256:
  `c37c42e0a2920624ddfa08751a0afa06b8ceb6b1c5377c1562a49164836e2cea`
- External runner SHA-256:
  `353eb47afb45890eb64f55a0baae8a129588686490bc1cad2238529bf8eac5bf`
- Pinned gimp-data: `a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`
- `component-run-001`: 2026-10-17 11:26:49–11:28:29 UTC

The default build, focused sanitizer builder, three-target normal Meson command
and focused event executable all exited 0. Both public execution logs retain
all four new key registrations: **96 unique key sends, 64 press callbacks and
32 release callbacks per execution**, with 24 sends and 16/8 callbacks in each
new group. The preceding 18 event groups passed again, including the separate
24 boundary-crossing observations and 20 signed steps. The six-group arithmetic
target includes all 20 helper combinations. No TAP was skipped or failed.
These are synthetic native display-event results, with source-derived old key
semantics and angle expectations as described above. They are not observations
from a newly executed old runtime, a GUI smoke or physical keyboard capture.

All nine native integrity seals passed: four equal clean source snapshots,
three equal 2,873-entry normal ELF snapshots, focused-builder preservation of
normal ELFs, full build freshness, compile database, registered tests, focused
artifacts/copies and all 361 locale catalogs. All 28 receipt-listed hashes were
verified. All **29 original public files**, including the receipt, were compared
byte-for-byte against the completed full archive. The compact archive preserves
22 originals unchanged and binds the seven omitted sanitizer executable/object/
thin-archive copies by exact hash and size. Deterministic repacking reproduced
identical bytes. No production source, executable test, fixture, arithmetic
oracle or committed procedure changes in this three-file integration.

The full public archive is `component-run-001-passed-public.tar.xz`,
**21,777,852 bytes**, SHA-256
`35dac069ca3b0fd80edb8458a576ec262c69e9647d76895a914710febfc7b283`.
It is retained separately and bound in the compact manifest and summary. The
compact archive omits those seven binary artifacts and is not a standalone
rebuild package. Only completed public output is packaged; private profiles,
raw environments and authentication material are excluded.

The parent's separate native audit reproduced all nine public privacy
transformations and verified reuse of the actual registered Meson event
environment with only the two exact ASan/UBSan option overrides. Instrumentation
remains exactly five C units; other GIMP, GTK and dependency units remain
uninstrumented, LSan stays disabled, and no suppression files/options are used.
The incremental default build and focused sanitizer build logs have no compiler
`warning:` lines. Ordinary icon, plug-in, D-Bus and GLib debug diagnostics remain
in runtime logs. Empty diagnostic-classifier arrays do not mean warning-free
application execution, and this is not a clean-from-empty build claim.

### Mirrored-arrow receipt-only descendant and pending acceptance

This component run attests only to A. The three-file receipt-integration
descendant B has no native or GUI run from that receipt. The unchanged strict
Clone acceptance must reject its changed source seal until the parent reruns
the unchanged **226 normal / 62 focused sanitizer** gate on B and refreshes
acceptance. A final documentary acceptance/ledger step remains separate from
both tested A and the eventual strict-tested B; neither run may be relabeled
as a run on that final descendant. This compact component archive is not valid
input to `check_clone_acceptance.py --current-evidence`.

**25.008/mirrored-arrow-input remains TODO**. The already accepted original
25.008 horizontal-reflection calculation row is unchanged, as are all other
ledger and acceptance files. The historical portrait GUI receipt remains bound
to `22708f41d368c6c7c2e425d30ea2765b9d29158b`; it supplies no new GUI evidence
for A or B. Both retained old captures keep teardown `command_exit=-11`, and
old-runtime **SKIPPED/unverified** status remains. The physical-input,
every-tool, controller-fallback, focus/device, vertical-reflection, pan/zoom,
whole-display/overlay, other-platform and broader acceptance limits above
remain, including all section 25, full-port, full-aggregate and release gates.

## 25.009 common-angle draw path: historical source-only checkpoint

The initial **source-only, not-yet-run checkpoint** was based on public
`05009fa529970b491b960ef4ee91d758f9202b38`, tree
`47e120b9d73d62fb0942ec444711ae61733efd37`. It adds five native registrations to
`app/tests/test-painter-navigation-events.c`, preserving the preceding 22 event
registrations and their assertions. Planned totals are **27 event cases and 37
normal navigation cases** (27 event + 4 UI + 6 arithmetic), not claimed results.
No production implementation or legacy fixture changes are part of this source
checkpoint. Original **25.009 and 25.009/drag-zoom-scroll remain open**; the
196 jointly assigned historical hunks and full section 25 are not accepted.

### Genuine draw boundary and finite matrix

The existing real-display, real-Paintbrush, first-focus-initialized fixture now
contains a 256x256 opaque gray image with three non-collinear colored rectangles.
Every observation calls `gtk_widget_draw(shell->canvas, cr)` with a fresh,
identity Cairo image surface after ordinary frame-clock processing. The test
never applies the production rotation matrix to that context and never draws
the raster or image-space items itself. The normal GTK draw signal reaches
`gimp_display_shell_canvas_draw`, the production image renderer/cache, the
rotated canvas group, the unrotated group, and the separate selection path.
Test-only items record the actual parent CTMs; real `GimpCanvasLine` and
`GimpCanvasRectangle` objects supply the measured image-space overlay pixels.
Observer counts are reset immediately before GTK drawing and must each become
exactly one, so an earlier window paint cannot satisfy an offscreen check.

| Registration under `/navigation-events/draw/` | Frozen observations | GTK snapshots |
| --- | --- | --- |
| `common-angle` | Two horizontal-reflection values, each at zero, free 127, Ctrl-snap 120, Ctrl-release plus stationary-motion unsnap 127, button release 127, unchanged second redraw 127. Each observation captures raster-only then real items visible. | 12 pairs = 24 |
| `fixed-view` | Pre-established horizontally mirrored 37-degree view at 1.5x zoom with offsets `(192 - floor(viewport_width/2) + 13, 192 - floor(viewport_height/2) - 9)`, then the same public right-to-down navigation drag reaches 127 degrees. Center preservation independently requires final offsets `(192 - floor(viewport_width/2) + 9, 192 - floor(viewport_height/2) + 13)`. Both exact scales and initial/final offsets are asserted. This is setup, not drag-zoom or pan acceptance. | 1 pair = 2 |
| `tiny-ctm` | Three unmirrored public inherited motions on `(cx+100000,cy+k)`, k=-1,0,+1, using the accepted boundary fixture's geometry. Each has raster-only and items-visible draws. | 3 pairs = 6 |
| `selection-boundary` | One mirrored 127-degree rectangular selection `(48,54,120,96)`, initially hidden, then visible and a second visible redraw, with marching-ants phase paused. No representative canvas items are visible. | 3 |
| `widget-anchor` | One image-anchored 19x11 GTK event-box container at `(184,92)`, north-west anchor, zero spacing, centered unit-scale view: zero, 37 degrees, unchanged second redraw. Both scroll offsets must remain unchanged. | 3 |
| **Total** | Five added registrations, 16 raster/item pairs and six separate-branch snapshots | **38** |

The common-angle event sequence starts at 37 degrees with Shift+middle on the
right ray, moves to the down ray, presses Ctrl and repeats stationary motion,
releases Ctrl and repeats stationary motion, then releases the initiating
button. The accepted 25.008 source-derived constants are 127/120/127 degrees
under both horizontal-reflection values. The input uses the public canvas tool
event handler; production navigation helpers never calculate expected angles.
Actual grabs are checked while held and ordinary idle cleanup after release.
The image's exact RGBA checksum must remain unchanged through each case.

### Independent geometry and tolerances frozen before native execution

The image markers are red `(61,71,14,10)`, green `(179,83,10,18)`, and blue
`(99,178,18,12)`. Their centers are `(68,76)`, `(184,92)`, `(108,184)`; these
non-collinear centers establish two independent directions. The real overlay
has lines from the first center to each other center and an unfilled rectangle
`(99,178,18,12)`, giving six checked segments. The four separate selection
edges are independently specified by the selection rectangle above.

Expected positions use literal sine/cosine constants and Euclidean geometry:
scale image coordinates, subtract the observed scroll offsets, reflect about
the scaled image center if requested, rotate, and translate back. The center
is `(-offset_x + ((int)(256*scale_x))/2, -offset_y + ((int)(256*scale_y))/2)`;
integer division is deliberate, matching the defined scaled-image coordinate
center. The test does not read `shell->rotate_transform` or call a production
transform helper to calculate expected positions. Observed scale/offset values
are view inputs, not an orientation oracle; the fixed-view case also asserts
the requested exact values.

| Quantity | Frozen expectation / tolerance |
| --- | --- |
| Free angle | 127 degrees; cosine -0.6018150231520483, sine 0.7986355100472928 |
| Snap angle | 120 degrees; cosine -0.5, sine 0.8660254037844386 |
| Widget angle | 37 degrees; cosine 0.7986355100472928, sine 0.6018150231520483 |
| Tiny negative / zero / positive | 359.9994270422049 / 0 / 0.0005729577951117246 degrees; cosine 0.99999999995 / 1 / 0.99999999995; sine -0.0000099999999995 / 0 / 0.0000099999999995 |
| Displayed angle and both CTM basis vectors | Absolute error at most 1e-10; includes both mirror signs, distinguishable tiny nonzero terms and exact zero control |
| CTM translation | Absolute error at most 1e-7 display pixels against independent origin geometry |
| Raster marker classification | Dominant RGB channel >130, both others <110; source dominant=232, others=24, background RGB=128 |
| Marker centroid | Each x/y within 1 display pixel of independent transformed center; 45–150% of scaled source marker area must be classified |
| Stale colored pixels | Every classified marker pixel anywhere in the canvas lies within half its scaled diagonal plus 2 pixels of the expected center |
| Real item and selection pixel difference | Any RGB difference >16 counts; more than 40 pixels must differ, every changed pixel must be within 3 pixels of one expected segment, and all quarter/mid/three-quarter samples of each segment must have a differing pixel within 3 pixels |
| Unrotated control | Identity basis within 1e-10, zero translation within 1e-7; solid magenta interior at display pixel `(28,33)` remains exact |
| GTK widget anchor | Stored x/y within 1e-7 of independently transformed anchor; displayed child matrix translation within 1 pixel for integer placement; unchanged size, zero child angle and identity basis within 1e-10 |

The 3-pixel band is fixed for half-pixel item rounding, double stroke,
selection closing-edge adjustment and antialiasing. It is separate from the
strict CTM tolerance. Raster-only and items-visible captures occur at the same
view. Navigation itself must invalidate the raster cache: the test does not
force cache invalidation between the free/snap/unsnap stages. Global marker
checks reject stale cached raster orientation, and overlay differences reject
painting in an old orientation. These full-surface snapshots and repeated
redraws do **not** establish screen damage/clipping/extents correctness.

### Overlay taxonomy and anchor risk

Image-space canvas items follow the image rotation/reflection. The test's
magenta screen-space control lives in the actual unrotated item group and must
remain stationary. Selection drawing is checked separately because its
boundary-mask route bypasses the rotated item group; this finite rectangle
does not claim every inner/outer segment topology. Image-anchored GTK widgets
stay upright while their anchor follows the image transform.

The GTK child test explicitly exercises the source-derived rotate-refresh risk:
at that checkpoint, `rotated()` rebuilt the transform without moving those children,
and `scroll_set_offset()` returns early when offsets are unchanged. The test
uses only ordinary rotation and GTK processing, with no explicit child move,
scrolled signal, forced size allocation, or test-side repair. Immediate stored
position is logged, but only the ordinary post-draw position is required: a
legitimate deferred GTK refresh is allowed. It checks
`GimpOverlayChild`'s stored position and actual embedding matrix; local widget
allocation x/y are not screen coordinates. This was a candidate regression,
**not an observed native failure** at the initial source-only checkpoint. The
subsequent exact-source failure and bounded repair are recorded below.

### Execution, evidence and remaining boundaries

Set `GIMP_TEST_NAVIGATION_DRAW_DIR` to a fresh external evidence directory for
PNG capture. Names distinguish mirror/state and raster-only vs composite.
PNG output is written before geometry assertions so failing snapshots remain
available; matrix, marker and changed-pixel diagnostics appear in the test log.
Use the shared build lock, authenticated display and isolated writable native
profile described by the build/test coordination and profile contracts.

At that checkpoint the focused event ASan/UBSan builder instrumented **five C
translation units only**: navigation arithmetic, modifier manager, rotate,
tool-events, and this event fixture. The newly exercised callbacks, image
renderer, canvas group/item/line/rectangle, selection, shell and GTK overlay
container/child are **uninstrumented**, as are the rest of core, GTK and other
dependencies. Running the new registrations with that executable is focused
instrumentation coverage, not full render-path or full-application sanitizing.

Parent-owned gates remain: exact frozen-source default build, the complete
registered navigation suite, focused event sanitizer run, evidence integration,
strict Clone acceptance on its separately identified source and an actual
branch portrait Xvfb GUI smoke. No final-revision run claim is made here. The
prior 32-normal/22-event and strict 226-test/62-row receipts remain historical
exact-A/B evidence and are not transferred to this new source. Physical input,
clipping/extents/hit-testing, other platforms, drag zoom/pan, all 196 historical
hunks, whole section 25, release acceptance and the user-approved skipped old
Edge/string-reader runtime stay outside this checkpoint.

### Retained exact-source widget-anchor failure (2026-10-17)

External component run `component-run-001` tested public commit
`bfbbbeade5ba733320b4c1b98a7f3213291928d2`, tree
`5999c39edb3658effe88ee5e8a3c17cc77607d28`, with the unchanged event fixture
SHA-256 `b5ff043ae3518784ebb485a1634cb38806fcdf1ab8b2f8ccf3d64b68255e6568`.
Evidence remains under the external `gimp-canvas-overlay-validation` directory;
it is retained failed-run provenance, not an accepted component receipt.

The default build and five-unit focused ASan/UBSan builder both completed.
Normal execution passed **36 of 37** expected cases: 26 of 27 event cases,
all 4 UI cases and all 6 arithmetic cases. Focused event execution passed the
same **26 of 27** cases. Both event executables then aborted on the explicit
`/navigation-events/draw/widget-anchor` assertion. The actual raster/item,
fixed-view, tiny-angle and selection cases preceded and passed that assertion;
their success does not override the incomplete event suite.

At 37 degrees, with unchanged offsets `(-296,-97)`, the independently expected
anchor was `(490.388929396,229.950762935)`. Immediate, post-draw stored and
visible positions remained `(480,189)`, the zero-angle position, with child
angle zero. This is an observed stale image-anchored GTK child, not a reported
ASan/UBSan memory error. The fixture's second rotated redraw was not reached.
All nine recorded source/build/artifact integrity checks were true.

Retained SHA-256 identities (paths relative to `component-run-001`):

| Evidence | SHA-256 |
| --- | --- |
| `receipt.json` | `2c10891579b17f61e39a89a64556f80e6829762d677e328938d6c8e45154eaa5` |
| `normal-gate.json` | `2a3a5a7b00d160dffffcc3725c2d86c4a42a12a2d32bcf42538ee7254fd31c89` |
| `testlog.txt` | `192e66bcca4a988d79712cf69f82f0a5a37e7e7695ca08d868cd423f45479abb` |
| `sanitizer-run.json` | `afc434f15db6909e7846dc377691f4552a26d24f0eb4c2b238f75597f3ec486b` |
| `sanitizer-run.log` | `16dcc163b328e773b226f50668713a3eb284a98f020a44a2aca6d72174eda516` |

The full 29-file public failure archive is retained separately: **21,887,824
bytes**, SHA-256
`51c4d9bb7170b4d0d332c5335534337884d410e627efece6d73ba4413ad8ecd2`.
The parent's separate audit verified all 28 receipt-listed artifact hashes,
nine raw-to-public transformations, four source snapshots and three normal
ELF snapshots, plus reuse of the actual registered Meson event environment
with only the two exact sanitizer-option overrides. Private profiles, raw
environments and authentication material are not included in the public archive.

### Source-only rotation-anchor repair checkpoint

The bounded production repair makes `gimp_display_shell_rotated()` refresh
every image-anchored child after updating the shell transform and before
emitting `ROTATED`, using the same existing transformed-anchor helper and
position setter as `scaled()` and `scrolled()`. It does not rotate the child
widget itself: the existing upright child basis and angle are preserved.
No navigation arithmetic, GTK renderer, signal consumer, fixture, expected
value, tolerance or test registration is changed.

The focused event sanitizer builder now includes `app/display/gimpdisplayshell.c`
alongside the existing five units, for **exactly six C translation units**.
The other callbacks, image renderer, canvas group/item/line/rectangle,
selection, GTK overlay container/child, remaining core, GTK and dependencies
remain uninstrumented. LSan remains disabled. Adding the shell unit is a
source configuration change, not a new sanitizer execution result.

At the source-only repair checkpoint there was **no build, native, sanitizer
or GUI run**. The unchanged 27-event/37-normal suite and six-unit focused
sanitizer execution were still required on the exact repaired source; their
subsequent passed component result is recorded below. Evidence integration,
the strict Clone gate on its separately identified source and branch portrait
Xvfb GUI smoke remain separate. No ledger, acceptance or parent-row closure
is part of this checkpoint; original 25.009 and 25.009/drag-zoom-scroll remain
open, and old Edge/string-reader runtime stays **SKIPPED/unverified**.

### Accepted exact-source canvas/overlay component receipt (2026-10-17)

The repaired exact source A passed **37 normal cases (27 event + 4 UI + 6
arithmetic) and all 27 focused event ASan/UBSan cases**. This is bounded
component evidence, not original 25.009 closure. The publication summary is
`migration/tests/canvas-overlay-component-receipt.json`; the deterministic
`migration/tests/canvas-overlay-component-evidence.tar.xz` retains 22 unchanged
original public metadata/log members plus one compact packaging manifest.

- Tested source A: `ad0dab2d49c2bafcd85da05d76f077071cd1c6e1`
- Tested tree: `a237803fe42b33f6c608fb993e45d838c4ecd88b`
- Native receipt SHA-256:
  `6be80fb0b6e1f87a5b239d1093455b3459f964ab6dbed24ffcda7fa8973a5404`
- External runner SHA-256:
  `27a46140530a6fe1f2f0cf8dc0b91afa77adfc0c25406a63d2e996f4da60d4e2`
- Pinned gimp-data: `a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`
- `component-run-002`: 2026-10-17 12:14:17–12:16:06 UTC

The default build, six-unit focused sanitizer builder, three-target normal
Meson command and focused event executable all exited 0. The exact event
fixture SHA-256 remains
`b5ff043ae3518784ebb485a1634cb38806fcdf1ab8b2f8ccf3d64b68255e6568`,
identical to the retained failed `component-run-001` on `bfbbbeade5ba733320b4c1b98a7f3213291928d2`.
No assertion, expected angle, geometry, pixel tolerance or test registration
was weakened. The previous failure is preserved, not overwritten by this pass.

All five new draw registrations passed in both executions, including **38
actual GTK snapshots per execution**: 16 raster/item pairs, three separate
selection draws and three upright-widget draws. Logs retain 34 draw CTMs,
51 marker classifications, 16 six-segment item differences, two four-segment
selection differences and three anchor observations per execution. At 37
degrees, immediate and both post-draw stored anchor positions now equal
`(490.388929396,229.950762935)`, with visible integer placement `(490,229)`,
unchanged offsets `(-296,-97)` and child angle zero. The unchanged second
rotated redraw passes. These observations close the bounded stale-anchor
regression demonstrated above; they do not establish damage or clipping.

The preceding 22 event cases also pass, retaining 96 mirrored-key sends,
64 press/32 release callbacks and 24 boundary-crossing observations/20 signed
steps per execution. No TAP case was skipped or failed. The PNG capture hook
was **not enabled** by this runner: real GTK draws and pixel assertions ran,
but this archive contains execution logs rather than saved PNGs. This is not
a branch portrait GUI smoke or physical-input capture.

All nine integrity seals passed: four equal clean exact-A source snapshots,
three equal 2,873-entry normal ELF snapshots, focused-builder preservation of
normal ELFs, full build freshness, compile database, registered tests, focused
artifacts/copies and all 361 locale catalogs. All **29 receipt-listed hashes**
were verified, and all **30 original public files**, including the exact
unchanged receipt, were compared byte-for-byte against the full archive. The
compact archive preserves 22 originals unchanged, including the complete
original nested file-hash inventory. It binds the eight omitted sanitizer
executable/object/thin-archive copies by exact hash and size; those binaries
are not present in the compact archive. Deterministic repacking reproduced
identical bytes.

The full public archive is `component-run-002-passed-public.tar.xz`,
**22,053,024 bytes**, SHA-256
`3cead13c93a73fdf56d579bde4d700a325c7dce033f42d3861c20a8427bbeae0`.
It is retained separately and bound in the compact manifest and summary.
The compact archive is not a standalone rebuild package. Only completed
public output is packaged; private profiles, raw environments and
authentication material are excluded.

The parent's separate native audit reproduced all nine public privacy
transformations and verified reuse of the actual registered Meson event
environment with only the two exact ASan/UBSan option overrides. Exactly six
C units are instrumented, including the changed display shell. The other
callbacks, renderer, canvas group/item/line/rectangle, selection, GTK overlay
container/child, remaining core, GTK and dependencies are uninstrumented.
LSan stays disabled and no suppression files/options are used. After ANSI
color stripping, the incremental default build has no compiler `warning:`
lines. The focused sanitizer build retains one `-Wmissing-declarations`
warning for `navigation_draw_observer_get_type` at
`app/tests/test-painter-navigation-events.c:1488:40`; no source was changed
to hide or remove it. Ordinary icon, plug-in, D-Bus and GLib debug diagnostics
remain in runtime logs; empty classifier arrays are not a warning-free
application claim. This is not a clean-from-empty build claim.

### Canvas/overlay receipt-only descendant and pending acceptance

This receipt attests only to tested source A. The three-file evidence-only
descendant B has no native or GUI run from that receipt. No production source,
test, fixture, builder, verifier, ledger or acceptance file changes in this
integration. The unchanged strict Clone checker must reject the changed
source seal until the parent reruns the unchanged **226 normal / 62 focused
sanitizer** gate on B and refreshes acceptance. The final documentary
acceptance step is separate from both tested A and the eventual strict-tested
B; neither run may be relabeled as a run on that final descendant. This
compact component archive is not valid input to
`check_clone_acceptance.py --current-evidence`.

**Original 25.009 and 25.009/drag-zoom-scroll remain open.** The 196 jointly
assigned historical hunks, whole display/overlay behavior, section 25 and
full-port, aggregate, platform and release gates are not closed. Damage,
clipping, extents, hit-testing, every overlay/selection topology, vertical
reflection legacy parity, physical input and transformed drag-zoom/pan/scroll
remain outside the bounded matrix. The historical GUI receipt remains bound
to `22708f41d368c6c7c2e425d30ea2765b9d29158b`; current branch portrait GUI
validation is pending. Old Edge/string-reader runtime stays
**SKIPPED/unverified**; both retained old captures keep teardown
`command_exit=-11`. No old runtime, capture or oracle was run or regenerated.
