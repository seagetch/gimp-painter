# Original14.006/editability-contract: derived-cache pixel history

The pinned old Clone `is_editable` returnsFALSE. Old image-Undo duties
01.002/000338–341 discard `GimpDrawableUndo` for that type before dirty/redo
publication. The old frozen branch reads an uninitialized pointer; that defect
is not preserved. Current content-lock preflight alone did not cover low-level
history: the retained expected-red probe accepts ordinary-layer history, then
fails because Clone pixel-history publication incorrectly returns non-NULL.

The fix is specifically for `GimpDrawableUndo` descendants whose item is a
CloneLayer, not arbitrary user-content-locked ordinary layers. Public helper,
generic constructor and prepared publication all reject derived-cache pixel
history before dirty/redo changes. Prepared rejection consumes its transferred
object and reports `G_IO_ERROR_NOT_SUPPORTED`. The image is leased across
callbacks; drawable properties are collected once and retained through native
construction. Existing prepared publication's reentry marker stays before the
first image reference.

Four focused native model tests establish:

* Permanent Clone content lock, PDB content refusal, explicit selection and
  allowed movement/Undo/Redo; position lock independently toggles permission.
* Ten rejection contexts: public/generic/prepared routes, base/derived Undo
  types, open/frozen history. Clean state and exact redo object survive;
  prepared ownership is consumed and its error is explicit. Ordinary locked
  layers continue to accept low-level native history.
* A buffer toggle callback drops the caller's last image reference during
  property collection: the operation safely retains the image until rejection.
* A separate ordinary-buffer toggle counter proves one property collection,
  rather than a temporary collection/ref gap followed by another collection.

Combined clean verification overlays committed pop guard1436f90961 plus only
the owned policy/tests onto the parent-prepared81bc24545d checkpoint with four
committed preset metadata files. It is not latest whole HEAD. The normal suites
pass69 Clone,12 prepared-Undo and11 pop-guard cases (92 total). The selected
four Clone cases also pass focused ASan/UBSan with10 instrumented units,
59 RTTI-only C++ units and remaining dependencies uninstrumented, LSan disabled.
The existing pop-guard implementation has separate sanitizer proof; it is not
counted among these10 instrumented units. Source/archive/executable/DSO hashes
and exact commands are retained in the receipts.

A prior sanitizer attempt was interrupted by executor transport disconnection
while compiling a RTTI-only object; it produced no test result. The fresh final
run supersedes that incomplete attempt. No denied operation was retried.

This is native model/API evidence, not GUI input or screenshot evidence. Original
14.005 and layer-slot prerequisites remain unchanged; task remains DOING until
its original dependencies are satisfied. Separate Filter editability, paused
renderer/canvas lifetime work and skipped old runtime are not claimed solved.
