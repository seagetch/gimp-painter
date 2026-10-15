# Independent legacy histogram toolbars

## Scope and source boundary

This is a bounded part of `29.010/standard-tool-options`, not completion of that
row or of the canvas UI. The legacy reference is
`afa43fae3e920210146abed514f136fd49f671b5`. Three current registered tools retain
this legacy factory family: `gimp-threshold-tool`, `gimp-levels-tool`, and
`gimp-curves-tool`.

The recovery document's original 34 supported / 16 missing inventory is a
historical checkpoint. Before this change, current `gimp-tools.c` already
registered independent Align and Paths factories; neither was still a missing
factory. This change supplies the genuinely absent histogram family. Use the
new runtime inventory and its tested source hashes for current counts; do not
reuse that historical list as a current acceptance result.

## Exact legacy-to-current correspondence

All source line references in this table are pinned to the legacy revision.
Hunk IDs refer to `migration/inventory/changed-hunks.tsv`; no inventory row is
marked implemented merely because it was enumerated.

| Legacy source / hunk | Legacy behavior | Current implementation |
| --- | --- | --- |
| `app/tools/gimpthresholdtool.c:82–96`, `01.002/001474` | Registers `gimp_histogram_options_gui_horizontal` | Existing horizontal dispatch reaches `gimp_painter_tool_toolbar_new`; `GimpHistogramOptions` is admitted and Threshold gets scale controls only |
| `app/tools/gimplevelstool.c:142–156`, `01.002/001309`; `gimpcurvestool.c:137–151`, `01.002/001230` | Registers `gimp_color_options_gui_horizontal` | The same independent builder supplies scale plus average controls for Levels and Curves |
| `app/tools/gimphistogramoptions.c:119–145`, `01.002/001272`–`001275` | Separate horizontal factory; a horizontal two-value `histogram-scale` radio frame, titled Histogram Scale | `painter_histogram` creates a fresh horizontal property-bound frame in a fresh toolbar; values remain Linear histogram / Logarithmic histogram |
| `app/tools/gimphistogramoptions.c:147–156` | Connect scale model to histogram view and notify it | Current Threshold/Levels/Curves native dialogs already have bidirectional `histogram-scale` bindings; the new toolbar uses that same canonical options object |
| `app/tools/gimpcoloroptions.c:140–155`, `01.002/001200`–`001204` | Color options delegate to histogram options for histogram-derived tools | `painter_histogram` creates scale first for the three known current histogram tools |
| `app/tools/gimpcoloroptions.c:174–204`, `01.002/001205`–`001207` | Inline Average toggle with Sample Average details popup | Levels/Curves receive an independent `sample-average` check button and top-position GTK3 popover; popup arrow and contents track toggle sensitivity |
| `app/tools/gimpcoloroptions.c:208–224`, `01.002/001207` | Popup radius control, property `average-radius`, increments 1/10, zero decimals | Independent property spin-scale uses the same property, bounds, increments and precision |

The legacy horizontal popup helper is `app/tools/gimptooloptions-gui.c:416–450`: the Average toggle disables the arrow itself. This binding is retained as well as the popup-content sensitivity.

The ordinary dock GUI remains cached and untouched. No dock constructor is
called or dock widget reparented by this builder. Each toolbar's controls and
popover are separate objects; the root retains the canonical options model.
The new code does not replace private native filter-dialog widget pointers.

The legacy Threshold toolbar contains no Average toggle or radius popup.
The Levels/Curves toolbar intentionally follows the old color-options factory;
modern Sample merged remains available in the ordinary dock. Modern threshold
range, channels, level endpoints and curves editing remain in their native
filter dialogs, as distinct controls from this legacy options toolbar. This
slice makes no assertion of pixel-processing algorithm parity or complete
modern filter-control presentation on the toolbar.

GTK3 popovers replace the old popup window helper. This retains independent
controls and toggle/value semantics, not byte-for-byte legacy popup geometry.

## Native acceptance coverage

`app/tests/test-painter-histogram-toolbar.inc` is included and registered by
`test-painter-compact-options.c`:

- `independent-histogram-toolbar`: all three registered factories, two independent
  toolbar instances, retained dock widget identities and all original parents,
  horizontal radio orientation, exact scale notifications, model-to-peer sync,
  Levels/Curves radius and Average edits in both toolbar/dock directions,
  toggle-dependent sensitivity, Threshold's absence of the other two controls,
  destruction of one toolbar while the peer/model/dock remain usable, and
  finalization of both roots.
- `histogram-toolbar-native-dialogs`: initializes the actual native Threshold,
  Levels and Curves tools against a selected layer; verifies bidirectional
  scale synchronization with their real histogram/curve view and unchanged
  parent, then halts/switches tool and verifies dialog finalization while the
  independent toolbar remains usable.
- `histogram-toolbar-canvas-popup-lifetime`: selects Levels and Curves in the
  actual canvas toolbar; reveals an off-viewport button using the native horizontal scrollbar adjustment, verifies it fits within the viewport, opens the real popup, sends Escape, reopens it, and
  changes the active tool while it has a grab. Repeats three times per tool,
  verifying finalization of popup/toolbar and retention of the ordinary dock.
- The existing `independent-horizontal-inventory` test discovers the three new
  factories and also checks two separate trees per registered supported tool.

Final tested-source hashes, retained failed attempts, passing native runs and
actual application screenshot provenance are recorded in
`migration/tests/histogram-toolbars/report.json`. These new cases were not run
with ASan/UBSan instrumentation; no sanitizer result is claimed for this slice.

## Actual-application inspection

The tool action names are verified by
`gimp_tool_info_get_action_name` in `app/core/gimptoolinfo.c` and the dynamic
registration in `app/actions/tools-actions.c`: `tools-threshold`, `tools-levels`,
`tools-curves`. Start with a real image canvas and Painter UI enabled. Threshold
must show the two radio choices in a lower horizontal Histogram Scale frame.
Levels/Curves additionally show Average and the radius-popup arrow. A screenshot
of a welcome screen or the filter dialog alone cannot establish that placement.

## Viewport regression diagnosis and focused result

The initial full-target run reached the third new case after the first two new
cases passed, then failed the strict popup-mapped assertion. The isolated case
reproduced it. Diagnostics showed every button ancestor visible, mapped and
sensitive, but the popup was visible/realized with a grab and still not mapped.
A two-second frame-clock wait also failed. This was not assumed to be timing:
allocation evidence established that the arrow began at x=411 with width=36,
outside a 398-pixel horizontal toolbar viewport (447-pixel contents).

The corrected fixture scrolls the actual native horizontal adjustment. The
focused run records offset 0 → 49 and anchor x=411 → 362, so all 36 pixels fit
inside the 398-pixel viewport. All six openings (three Levels, three Curves)
then record `active=1 visible=1 mapped=1 realized=1 grab=1`. The assertions still
require complete anchor reachability, mapped popup, active grab, Escape
closure, remapping after reopen, popup/control-tree finalization on tool switch,
and no residual GTK grab. No mapping assertion, feature, or factory control
was removed or suppressed, and no production layout was changed to bypass it.

The focused `histogram-toolbar-canvas-popup-lifetime` case passes, exit 0,
3.72 seconds, on the ordinary native build under authenticated loopback-only
Xvfb at 1080×1920×24. Coordinating run artifacts:

- `histogram-popup-reveal.log`, SHA-256
  `3c9b61486a193723a200c05709f84b65f7137ee81ce371351451f54a9e260064`
- `build/meson-logs/histogram-popup-reveal.json`, SHA-256
  `8b22b784fb927aba0246660f27726b903d5541560726c79b4854779c331031d7`
- Prior failed `histogram-toolbar-full`, `histogram-popup-focused`,
  `histogram-popup-diagnostic` and `histogram-popup-frame` logs are retained.

The final full native target passes all 50 registered cases in 106.18 seconds,
exit 0, with zero skips (`histogram-toolbar-final.json`). The final runtime
inventory reports 39 supported factories and 11 remaining unsupported tool IDs;
all are enumerated in `migration/tests/histogram-toolbars/report.json`.
All three new cases also pass at `GDK_SCALE=2` in 4.42 seconds, exit 0, with
zero skips (`histogram-toolbar-scale2.json`). This is synthetic HiDPI coverage,
not physical-tablet or platform coverage.

`migration/tests/histogram-toolbars/portrait.png` is the inspected actual
installed application at 1080×1920. It shows an image canvas, native Apply
Threshold dialog, and the bottom horizontal Histogram Scale bar with both
radio choices, alongside the existing Painter wheel/FG-BG/tool/layer views.
Colors → Threshold activated the verified specialized tool. Earlier unsuccessful
keyboard-search captures showing another tool are excluded from acceptance.
The final application quit normally, exit 0; its launch log contains font
fallback and D-Bus availability diagnostics, but no critical/error. The installed
binary SHA-256 is
`4e4725e69c38049265b59a87ffd14eea5705e79f982d19046b3b71a0e6d80966`.
Screenshot, capture script, logs, application/test binaries and exact matching
checkout/native-source hashes are in the sanitized report. No inherited
process environment is copied into that report.

This completes the bounded histogram-toolbar child acceptance only.
`29.010/standard-tool-options` and the broader UI/session/platform work remain
open. None of these ordinary native results establishes ASan/UBSan coverage
or complete port acceptance.
