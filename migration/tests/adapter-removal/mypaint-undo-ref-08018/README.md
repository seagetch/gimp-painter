# Native MyPaint Undo source name and typed ownership

The original opaque `GimpMypaintCoreUndo` declaration is restored in
`paint-types.h`. The native type already exists: this does not alias a generic
PaintCoreUndo or introduce a second GType/backend. Its existing private
TypeTraits specialization moves into the public C++ header, where the thin
`MypaintCoreUndoRef` delegates adopt/retain/copy/move/release/reset to ObjectRef.
Type checks reject unrelated GObjects without consuming their references.

This is bounded source-name, get_type and typed ownership compatibility only.
The legacy public instance layout, direct parent and raw `Stroke*` property ABI
are not restored. The actual native entry derives DrawableUndo and owns a typed
state object; the original paired-history parent remains open for the explicit
external-resize cancellation observer obligation. Do not infer full legacy
runtime or ABI compatibility from a working handle.

C11 probes use the documented core-header GEGL prerequisites. C++14/noRTTI
probes include the public header first and twice. Both original missing names
fail against the preceding committed headers and compile against the successor.
An initial probe omitted the C core prerequisite; corrected setup, not a
production defect. The initial full build caught the private TypeTraits
specialization duplication, resolved by relocation rather than a second trait.

Native tests create an actual unpublished typed entry with native image, layer
and independent pixel buffer. A weak GObject witness and weak typed-state owner
prove retention through copies/moves, explicit release/adoption, and exact final
cleanup. Empty handles and wrong-type inputs are separate cases. This is a
model-only API test in authenticated portrait Xvfb with oncanvas enabled, not a
new visible UI feature; rendered paired history remains the immutable preceding
`paired-stroke-09011` checkpoint, not new screenshot evidence.

Selected sanitizer coverage instruments seven source units, excludes vptr,
leak detection and dependencies; no whole-application claim. Two compiled
header controls remove borrowed-reference retention or return the wrong native
type. Existing native bridge, paired controller and Surface tests are relinked
against the successor; old evidence archives remain unchanged.
