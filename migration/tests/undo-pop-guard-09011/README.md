# 09.011/pop-guard — optional native state lease

`GimpUndoPopGuard` is an internal opt-in GInterface with prepare/finish callbacks. It does not append native GimpUndoClass fields or change existing instance layouts. Every successful prepare receives one finish, in reverse order, including sibling rejection. FALSE must leave no held state lease. C++ implementers must not throw across either C callback.

Native image Undo/Redo pins the top/group tree, rejects pending paint and nested requests before any stack movement, prepares all guarded entries, and revalidates top/group identity and order before native pop. Pins and prepared owner guards survive remove signals, native pixel callbacks, Undo events and final notification thaw. Strong Undo/Redo propagate rejection rather than looping on an unchanged weak entry. Ordinary classes need not implement the interface.

11 normal and selected-six-unit ASan/UBSan cases: ordinary class unchanged; success/UndoRedo; rejected Undo/Redo preserving top and dirty; group reverse unwind; pending paint; remove callback nested Undo/Redo/strong rejection; notification lease; history altered during prepare (reject, unwind, no pop); caller-last-image release; nested groups; weak strong-Undo/Redo rejection. The fixture dirty-accessor compile typo is preserved separately, not called a product regression.

Three actual native replacement-object mutants fail original assertions: skip guard, skip stale-plan check, release guard before container removal. The last specifically proves the earlier race where checking readiness only inside the pop vfunc was too late.

Regression: prepared-publication12, real native Surface24, oncanvas-enabled portrait preset UI18 and perspective UI24, and 431 C11/C++14 header probes. Xvfb authenticated 1080x1920x24. No new visible UI feature/image claimed. Selected instrumentation covers image-undo/undo/undostack/object/interface/fixture only, not all dependencies, LSan or application allocator behavior.

This prerequisite does not close the original paired stroke Undo task. The controller must supply a real pin/transition guard (including compatible same-owner nested-group locking) and allocation-free newly introduced historical state install. Standard native pixel Undo allocation behavior is unchanged. Arbitrary observer-originated destructive history/target mutations are not rolled back; prepare-time observer mutation is detected and rejected, with all acquired leases released. Native callbacks that bypass controller guards remain outside this bounded state-ownership guarantee.

`python3 migration/tests/undo-pop-guard-09011/validate.py --check-source` verifies current selected source identities. The immutable archive remains replayable after later push-policy or controller changes; do not reseal it.
