# Original WBS 08.009: perspective model and image ownership

The model is a native GObject with one common BindingStore slot containing its
bounded point state. The image owns a nullable reference. The C getter borrows;
the setter retains incoming before releasing previous, including self-assignment.
It publishes the new pointer before notifications and leases the image through
callbacks. Disposal first clears the pointer and blocks reattachment, then emits
removed/change notifications and releases the old owner. Three missing public
CLASS/IS_CLASS/GET_CLASS macros are restored to match the pinned legacy interface.

## Actual acceptance

The current core harness passes twelve cases normally and under focused
ASan/UBSan. Three new cases establish the remaining type and ownership boundaries:

- A real derived model overrides the removed class slot. Native type/class sizes,
  the zero-argument void signal and RUN_LAST order are checked. Self-assignment
  and repeated null assignment do not emit removal. Detaching and later closing
  an image each emit once. Explicit observer disconnect releases each closure
  exactly once; final model release cannot release it again
- A removed callback releases the caller's image, previous model and incoming
  model references. The setter's retained owners survive through notification;
  all three actual finalizers run by return, with one incoming removal at image
  teardown
- A duplicate preserves id, angle and all three points while remaining an
  independent model. Editing/closing the first image leaves the second model
  intact; closing the second releases its last reference

Every fresh image created by these cases is explicitly checked to have no model.
The existing bounds, captured legacy model fixture, source-helper comparisons,
threshold/tie behavior, owner release, replacement reentry, disposal reentry,
Undo/Redo and changed-callback last-reference cases are rerun on current sources.
The 7,500 snap comparisons use an extracted source oracle. The runtime fixture is
an earlier captured old-model result; this task does not claim a fresh old-binary
run. The existing corrected angle accessor remains distinct from the old id
getter/setter misbinding.

The old removed implementation re-emitted its own signal through the bound class
slot. The native base slot stays null and supports normal subclass overrides;
that recursive default is not restored. Removed means one actual image detachment,
not necessarily final object destruction. Caller-owned handlers may remain on a
retained model for later reuse. Production overlay ownership closes its scoped
changed connection before releasing/replacing the model. Tool, Undo and shell
callers retain their own references and use the image setter's borrowed input.

## Source duties and verification limits

The source mapping accounts for 31 obligations in 11 pinned hunks, including the
old image field, source registration, setter, teardown, model and actual tool
setter callers. The disabled #if 0 image-construction scaffold remains an explicit
inactive-source disposition; its two initial points are not activated.

Five unrelated ruler-profile hunks lose only the three model-gate assignments:
math include, blank removal, tool-header include, hit-test coordinate transform,
and the tool-only public header. All other tool/geometry duties remain unchanged.
The fifteen removed TODO rows yield no completed duties. Thirty-one actual
obligations are accepted; every other source-work row is compared unchanged.
Exact source blobs and changed-line payloads were verified, rather than claiming
full historical regeneration from missing local legacy trees.

The sanitizer builder now uses the current shared RTTI closure and replaces all
private archive aliases, including flattened test archives. Its default four
harnesses remain selectable as before; this task uses the new core-only option.
The report identifies instrumented and RTTI-only sources, source/header seals,
executable hashes and unchanged normal archive/object/executable hashes. Remaining
GIMP/dependencies are uninstrumented and LeakSanitizer is disabled. Twelve focused
passes are not full application, GUI, physical input or platform acceptance.
The existing test-profile data-folder warning remains visible. Historical test
reports and their source hashes are unchanged.
