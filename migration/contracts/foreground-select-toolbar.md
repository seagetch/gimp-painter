# Independent Foreground Select toolbar

## Bounded scope and status

This is a bounded current-model part of `29.010/standard-tool-options`.
It adds an independent horizontal factory for the already registered
`gimp-foreground-select-tool`, retaining the ordinary dock GUI. It does not
complete that row, the legacy Foreground Select correspondence, or the port.

The legacy reference is `afa43fae3e920210146abed514f136fd49f671b5`.
That implementation uses SIOX, whereas the current `GimpForegroundSelectOptions`
and tool use GEGL matting. Surviving concepts have useful current bindings;
missing SIOX properties and algorithm behavior remain unresolved obligations,
not controls declared obsolete by this toolbar change.

Native compilation succeeds. The final focused run passes 3/3 cases in
4.25 seconds; the full compact-options target passes 53/53 in 105.30 seconds;
the three new cases pass with `GDK_SCALE=2` in 4.29 seconds. All exit 0 with
zero skips. Exact source/log/binary hashes and failed attempts are recorded
below. These are ordinary native results, not sanitizer or full-port evidence.
The final runtime inventory verifies 40 supported independent factories and
10 unsupported IDs. The earlier 39/11 inventory remains historical.

The coordinator separately reports a portrait application/Details capture and
clean exit, but the production popup-direction correction requires matching
final-binary capture provenance before screenshot acceptance. The preceding
fixed-source full-target baseline must not be mixed with evidence for these
edits.

## Exact source and hunk correspondence

All legacy line numbers below refer to the pinned revision above. Hunk IDs are
from `migration/inventory/changed-hunks.tsv`; their enumeration is not proof
that every source behavior has been implemented.

| Legacy source / hunk | Legacy behavior | Current implementation or explicit gap |
| --- | --- | --- |
| `app/tools/gimpforegroundselecttool.c:139–143`, `01.002/001263` | Registers `gimp_foreground_select_options_gui_horizontal` separately from the dock factory | The existing `gimp_tools_horizontal_options_gui` dispatch reaches `gimp_painter_tool_toolbar_new`, which now admits `GimpForegroundSelectOptions`; no dock constructor is called |
| `gimpforegroundselectoptions.c:54–64,261–295,404–414`, `01.002/001256`–`001261`; `.h:53`, `01.002/001262` | Separate horizontal selection control tree with an independent Details view | `painter_selection` and `painter_foreground` create fresh controls, including a top-position GTK3 Details popover, on each factory call |
| `gimpselectionoptions.c:209–265` | Four selection-operation icon choices; Replace first | Existing `painter_selection` supplies fresh canonical `operation` controls with Replace first; there is no new private `mode_box` assignment |
| `gimpselectionoptions.c:267–309`; `gimpforegroundselectoptions.c:279–280` | The live shared helper is an `antialias`-bound toggle/popup containing feather radius, and Foreground disables that whole helper | Current native Foreground hides antialias. The new toolbar likewise keeps its independent antialias control hidden, including after `show_all`. Its separate working Feather toggle/radius is current-model coverage, not exact restoration of the disabled legacy helper |
| `gimpforegroundselectoptions.c:282–285` | Inline `contiguous` toggle | No current option or GEGL matting equivalent is supplied. Legacy contiguous-selection behavior remains open under `29.010/standard-tool-options` |
| `gimpforegroundselectoptions.c:287–292`, `01.002/001260` | Details popup in horizontal mode | Independent `gimp-painter-foreground-details` menu button and GTK3 popover, sharing only the canonical options model |
| `gimpforegroundselectoptions.c:314–325` | Boolean `background`, with Mark background/foreground radios and a modifier-label hint | Current `draw-mode` enum provides Draw foreground/background and additionally Draw unknown; current native labels are used. The inherited `GimpContext::background` property is a `GeglColor`, not the old Foreground Boolean, and must not be rebound as marking mode. The two marking intentions correspond through `draw-mode`, but the model, third mode and label presentation differ |
| `gimpforegroundselectoptions.c:328–357` | Small/Large brush labels and value-hidden scale bound to `stroke-width`, increments 1/5; old range/default is 1..80/18 (`:99–103`) | Independent current-native spin scale uses `stroke-width` 1..6000/default 10, increments 1/10, integer precision, scale limits 1..1000 and gamma 1.7; native-width Reset is a current addition. The old slider geometry/range is not claimed restored |
| `gimpforegroundselectoptions.c:359–369` | `smoothness` scale, range 0..8 | No current property or equivalent algorithm control. Legacy smoothing behavior remains open under `29.010/standard-tool-options` |
| `gimpforegroundselectoptions.c:371–375,416–440` | `mask-color` enum selecting red, green or blue; resulting alpha fixed at 0.5 | Current `mask-color` is a `GeglColor`, edited by an independent native `GimpColorPanel`, including alpha. The canonical current property is shared; this is a model/presentation change, not exact enum equivalence |
| `gimpforegroundselectoptions.c:377–399` | `expanded` Color Sensitivity frame containing `sensitivity-l`, `sensitivity-a`, `sensitivity-b` | Those four properties and their SIOX behavior are absent from the current model. They remain explicitly unresolved under `29.010/standard-tool-options`; engine controls do not substitute for them |

### Current-model additions

The existing ordinary implementation in
`app/tools/gimpforegroundselectoptions.c:gimp_foreground_select_options_gui`
is the source for current matting control behavior:

- Three draw modes, including Draw unknown
- Two preview modes, Color and Grayscale
- Numeric width scale and reset to 10
- Full preview-color editor with alpha and the canonical context
- Engine enum with Levin disabled as a choice when `gegl:matting-levin` is
  unavailable; the existing model coerces unsupported Levin to Global
- Levin `levels` and `active-levels`, and Global `iterations`, each 1..10,
  increments 1/1 and integer precision
- Engine-dependent visibility uses the same enum mapping as the native GUI.
  Independent scale controls additionally use `no-show-all`, so showing a
  toolbar or its popup cannot expose parameters for the wrong engine
- Current Feather toggle/radius, consumed by
  `gimp_foreground_select_tool_commit`, with both popup arrow and contents
  disabled while Feather is off

The view exposes 11 property controls: selection operation, Feather, feather
radius, draw mode, stroke width, preview mode, mask color, engine, levels, active
levels and iterations. Antialias is a twelfth constructed property control kept
hidden to match the current native tool. Width Reset is a separate action, and
Details/Feather have two separate popup launchers. Engine-dependent controls
are counted once each, regardless of their current visibility.

The new view does not add or modify any of these options or processing
algorithms. The ordinary GUI's `mode_box` and `antialias_toggle` remain intact.
The toolbar root retains the options model; native property controls own their
normal bindings. The width-reset callback uses an object-bound signal, and
engine visibility bindings are target-lifetime-bound. Separate toolbar trees
share neither controls nor popovers with each other or the cached dock.

GTK3 popovers replace the old popup-window helper. Exact legacy window geometry,
modifier hints and popup/session persistence are not established by this slice.

## Native acceptance cases

`app/tests/test-painter-foreground-toolbar.inc` is included and registered by
`app/tests/test-painter-compact-options.c`. These are real native GTK/GIMP tests,
not an extracted model harness. The assertions below pass in the final runs
recorded under Final native verification.

### `independent-foreground-toolbar`

- Creates two full toolbar trees and verifies they are disjoint from each other
  and every original dock widget; preserves every original parent and the native
  private `mode_box` / `antialias_toggle` pointers
- Checks horizontal orientation, hidden antialias, independent Details and
  Feather controls
- Edits selection operation, draw mode and preview mode in toolbar and dock
  directions, verifying exact notification counts and all peer radio values;
  also exercises Draw unknown
- Edits width and all engine parameters in both directions, including their
  bounds, exact notifications and width reset
- Checks Feather on/off sensitivity of the arrow and radius, and radius sync
- Edits preview RGB and alpha through toolbar and native dock color controls,
  checking exact notifications and both peer views
- Exercises engine switching repeatedly, conditional on actual Levin
  availability; checks unsupported-engine model fallback without a skipped
  branch, sensitivity and visibility after repeated `show_all` calls
- Verifies strict absence of six old SIOX properties (`contiguous`, `smoothness`,
  `expanded`, `sensitivity-l`, `sensitivity-a`, `sensitivity-b`) and inert
  substitutes. Separately requires the same-named context `background` property
  to be owned by `GimpContext` with `GeglColor` value type, no toolbar binding,
  zero background notifications and unchanged RGBA throughout marking-mode and
  preview-color edits. This records unresolved correspondence, not acceptance
  of the missing algorithm or controls
- Destroys one toolbar, continues editing the model and peer/dock, then verifies
  both toolbar roots finalize and original dock identities/parents remain

### `foreground-toolbar-model-before-view`

- Creates an independent options object and both toolbar views before any native
  dock GUI through the production `gimp_painter_tool_toolbar_new` builder,
  verifying the native private pointers remain null. Explicitly asserts that
  the canvas dispatcher rejects this noncanonical fixture; the independent
  binding case and real canvas case separately exercise canonical dispatch
- Releases the external model reference while the toolbar trees remain live;
  verifies model retention and peer synchronization
- Destroys both trees, verifies model finalization, then emits a click on a
  separately retained destroyed reset button to detect stale callbacks

### `foreground-toolbar-canvas-popup-lifetime`

- Activates the real on-canvas Foreground toolbar three times
- Uses the existing tested scroll-to-reveal helper to establish that the actual
  popup anchor fits within the horizontal viewport
- Requires the Details popup to map with an active grab, handles Escape,
  reopens it, checks the actual top popup position, and switches tools with
  its mapped popup grab still active in the middle cycle
- Opens the native preview-color dialog in the first and last cycles; in the
  first, edits its real color-selection widget and clicks the native Cancel
  and OK response buttons, verifies rollback/commit in both toolbar and dock,
  then reopens it before tool switching with the dialog and Details popup alive
- Requires toolbar, popup and color dialog finalization and no residual grab;
  explicitly opens the registered native Tool Options Dockable and verifies
  its actual dockbook/editor contains the Move GUI before Foreground activation,
  then requires that same Dockable/editor/parent plus
  unchanged native child parents/private pointers through all three cycles;
  asserts native visibility while active and hiding after tool switch

The existing `independent-horizontal-inventory` test will also exercise the
new factory twice and report the actual remaining unsupported IDs.

## First focused-run diagnosis

The first three-case focused invocation compiled successfully but aborted after
1.59 seconds in `independent-foreground-toolbar`, before completing that case.
The assertion treated every old Foreground-specific property name as globally
absent, including `background`. The current class inherits that name from
`GimpContext` (`app/core/gimpcontext.c:703–709`) as a `GeglColor`, whereas the old
Foreground class installed a Boolean marking flag (`gimpforegroundselectoptions.c:92–97`
at the pinned legacy revision). No successful three-case result is implied.

The correction keeps strict NULL assertions for all six actually absent
properties and separately verifies the inherited property's exact owner/type,
absence from both toolbar trees, zero notifications and unchanged RGBA while
marking modes and preview colors are edited. It neither hides a current control
nor restores or claims equivalence for the old Boolean property. The original
failed run is retained as `build/meson-logs/foreground-focused.json` in the
coordinating native run. The first retry passed `independent-foreground-toolbar`, then aborted in the
second case because the fixture called the canvas dispatcher with a newly
created, noncanonical options object. `gimp_tools_create_painter_toolbar_gui`
in `app/widgets/gimpwidgets-utils.c:1855–1857` deliberately rejects an object
that is not its tool-info's `tool_options`. The lifetime fixture now asserts
that rejection explicitly and uses the actual production
`gimp_painter_tool_toolbar_new` builder for its two fresh trees. It does not
replace the canonical model or weaken any root/model/reset-button lifetime
assertion. Canonical dispatch remains covered separately by the first case
and actual canvas test. The retry log is retained as
`build/meson-logs/foreground-focused-retry.json`. The later corrected focused rerun is recorded below.

### Popup-direction regression exposed by the new assertion

The separate popup-only run (`build/meson-logs/foreground-popup.json`) failed
in 2.62 seconds because the requested popover position was `GTK_POS_BOTTOM`
although the shared helper initially called `gtk_popover_set_position(TOP)`.
`gtk_menu_button_set_popover` reapplies the button's default downward direction
when attaching it, overwriting that earlier request. GTK can still display it
above the bottom toolbar due to screen constraints; that visual fallback does
not establish the requested-position contract.

The helper now sets the menu button's direction to `GTK_ARROW_UP` before
attaching its popover. This corrects the intended top placement for the shared
Details helper, rather than weakening the strict `GTK_POS_TOP` assertion or
relying on screen constraints. The full compact-options rerun must cover the
other callers of that helper as well as Foreground Select.

### First native editor attachment in the canvas fixture

After the first two corrected cases passed, the popup case reached its final
first-cycle ownership assertion and failed because the preactivation native
GUI parent was still NULL (`build/meson-logs/foreground-focused-final.json`,
2.77 seconds). The ordinary `GimpToolOptionsEditor` attaches a cached options
GUI on its first activation (`gimptooloptionseditor.c:485–491`); switching away
hides it without removing that parent (`:460–463`). A cached but never-activated
GUI legitimately changes from no parent to the native editor's options box.

The corrected fixture establishes the expected parent and
`GimpToolOptionsEditor` from the already-active Move dock before creating any
Foreground toolbar. It then requires Foreground to have that exact parent and
editor while active and after every switch, preserves all preexisting child
parents and native private pointers, and checks active/hidden visibility. Only
the first root attachment is admitted; a toolbar stealing the native controls
still fails. The next run (`build/meson-logs/foreground-focused-final3.json`, 2.35 seconds)
passed the first two cases but established that this GUI-test profile may have
no Tool Options editor at all: selecting Move does not guarantee one exists.
The fixture now explicitly opens the registered `gimp-tool-options` Dockable
through `gimp_window_strategy_show_dockable_dialog`, verifies its actual
native dockbook and `GimpToolOptionsEditor`, and requires the Move GUI to be
populated there before the Foreground cycle. Each active/switch phase checks
the editor's actual visible options model as well as ancestry and retained
child identities. A newly created fixture Dockable is destroyed during cleanup;
an existing native one is left intact. No parent assertion is waived, and the
later corrected focused rerun is recorded below.

### Runtime-confirmed active-tool sequencing

Explicit diagnostics in `foreground-dock-diagnostic.json` (failed, 2.42 seconds)
showed that both the user context and real native editor held
`gimp-paintbrush-tool`, with the Paintbrush GUI attached and the Move GUI
unparented. This was not a toolbar stealing the Move GUI. The subsequent
phase diagnostics established that `new_shell()` had already restored
Paintbrush before native Dockable construction.

The fixture now initializes the actual shell and native Dockable first, then
selects Move, drains the loop and asserts that the user context and native
editor both hold Move before deriving expected ownership. It retains every
Dockable/editor/parent, descendant-parent, visibility, popup-position/grab,
color rollback/commit and finalization assertion. The corrected three-case
run `build/meson-logs/foreground-dock-sequence.json` passes all three cases,
exit 0, in 4.25 seconds under authenticated 1080×1920×24 loopback-only Xvfb.
No assertion was waived or replaced by merely checking for any parent.

## Final native verification

All runs use the shared build lock and authenticated loopback-only Xvfb at
1080×1920×24 in `/workspace/shared/gimp-native.RoEPfcPS`. The wrapper verifies
authorized-client success and empty-authority rejection. No native source or
build operation ran concurrently with these phases.

| Final run, under `build/meson-logs/` | Result | SHA-256 |
| --- | --- | --- |
| `foreground-dock-sequence.json` | Focused 3/3, exit 0, 4.2495 s, zero skips | `fef3f2ba122de3adb5920f7ff971571d3a6a68181492f0daa6559f3265142f41` |
| `foreground-compact53-final.json` | Full compact 53/53, exit 0, 105.2980 s, zero skips | `1d7e5a64bd9655f37c4ef90b5ac3068d6caed1c2b468ecb6d742b36d1e8a642b` |
| `foreground-scale2-final.json` | Focused 3/3 with verified `GDK_SCALE=2`, exit 0, 4.2859 s, zero skips | `b99911ea9225f1cdf99995b51059bf676dc3d985a370d70c7452d09663341c1a` |

The sanitized `migration/tests/foreground-toolbar/report.json` records all ten
attempt results/TAP summaries, the runtime inventory and exact source hashes.

Test executable: `build/app/tests/painter-compact-options`, SHA-256
`88ec5ecec07071ebd7641dee6a22f16dfa1f7c0d903ee22b37491af95fb309c8`.
The final build phase is logged in `foreground-dock-sequence-build.log`.

The actual native source base is
`4e12f80e30dcbe5e05c17d56d86c3a4f53ae8f47`. It has the published large-XCF
registration change in `app/tests/meson.build` plus the Foreground working
changes; this is not an exact-commit run of a newer development HEAD. The
complete tracked native `git diff --binary HEAD` is 11,441 bytes with SHA-256
`eea635a4dc949dedf8962d305886a32f5d41b718873ae91e36d220f688a56f70`.
The new untracked `test-painter-foreground-toolbar.inc` is additionally pinned
by its file hash below. `app/tests/meson.build` has SHA-256
`d5b3588c94b1363f729d06a855471fbd07bc826585f3a605962522042281bc38`.
The exact three Foreground source files were verified byte-for-byte equal
between the development checkout and native source before completion:

| Source | SHA-256 |
| --- | --- |
| `app/tools/gimppaintertooltoolbar.c` | `acb8e46313eb606588a82a8c0212080a2a8d96e8c616a7daa3c6fee5c56a024e` |
| `app/tests/test-painter-compact-options.c` | `24c8f413eac174fbca871f26da1c73e3d1d56c07499bccf1675ec7d1dbef3343` |
| `app/tests/test-painter-foreground-toolbar.inc` | `17f939cf9f5fdb25395a980e2b7f6c66f9867eb76693ea84892acf916874651c` |

The full target also reruns the histogram popup tests after the shared helper's
UP-direction correction. Its inventory verifies 40 supported independent
horizontal factories and these 10 unsupported tool IDs:

- `gimp-unified-transform-tool`
- `gimp-handle-transform-tool`
- `gimp-perspective-guide-tool`
- `gimp-transform-3d-tool`
- `gimp-warp-tool`
- `gimp-text-tool`
- `gimp-brightness-contrast-tool`
- `gimp-offset-tool`
- `gimp-gegl-tool`
- `gimp-operation-tool`

The initial failed JSON logs remain intact: `foreground-focused`,
`foreground-focused-retry`, `foreground-popup`, `foreground-focused-final`,
`foreground-focused-final3`, `foreground-focused-final4`, and
`foreground-dock-diagnostic`. Their fixture and production diagnoses above
explain the corrections rather than treating aborted runs as passes. Only
whitelisted result fields and the scale setting were inspected for this
contract; no inherited process environment is copied into it.

Scale-2 is synthetic HiDPI coverage. No ASan/UBSan, physical-tablet, complete
application instrumentation, or cross-platform result is claimed for this slice.

## Remaining acceptance and ownership

The native compilation, three focused cases, full compact-options target and
scale-2 checks above pass. Before screenshot acceptance, the coordinator must
relink/install the application containing the popup-direction fix and inspect
an actual portrait capture showing the Foreground toolbar and Details, with
matching final-source and application-binary provenance. Keep those results
separate from the preceding fixed-source baseline and earlier capture. Add
sanitizer coverage only when actually executed with that configuration.

The unresolved SIOX controls and algorithms above remain owned by
`29.010/standard-tool-options` and its broader legacy UI correspondence work.
This child neither deletes them from the ledger nor asserts that current GEGL
matting is algorithmically equivalent. Native image extraction, physical tablet
input, complete legacy geometry, window/session behavior, platform testing and
full UI acceptance remain outside the prepared toolbar-binding tests.

## Final installed portrait capture

The coordinator rebuilt and installed the final application after the shared
UP-direction fix, then opened the real fixture with Foreground Select active
and clicked the actual Details button. `migration/tests/foreground-toolbar/portrait.png`
shows the independent bottom toolbar, mapped Details, whole native Dockables,
wheel and custom layer tiles on the 1080×1920 authenticated TCP screen.
Normal GUI quit returned zero; its launch log contains no CRITICAL, ERROR or
WARNING. `report.json` seals both installed/build binaries and the screenshot
SHA-256, separately from the earlier pre-fix capture. This is bounded toolbar
acceptance, not full legacy SIOX or UI/window/session equivalence.
