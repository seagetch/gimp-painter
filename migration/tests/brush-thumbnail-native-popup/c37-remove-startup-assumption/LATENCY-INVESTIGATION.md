# Case14 latency: evidence and next bounded investigation

Read-only investigation, 2026-11-03. Source references below are to public C36 commit `e7a943ebc88869bd250a714fb5467d9da68817e8`, not the changing C37 working copy. No production latency fix is established.

## Evidence restored

Run54: https://github.com/seagetch/gimp-painter/actions/runs/41320949207

Paths below are relative to the retained run54 evidence archive data directory.

- `run54/owner-exported-records/sample.json`: numeric owner events and bounded sampled stacks.
- `run54/owner-exported-records/build-identity.json`, `identity-verified.json`, `primary-final.json`: separate diagnostic identity evidence.
- `run54/run54-terminal-observation-summary.json`: native results and timing summary.
- `run54/logs/brush-sanitizer.log`: original sanitizer job log.
- `run54/logs/brush-positive.log`: positive job log, including aggregate zero-host assertion failure.
- `run54/artifacts/`: all eight original artifact ZIPs.
- `c36-source/app/tests/test-painter-brush-thumbnail-popup.inc`: frozen C36 case source.

Containing retained run54 supplement archive was SHA256-verified as `30c0b69d1e65335590aec7090c2dda347c13fe7314402573ff663a6f0b3c7e72` (49,256,930 bytes).

## Established observations

Native isolated sanitizer initial-host setup reports ready after535,560us, four frames/three stable comparisons; subsequent original private first wait still reports12,171,110us elapsed and12,165,616us dispatch with zero stable comparisons. Thus C36 initial readiness does not resolve later latency. Aggregate zero-host failure is a separate introduced regression.

The separate, nonacceptance debugger owner trace records the following relative times (microseconds). They are debugger-observed timings, not native performance measurements:

- Original GimpImageWindow layout enters at10,504 and exits11,573,555:11,563,051us.
- Its nested check-resize enters763,774 and exits11,573,503:10,809,729us. Do not add these overlapping durations.
- Private target layout enters12,176,469 and exits12,186,033:9,564us.
- Private target nested check-resize enters12,176,529 and exits12,186,009:9,480us.
- Enclosing main-context dispatch exits12,539,519.

This localizes most observed work to the original image window, before the private target can complete its first geometry wait. It does not prove the initiating invalidation or exclusive cost source. Sample1 includes original wait/open/case frames. Samples2–8 all hit the96-frame cap; six contain synchronous librsvg loading, and all contain8–12 `gtk_widget_get_preferred_width` frames. None of those seven retains the internal GIMP widget/caller. The capture does not prove that every sample belongs to the same wait invocation.

## Exact source sequence

`app/tests/tests.c:488–498` initializes GUI/icons, restores GIMP, and calls `g_test_run` directly. In `app/tests/test-painter-brush-thumbnail-popup.inc`:

1. Line1838 runs C36 initial main-window readiness.
2. Lines1839–1843 lookup tool, construct private options, copy context properties, assert null parent. `app/tests/test-painter-compact-options.c:95–96` shows `info()` is a lookup, not global tool selection.
3. Lines1844–1850 construct private brushes and assign the private context brush.
4. Lines1851–1856 retrieve and call the registered horizontal factory. `app/tools/gimppainterpainttoolbar.c:648–695` constructs the independent toolbar; shared controls start at384. No direct global-theme change was found in this path.
5. Lines1857–1858 create an ordinary GtkWindow, add toolbar, show all, then call `drain()`. The helper at `test-painter-compact-options.c:70` uses `gimp_test_run_mainloop_until_idle`; an idle drain is not proof that subsequent frame work or later invalidations cannot occur.
6. Lines1859–1860 locate the thumbnail and call `brush_popup_open`. Its line426 first calls `transform_popup_wait_stable_geometry`; reveal and click/emission happen later. Thus this stall precedes popup opening.

## Hypothesis, not established cause

Mapping the new ordinary GtkWindow may alter original-window activation/backdrop style after the initial readiness barrier. `app/display/gimpimagewindow.c:984–1027` implements `style_updated`; it chains the parent at994, requests statusbar preferred size at1001 and menubar preferred size at1010, then updates geometry hints at1024. Theme CSS explicitly contains backdrop selectors (`themes/Default/common.css:718`,883,887). This provides a concrete path worth observing, not proof it executed during the slow interval. Delayed icon-theme/style invalidation or another constructor side effect remains possible.

## Minimal next diagnostic contract

Use a separately sealed, nonacceptance source/build overlay only; do not modify the primary/protected acceptance sources or bypass denied local execution.

- Attach fixed-size numeric probes to existing original image-window owners before private options/factory work. Mark before/after context copy, before/after factory, before/after new-window map, before/after idle drain, and entry/exit of the original first geometry wait.
- Record `notify::is-active`, `state-flags-changed` (including backdrop bit), `style-updated`, `check-resize`, frame-clock layout, and GtkIconTheme changed events with bounded static buffers. Validate signal support; missing hooks, clipped records, destruction, replaced clocks and unmatched entries must be explicit. No allocation or logging inside hot callbacks.
- In that same separate overlay, bracket the existing statusbar and menubar preferred-size calls in `gimp_image_window_style_updated`, using fixed role IDs and monotonic timestamps. Preserve their order and arguments. This distinguishes costly style-handler requests from later descendant resize traversal; absence of bracket hits disproves attribution to those two call sites, not all style work.
- Export only bounded numeric records after observation. Preserve the original3-second deadline, stable predicate, test sequence, sanitizer options, leak gates and protected26. Diagnostic observations cannot supply an acceptance pass.
- Stop when phase ordering and owner attribution are established or the bounded capture is inconclusive. Only then choose a narrowly supported production/fixture correction. Do not insert another prewarm, extend deadlines, disable focus, suppress leaks, or change icon settings merely to make the test pass.

No repository edits or runtime debugger/babl execution were performed for this investigation.
