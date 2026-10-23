# 09.011/prepared-undo: native publication prerequisite

The original `09.011/mypaint-undo-stroke` remains **open**. This package implements only a prepared native GimpUndo publication route, not the typed stroke property, evaluator/pixel pairing, allocation-free historical pop, or atomic PaintCore commit.

`gimp_image_undo_push_prepared` consumes the initial reference only after valid image/type/owner/error/not-published/not-publishing preconditions. GInitable initialization and frozen-history rejection precede helper dirty/history/redo changes. Both original constructor and prepared routes share one native stack publication function; no replacement history backend. Image/Undo leases and weak return guards cover reentry, synchronous history expiration, and final-owner destruction. Legacy constructor preserves its prior dirty-before-initialization semantics.

## Evidence

- 12 native normal and 12 selected-six-unit ASan/UBSan cases: success/free/UndoRedo; initialization failure preserves redo pointer/dirty/depth; frozen rejection; wrong owner remains caller-owned; duplicate publication; grouped records; initializer reentry; immediate history clear; last image owner; toggle-ref reentry; legacy constructor reentry and last-owner release.
- 427 C11/C++14 header probes and reviewed changed-baseline header route; dedicated first/double C++14 noRTTI compile/link fixture.
- Both actual replacement-object mutants fail assertions: omitted initialization preflight; omitted final weak return guard.
- Authenticated Xvfb 1080x1920x24: existing oncanvas-enabled preset UI18 and perspective UI24, plus paint registration9 pass. This changes no visible UI feature; no new screenshot claimed.
- `missing-symbol.log` preserves expected absence of this *new prerequisite* API before implementation (not an original legacy API regression). `fixture-types-build.log` is a repaired fixture compile issue. `dangling-return-red.log` preserves the genuine initial implementation failure fixed without weakening the final-owner assertion.

Selected sanitizer instrumented only image-undo, undo, undostack, object, and the C/C++ fixtures. Dependencies/full app, LSan and vptr are not covered. Build source hashes match the immutable archive. Private build uses clean committed sources plus separately owned overlays, not an aggregate current-branch proof.

## Commit boundary / limitations

A NULL borrowed return can mean rejection **or successful publication immediately expired by observers**. Callers needing a commit decision must not infer rollback solely from NULL. Initialization/frozen errors are distinct from success with no surviving entry. Observer side effects are not rolled back. Arbitrary dirty/redo callbacks can change history/targets; final paired integration must establish an explicit commit outcome and reentry policy.

In particular existing PaintCore finish steals its baseline and allocates pixel snapshots after group-start has already invalidated redo. Wrapping it in a prepared stroke entry is **not** atomic. The remaining paired task must prepare pixel/state objects while rollback baseline is retained, reject unsupported target/geometry changes, and only then publish with a single well-defined commit boundary. No claim here that native mock initialization controls arbitrary callback side effects.

Run `python3 migration/tests/prepared-undo-09011/validate.py --check-source`. Without the option it verifies the immutable evidence archive; current source changes must not be resealed into this receipt.
