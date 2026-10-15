# Legacy UI completion correction and recovery acceptance

Recovery work dated 2026-10-15; ledger audit performed 2026-10-14 UTC.

## Status and evidence boundary

- Legacy behavioral reference: `afa43fae3e920210146abed514f136fd49f671b5`.
- Public recovery baseline: `3f831a8a862dabcc60f0049c154f93655d8004e2`.
- The original WBS29 parents (29.001–29.020) and WBS30 parents
  (30.001–30.017) are all unchecked and TODO at that public baseline.
  Neither section is complete. The inspected shallow recovery history does not establish whether a parent was *ever* marked DONE in
  inaccessible earlier history. The conclusion above concerns the inspected
  public baseline, not an invented historical audit.
- `29.001/native-canvas-tiles` and `29.010/native-compact-runtime` remain DONE
  as historical, narrowly scoped component records. Their frozen evidence is
  `migration/tests/canvas-ui-checkpoint.json` (2026-10-02, base `cb42d4c6…`)
  and `migration/tests/compact-options-checkpoint.json` (2026-10-02, baseline
  `36eff976…`). Neither proves faithful restoration of the full legacy UI.
  A shared handle name, working controls, 50 registered GUIs or preservation
  of 2,868 widgets does not prove the original drawing, geometry, factory or
  window/session behavior. Any reading of those results as full legacy UI
  completion is explicitly corrected here, without deleting prior evidence.
- Recovery followed loss of uncommitted work. Immutable reference snapshots
  establish correspondence; they do not recover the lost bytes. Native build
  recovery has been reported, while final runtime revalidation is in progress.
  This contract asserts no recovered-code 188-test result, ASan/UBSan pass,
  final screenshot approval, whole-port completion or distribution readiness.
  Final validation must identify the actual tested source and logs; historical
  checkpoint results cannot substitute for rerunning changed code.

The audit workspace `legacy-ui-recovery-audit-20261015` contains
`CORRESPONDENCE.md`, and `legacy/` / `head/` source snapshots whose line numbers
are preserved. It is a local audit artifact, not a packaged dependency. The
immutable public source links below make the correspondence independently
locatable. Current recovery edits require their own final-source evidence.

## Source-backed acceptance correspondence

All legacy links are pinned to the legacy revision above. All HEAD links are
pinned to the public recovery baseline, not moving branch names.

| Behavior | Exact source correspondence | Required recovery acceptance / open owner |
| --- | --- | --- |
| Dynamic image tabs | [Legacy `app/display/gimpimagewindow.c:513–530,2144–2154`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpimagewindow.c#L2144-L2154); [HEAD same file:566,2541–2562](https://github.com/seagetch/gimp-painter/blob/3f831a8a862dabcc60f0049c154f93655d8004e2/app/display/gimpimagewindow.c#L2541-L2562) | GtkNotebook allocation width > height gives LEFT; otherwise TOP, including square. Do not substitute canvas aspect or fixed LEFT. Resize, mode-toggle and reopen evidence belongs to 29.011/29.020. |
| Whole left dock hidden | [Legacy `gimpimagewindow.c:643–660,1887–1937`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpimagewindow.c#L643-L660); [HEAD:552,2620](https://github.com/seagetch/gimp-painter/blob/3f831a8a862dabcc60f0049c154f93655d8004e2/app/display/gimpimagewindow.c#L2620) | Hide the whole `left_docks` container in Painter mode without destroying saved children; restore ordinary-mode policy on disable. 29.001/30.005 remain open. |
| Whole native right Dockables collection | [Legacy `gimpimagewindow.c:2080–2109,2538–2625`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpimagewindow.c#L2080-L2109), [`app/display/gimpdisplayshell-overlays.cpp:670–726`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpdisplayshell-overlays.cpp#L670-L726), [`app/widgets/gimpdock.c:1081–1089`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/widgets/gimpdock.c#L1081-L1089) | Transfer the entire right column to the active canvas, retaining arbitrary native Dockables and hidden-child state. Canvas W < H places it at the top; landscape/square puts it below the wheel. Window W > H gives vertical docks; otherwise horizontal, including square. Preserve reentry/revision guards, balanced refs, parent restoration and close behavior. 29.001/29.011/29.016/29.019/30.005. |
| Custom Cairo LayerTileView | [Legacy `app/widgets/gimplayertileview.cpp:428–432,570 onward`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/widgets/gimplayertileview.cpp#L570), [`gimpdisplayshell-overlays.cpp:365–415`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpdisplayshell-overlays.cpp#L365-L415); [HEAD `app/widgets/gimppainterlayertiles.cpp:384,477 onward`](https://github.com/seagetch/gimp-painter/blob/3f831a8a862dabcc60f0049c154f93655d8004e2/app/widgets/gimppainterlayertiles.cpp#L384) | Restore the custom drawing-area/Cairo tile presentation, selection/visibility/group/mask interactions and popup behavior; a GtkListBox with thumbnails and labels is not equivalent. Use current safe preview/model ownership. Legacy width 88 and portrait y-offset 256 govern placement. 29.002–29.006/29.019/29.020. |
| Native color wheel and FG/BG | [Legacy `gimpdisplayshell-overlays.cpp:497–596`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpdisplayshell-overlays.cpp#L497-L596); [HEAD `app/display/gimppaintercanvasui.cpp:272–290,303–307`](https://github.com/seagetch/gimp-painter/blob/3f831a8a862dabcc60f0049c154f93655d8004e2/app/display/gimppaintercanvasui.cpp#L272-L290) | Keep the actual native color notebook/ColorselWheel, native FG/BG and shared context synchronization. Legacy request is 256×256, opacity 1.0, top right beside FG/BG. Do not replace the wheel with a custom rendering. 29.007/29.020. |
| Independent multi-tool bottom toolbar | [Legacy `app/tools/gimp-tools.c:385–406`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/tools/gimp-tools.c#L385-L406), [`app/widgets/gimptooloptionstoolbar.c:251–362`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/widgets/gimptooloptionstoolbar.c#L251-L362), [`gimpdisplayshell-overlays.cpp:448–492`](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpdisplayshell-overlays.cpp#L448-L492) | Separate toolbar controls share the canonical options model while ordinary dock controls remain intact. Bottom placement persists in portrait; allocation/requisition and dock overlap govern size. The toolbar must follow the active tool, including paint, non-paint and transform tools; MyPaint-only controls do not satisfy this contract. Multi-tool restoration is in progress and awaits final-source validation. 29.010/29.010/standard-tool-options/29.010/toolbar-ref-balance/29.011. |
| No added six-button action bar | [HEAD `gimppaintercanvasui.cpp:294–303,406–412`](https://github.com/seagetch/gimp-painter/blob/3f831a8a862dabcc60f0049c154f93655d8004e2/app/display/gimppaintercanvasui.cpp#L294-L303); [legacy overlay creation:624–645](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpdisplayshell-overlays.cpp#L624-L645) | Remove the added Undo/Redo/Compact Options/Tool Options/MyPaint Editor/Hide bar and its geometry reservation/index assumptions. Keep canonical keyboard/menu commands and necessary internal options actions. 29.010/29.020/30.001. |

The notebook, window and canvas aspect rules are intentionally different.
Square allocation is a required boundary test, not a reason to merge them.
Dock orientation must use the image-window allocation, while dock overlay
position/size uses the canvas allocation. A portrait canvas inside a landscape
window must not incorrectly force horizontal dock orientation.

## Active multi-tool restoration, 2026-10-14 23:55 UTC

The clarified requirement is a lower toolbar for the active tool, not a
MyPaint-only lower toolbar. Current source has replaced the earlier MyPaint-only
admission check in `app/widgets/gimpwidgets-utils.c` with lookup of
`gimp-tool-options-gui-horizontal-func` on `GimpToolInfo`.
`app/tools/gimp-tools.c:gimp_tools_register` registers that separate factory;
`gimp_tools_horizontal_options_gui` has a MyPaint branch and calls
`gimp_painter_paint_toolbar_new`, then `gimp_painter_tool_toolbar_new`, then
`gimp_painter_transform_toolbar_new`, for other tools. The paint, non-paint
and transform builders are being restored;
these dispatch references do not prove completed implementations or acceptance.
The cached ordinary dock GUI remains a separate control tree sharing the
canonical tool-options model. Reconstructing or reparenting that singleton
would risk native option classes' private control pointers and is not the
independent-toolbar contract.

### Runtime factory inventory, partial restoration

`29.010/standard-tool-options` is DOING, not DONE. The source builders and
runtime inventory reports the following 34 independent horizontal factories.
The compact-options native run passed 27/27 cases, exit 0, in
`recovery-ui-20261015/tests-multitool-extra/testlog.txt` (SHA-256
`3a2e1f6b2d1e0eb60bf418d9a3d68ea977a6f42672f5fef2f4c77b49178fccd5`).
The inventory exercises two independent controls against each retained dock
GUI, checking orientation, identity and native parent preservation across
destruction. This is a focused native result, not proof of complete controls,
full UI parity, full-pipeline success or sanitizer instrumentation. Environment
ASAN_OPTIONS/UBSAN_OPTIONS in the log alone do not establish instrumentation.
The coordinating build run also reports successful compilation of the native
application and three test targets against the frozen source. Final combined native/sanitizer evidence is recorded in the final focused
validation section below.
All IDs below have the `gimp-` prefix and `-tool` suffix.

| Runtime factory group | Tool ID stems | Current source |
| --- | --- | --- |
| Painter MyPaint (1) | painter-mypaint | `app/tools/gimp-tools.c:gimp_tools_horizontal_options_gui`, independent MyPaint GUI using the canonical options model |
| Paint (14) | paintbrush, pencil, airbrush, eraser, clone, heal, perspective-clone, convolve, dodge-burn, smudge, ink, mypaint-brush, bucket-fill-brush, painter-smudge | `app/tools/gimppainterpainttoolbar.c:gimp_painter_paint_toolbar_new` |
| Non-paint (15) | move, free-select, fuzzy-select, by-color-select, rect-select, ellipse-select, flip, zoom, color-picker, bucket-fill, gradient, cage, iscissors, crop, measure | `app/tools/gimppaintertooltoolbar.c:gimp_painter_tool_toolbar_new` |
| Original grid transforms (4) | rotate, scale, shear, perspective | `app/tools/gimppaintertransformtoolbar.c:gimp_painter_transform_toolbar_new` |

The new builders use independent property-bound controls and horizontal
inline/details layouts, not reorientation of the cached ordinary dock GUI.
Known incomplete details include rectangle/crop auto-shrink/unit/history
controls; modern bucket-fill line-art/stroke details; modern gradient
instant/modify-active controls; modern measure Straighten/associated transform
controls; clone source-description text and multi-selection sample-merged
sensitivity; and the modern transform direction-link control (the old
horizontal arrangement is retained).

The runtime inventory explicitly reports these 16 registered tool IDs without
a horizontal factory (again `gimp-<stem>-tool`): foreground-select, vector,
align, unified-transform, handle-transform, perspective-guide, transform-3d,
warp, text, brightness-contrast, threshold, levels, curves, offset, gegl,
operation. These current-tool omissions must be classified against legacy
requirements; no full legacy-family or modern-tool parity is claimed. Text
expansion is outside this recovery slice. Full per-control source mapping,
remaining control semantics and window/session behavior, and final combined
build/interaction evidence remain open. The factory inventory test and focused
binding/lifetime coverage do not close this task or its parent.

`app/tests/test-painter-legacy-layout.inc:legacy_tabs_and_collection` now
contains a genuine populated fixture: two native docks, each with a native
`gimp-buffer-list` Dockable. It checks orientation after landscape, portrait
and square requests, whole-column movement between two shells, and retained
dock ancestry. The planned run must validate the actual allocations and
populated collection; an empty-dock loop cannot establish orientation coverage.
This paragraph records test implementation and intended coverage, not a pass.
The focused multi-tool runtime result above is separate from final combined
runtime evidence and populated-dock acceptance, which remain to be appended
after execution against the final source.

## Obligations still open

1. Enumerate and implement the legacy per-tool horizontal factories with each
   control, value, notification, popup and orientation mapped to its source.
   The generic compact adapter is insufficient. Multi-tool horizontal factory
   restoration is now active through the distinct registration and dispatch
   described above. Neither the earlier MyPaint-only slice nor the new dispatch
   establishes standard-tool completeness. No unverified remaining-factory
   count is used as an acceptance inventory. The per-tool source mapping,
   implementation and validation remain open under 29.010/standard-tool-options.
2. Full toolbar window/session registration, state restoration, tool switching,
   ownership and hide/show model remain open under 29.010 and 30.005/30.006.
   Independent active-tool controls and ordinary dock coexistence are required;
   their in-progress restoration does not establish that the entire old toolbar
   window/session model was ported.
3. Run final-source native interaction, lifetime and multi-image tests, inspect
   landscape/portrait/square output, and separately build/run any claimed
   focused sanitizer or HiDPI coverage. Record actual failures and constraints.
   Do not count a recovered binary build as runtime acceptance.
4. Physical tablet behavior, Windows/macOS/Linux distribution and full UI
   acceptance remain their existing 29.020/32/33/34/36 obligations. Synthetic
   pointer tests and GTK scale factors do not close hardware/platform gates.

## Ledger reconciliation

No task was added, removed, checked or unchecked. The public baseline had
336 DONE, 584 TODO, 14 DOING and 1 BLOCKED rows. The active substantive
factory work now changes exactly `29.010/standard-tool-options` from TODO to
DOING; all other statuses were preserved in the UI recovery commit.
The following counts are that recovery checkpoint, not a live ledger after
subsequent task commits:

| Scope | Total rows | DONE | TODO | DOING | BLOCKED |
| --- | ---: | ---: | ---: | ---: | ---: |
| Full WBS/progress ledger | 935 | 336 | 583 | 15 | 1 |
| WBS29 including child rows | 35 | 2 | 32 | 1 | 0 |
| WBS30 including child rows | 31 | 6 | 25 | 0 | 0 |
| Original WBS29 parent rows | 20 | 0 | 20 | 0 | 0 |
| Original WBS30 parent rows | 17 | 0 | 17 | 0 | 0 |

The fixed initial 661 leaf-task plan remains distinct from the expanded
935-row ledger. `python3 tools/check_tasks.py` checks WBS/progress consistency;
it is not a UI acceptance test. G4, G6 and G7 are not closed by this correction.
Final recovery validation may be appended with exact evidence before commit.

## Final focused recovery validation, 2026-10-15 00:00 UTC

`migration/tests/legacy-ui-recovery-20261015.json` records pinned revisions,
24 final changed source/test/build/config/header-inventory hashes and log hashes.
Native coverage passes 148 cases: corrected canvas89 in
`tests-canvas-region-final-native/testlog.json` (22.61s), plus unchanged compact27
and entry-contract32 from `tests-final-native/testlog.json`, all exit 0.
Focused ASan/UBSan coverage also passes 148: corrected canvas89 in
`tests-canvas-region-final-asan/testlog.json` (57.99s), plus unchanged passing
compact27/entry32 from `tests-final-asan/testlog.json`. These are combined final
results, not one newly rerun 148-case invocation. `detect_leaks=0`:
LeakSanitizer is unavailable and no leak-checking success is claimed.

The prior canvas sanitizer attempt's allocation-size assertion failure is
preserved in the record. Comparing the relevant fixed 64×64 tile region avoids
unrelated full-widget allocation changes; the final native and sanitizer
canvas reruns pass. Earlier missing-menu fixtures were supplied by actual
Meson menu generation, not substitutes. These focused results establish no
whole-application instrumentation, physical-tablet/platform or full UI parity.

Ordinary portrait Paintbrush and landscape Move screenshots and logs are
hashed; both capture logs report `NORMAL_EXIT 0`. Both actual application logs
also contain a `Gimp-Widgets-CRITICAL` from `gimp_device_info_save_tool` failing
`GIMP_IS_DEVICE_INFO(info)`, plus four stale contexts (two `GimpStrokeOptions`
and two `GimpFillOptions`). The earlier recovery v4 log contains the same
warnings, but their presence in the untouched public baseline is unverified.
A zero exit is not a diagnostic-clean shutdown. These warnings remain open,
as do the 16 unsupported factory IDs, incomplete controls, toolbar/window/
session behavior and hardware/platform gates. WBS status remains DOING.
