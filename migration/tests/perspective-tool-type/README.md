# 08.010 — native perspective editing-tool type

Scope: original task 08.010, registration of the perspective ruler tool as a GimpDrawTool with native tool options. The prerequisite image-owned model is covered by 08.009. This does not complete all section26 ruler/rendering work or cross-platform gates.

## Source and runtime findings

The pinned legacy tool declares an empty GimpToolOptions subclass. Its rate and eraser-mode properties and custom controls are inside #if0. Native GimpToolOptions and gimp_tool_options_gui provide the active contract. The old brush-context mask is not consumed by this non-painting editing tool; native registration uses no paint resources. Stable tool identifier, shortcut G, help ID and perspective icon are preserved. Gimp3 stores tool visibility in GimpToolItem/toolrc instead of the old gimp-tool-default-visible qdata.

The four missing public cast/class/check/get-class macros are restored. The nonserialized guide object property is restored through the same common store, with borrowed setter input retained and getter output owned by the GValue. It is a tool-local association; hover/display attachment adopts the image model, as in the old tool.

Three new GTK regressions failed against the unmodified production source: cancelling Add from image notification left a guide, cancelling Remove from model notification lost the original guide, and proximity did not publish the configured status. The fix publishes transaction changes before notifications, retains models across callbacks, invalidates resumed work after cancellation/disposal, publishes transaction completion before rollback/Undo notifications, and invokes GimpDrawTool's proximity/status implementation. Final-reference destruction is included before returning a generation verdict.

## Acceptance status

Native GTK:21/21 and focused ASan/UBSan:14/14 against current source. Initial three production failures and a subsequently reproduced property-finalizer model-replacement failure are preserved separately. Windows/macOS/tablet and full legacy renderer compatibility remain separate gates. Normal Gimp3 icon/data-folder warnings are retained in logs, not treated as evidence of a warning-free application.

## Source routing

An independent source review verified all17 original08.010 assignments. Twelve model/image prerequisite or unrelated math/blank/MyPaint/Fill-registration hunks have only their08.010 route removed. Existing08.009 and other feature duties remain. The five retained duties cover tool-header inclusion, actual registration, the called hit-test transform helper, the implementation/options adapter and public type header. Routing correction contributes zero accepted obligations.


## Fixture corrections

The first registration fixture incorrectly expected the old qdata flag. GIMP3 uses GimpToolItem/toolrc. A second attempt read the dependency prefix's installed defaults rather than this checkout's shipped etc/toolrc; the final test directly feeds the current file to the actual native parser, after asserting native registration visibility. The initial display-switch finalizer fixture used oper_update during an active edit, which deliberately skips attachment; button_press reaches the intended path. The isolated old-toolrc case passed its assertions but failed teardown without a realized image/current device; it now initializes and closes the same actual GUI scene as the other cases. These fixture/setup failures are distinct from the measured production defects.

## Reproduction

Build the normal painter-perspective-ui target in a configured GIMP3 build and run all registered /perspective-ui cases on an actual GTK display. The run-native.py record runs each case in a fresh process with source and executable seals. For a fresh sanitizer executable, use migration/tests/build_perspective_sanitizers.py BUILD --test painter-perspective-ui --report REPORT (and --ninja-file for an existing guarded manifest). Run the14 focused cases with ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. The published build summary keeps complete selected source seals and commands while aggregating the1,276 unchanged normal-artifact hashes; the full report digest is retained for correlation. This is focused instrumentation, not whole-application or LeakSanitizer acceptance.
