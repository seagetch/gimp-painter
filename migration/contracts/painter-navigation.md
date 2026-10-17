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
