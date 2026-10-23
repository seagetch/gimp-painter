# Constructor review follow-up: causal reentry and final application check

This receipt supersedes the **source-state** claim of the preceding nine-case
`mypaint-options-construction-08018` proof, whose files/archive stay immutable.
It reruns those exact nine fixture cases plus three new causal cases on the
final constructor source. This is the same bounded 08.018 constructor/type task,
not an extra completed WBS task and not readiness/GUI implementation.

## Causal proof

1. Acquiring the ToolInfo lease invokes its actual GObject toggle callback,
   which releases and finalizes the borrowed application. An application weak
   witness installed before acquisition becomes NULL; the constructor returns
   NULL without dereferencing its now-stale info->gimp pointer.
2. Acquiring the application lease invokes its actual toggle callback, which
   releases the caller's last ToolInfo reference. The constructor's info lease
   keeps the object alive through validation; its final weak pointer clears
   exactly when the temporary lease ends. The minimal application has no config,
   so no native options construction is attempted.
3. An actual `painter-mybrush-changed` emission hook during native options
   construction closes the application's BindingStore. Before the fix, the
   original nine cases and first two new cases pass, but the third asserts:
   `'options' should be nullptr`. Thus an active options binding alone was
   insufficient to publish options against a closed application.

The bounded fix validates options/context/info identity after construction,
releases the temporary ToolInfo lease, then rechecks application config/binding
state before publishing the owned options. The original transfer-full result,
name, sole native GType, independence and no tool-info assignment remain intact.
Gimp itself remains **borrowed**, just as for every native GimpContext. The caller
must keep it alive throughout use of the returned options. This is not a global
application lifetime redesign or a guarantee against arbitrary caller teardown
when the temporary application lease is released or after return.

The bare-endpoint test sets up GEGL and explicitly shuts down the minimal Gimp's
default contexts before testing finalization. Early fixture setup failures
(missing GEGL initialization and omitted minimal context cleanup) were not counted
as the causal production red; the archived red has both endpoint cases passing
and only the native callback publication assertion failing.

## Final execution

Normal 9+3 and selected five-unit ASan/UBSan 9+3. The original nine-case C/C++
fixtures are unchanged. New instrumentation includes the options adapter, binding
store and three C/C++ fixture units; dependencies/whole app/vptr/LSan are not
instrumented. Authenticated Xvfb1080x1920x24 is verified. Real initialized-app
cases set and read back oncanvas TRUE; minimal endpoint cases do not render UI.
No screenshot/UI acceptance claim is made.

Eight providers for all four paused paths are sealed to81bc24545d before build,
with post-test hashes unchanged. The archive is an exact selected-input receipt,
not a whole-HEAD claim. Existing Filter/Undo/fixture overlays remain explicit
ancillary inputs; the corresponding Filter tests are not part of this run.

The old two compiled negative controls and nine-case archive remain historical,
not silently resealed against the post-review source. The new actual causal
red/green closes the discovered publication gap. Readiness/gui/gui_horizontal
remain open; full legacy layout/runtime ABI and parent08.018 remain unclaimed.
