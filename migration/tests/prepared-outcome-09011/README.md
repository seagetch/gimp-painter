# Explicit prepared Undo publication outcome (bounded 09.011 prerequisite)

`gimp_image_undo_publish_prepared` preserves transfer-full ownership and exposes
whether native stack insertion actually completed, separately from the borrowed
surviving entry. TRUE with NULL means observers expired a published entry; it is
not a prepublication failure and must not cause the controller to roll back a
committed operation. Existing `gimp_image_undo_push_prepared` retains its exact
survivor-return contract through the same internal implementation.

18 native tests and selected-six-unit ASan/UBSan tests pass, including all prior
12 and six retained/expired/rejected/frozen/last-owner/dirty-callback cases. Two
compiled causal mutants fail the specific expired and rejected assertions.
Optional pop guard 11, native Surface 24, actual portrait oncanvas preset UI 18
and perspective UI 24, and 433 C/C++ header probes pass. UI regression is not a
new visible feature; this task adds no screenshot claim. Xvfb is authenticated
1080x1920x24. Native dependencies/full application and leak sanitizer are not
fully instrumented.

The archive pins owned tested source and selected sanitizer inputs. This is a
scoped clean-private-build checkpoint, not an assertion that all current branch
sources were built. The prior prepared-undo and undo-pop-guard receipts remain
immutable predecessor evidence; their current-source checks naturally cease to
match this successor. `validate.py --check-source` checks this package's current
inputs; omission checks its archived evidence only. Independent Clone-policy
successor replay is recorded in the Clone worker's package, not relabeled here.

No global callback-side-effect rollback or native allocator/OOM guarantee is
introduced. FALSE reflects this helper's rejection before insertion; arbitrary
observer edits are not undone. Original paired pixels/evaluator/resources,
source runtime type/property and once-free duties remain open until the actual
controller/typed bridge integration is verified.

## Clean dependency replay

The first private run was retained privately as historical diagnostic after
three held Bucket provider candidates were identified. This package instead
contains the subsequent clean replay: those three providers were preserved
outside the source tree and restored exactly from committed HEAD; five earlier
held providers equal baseline77bcdf2b7f. All eight exact dependency sources are
archived separately in held-baseline-sources.tar.gz, not asserted equal to the
working candidate tree. build.log proves both changed tools units recompiled
and their archive relinked before the new normal, selected sanitizer, mutations
and portrait UI regressions. No held candidate test or hardening was retried.
