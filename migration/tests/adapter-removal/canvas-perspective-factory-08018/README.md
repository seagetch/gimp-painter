# Canvas perspective native factory and owning facade (08.018)

Bounded child of 08.018; **not** restoration of historical C/layout ABI. The
current one-argument `GimpCanvasItem *gimp_canvas_perspective_guide_new(shell)`
signature is retained. Historical inventory records a two-argument, differently
spelled return type, which this change does not silently declare compatible.

The C factory now returns NULL after any contained binding initialization or
constructed-stage exception; direct `g_object_new` remains an inert, safely
disposable native object. Construction registers shell/display/image/guide
connections before activation, and failure closes partial registrations.
`gimpdisplayshell-items.c` tolerates a missing optional overlay. Its teardown
already clears the parent canvas group and then resets the borrowed pointer;
there are no other production dereferences of `shell->perspective_guide`.

`CanvasPerspectiveGuideRef` is an owning facade over this same checked C
provider and native GType, not a second Impl or a legacy layout cast. Native
canvas objects are nonfloating. `adopt` consumes exactly one existing ref;
`retain` takes one, and copies/moves/reset follow ObjectRef semantics. The shell
is borrowed, exactly like the base canvas item; it must outlive item operations.
Destroying an owning facade does not extend the shell's lifetime.

The dedicated native test target compiles actual callers as C11 and injects
four exception categories from C++ at three construction stages: before store
creation, during the second connection after the first was installed, and
after activation. Raw-constructor and checked-factory cases check failure,
inert extents, disconnected installed signals, repeated dispose, finalization
and healthy subsequent creation. Typed facade tests cover wrong-type rejects,
empty refs, copy/move/self-assignment, lifetime and borrowed shell ownership.
A failed optional overlay during actual shell creation is drawn and torn down.

The existing perspective UI suite remains the rendering/original behavioral
regression. Both test fixtures explicitly enable oncanvas UI; screenshot
capture asserts it is visible. Native runs use authenticated 1080x1920x24 Xvfb.

Sanitizers are bounded, with LSan disabled. The exact instrumented sources and
RTTI-only units are listed by `build_canvas_factory_sanitizers.py`; they do not
establish full application/dependency coverage. Original timeouts remain 60 s.
Expected-red and setup failures are distinguished from accepted runtime proof.

## Accepted checkpoint and retained regression

The checked native factory passes **27/27 normal (2.71 s)** and **27/27
ASan/UBSan/float-cast-overflow (4.97 s)**, with the unchanged 60-second cap.
The build instruments 39 units and builds 41 additional RTTI-only units. A
source-frozen normal snapshot and precise sanitizer build manifest are retained.

Enabling oncanvas explicitly exposed a separate, genuine tool-group retirement
bug in the existing full perspective UI regression. Its first seven cases,
including actual marker rendering and portrait capture, passed; the eighth
`old-toolrc-preserves-groups` aborted because live toolbar ownership kept an
old group's tools parented during immediate deserialization. This run is
**FAILED**, not skipped or accepted. A separate implementation task owns that
repair; no manual test detach or assertion suppression was used. The screenshot
is a successful rendered scene from that otherwise failed aggregate.

The earlier red test aborts at the checked C factory returning non-NULL after
an injected initializer fault. A preceding linker setup failure (wrapper flag
without wrapper definition) and initial syntax-only declaration warnings are
separately retained; neither is runtime proof. The corrected sources compile
without new warnings in the five-unit syntax checks.

## Final repaired aggregate

The separate tool-group retirement fix was then verified without excluding any
UI test. `final/` passes **27 factory +24 perspective UI** cases normally
(2.86/5.36 s) and under the combined sanitizer build (4.89/8.87 s), all within
original 60-second caps. The union instruments **47 units**, plus **41 RTTI-only
units**, with zero source drift. The 24 UI cases include all original 22 and
two additional repeated retirement regressions. The failed first aggregate
above is retained unchanged. Final portrait evidence and the separate repair's
receipt hashes are bound in `final/tool-reset-binding.json`.

This supersedes only the current UI-regression failure, not the historical ABI
or unimplemented explicit-guide public operation. The canonical factory entry
remains PARTIAL. A separate next child owns restoration of its old signature
and explicit model property; no source/layout compatibility is implied here.
