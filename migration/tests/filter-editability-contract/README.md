# 15.006/editability-contract

Causal red: ordinary drawable push is accepted, but Filter derived-cache pixel
Undo was incorrectly accepted too. Pinned old Filter is_editable returnsFALSE;
the old generic Undo path excludes this cache (its frozen uninitialized-pointer
defect is not copied). The fix adds Filter beside Clone in exactly two type
predicates, preserving collection/lifetime/publication ordering and ordinary
user-locked layer behavior. Ten helper/generic/prepared base/derived open/frozen
contexts prove refusal before dirty/redo changes and prepared-object consumption.
Definition edits remain allowed; exact serialized payload survives Undo/Redo.
Clone4 and prepared18 native regressions pass; selected Filter2 native and the
combined Filter3 sanitizer pass. No blanket user-lock prohibition is added.

This is checkpoint-scoped model evidence, not UI, all-artwork, whole-HEAD or
old-runtime proof. Sanitizer scope/counts and exact source/object/executable
hashes are retained. Remaining dependencies are uninstrumented; LSan disabled.
Original15.005 prerequisite and criterion remain unchanged.
