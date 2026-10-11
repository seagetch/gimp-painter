# Fill tool/options checkpoint (08.012, unfinished)

This checkpoint preserves the five source-file changes recovered from the saved
2026-10-10 work, based on public commit
`1a643cc48022f0907bf716d082043b3332013b97`.

The changes complete the public tool class macros, translate the Rate property
label, retain the tool during operation-update callbacks and their parent/dispatcher
continuations, and guard preview restart after disposal or replacement input.
Six new GTK tests cover registration, configuration/preset round trips, property
bindings, owner loss, disposal during rollback, and already-disposed callbacks.
The multi-image fixture selects the newly created image's display.

## Verification status

The previous execution reported 32/32 normal GTK cases passing. The full passing
logs did not survive in the available recovery snapshot, so this is a historical
report, not independently recoverable acceptance evidence. The restored snapshot
contains earlier failure/probe logs and the source patch. Focused ASan/UBSan
completion is unconfirmed. No build or runtime test was rerun for this checkpoint.

The recovered patch was checked against its archive manifest and applied to the
exact public parent. All five resulting source files match the saved source
copies byte-for-byte. This establishes recovery integrity, not runtime correctness.

Original WBS 08.012 remains incomplete. The completed original-task count stays
135/661. This commit does not claim renderer, sanitizer, platform, or full migration
acceptance. Finish the remaining verification before closing the original task.

## Build procedure correction (2026-10-11)

New verification uses the [standard native build workflow](contracts/native-build-workflow.md):
a detached worktree at a recorded commit, a fresh out-of-source Meson build,
normal `meson setup`/`compile`/`test`, and review of generated source changes.
The custom helper that rewrote Meson executable metadata and Ninja commands,
its unit-test runner, and its two newly added fixture/documentation files are
removed. Earlier recovery archives and test artifacts are retained unchanged.

The helper's 29-test results are historical helper-specific checks, not native
GIMP or GTK acceptance. This procedure correction does not rerun native tests.
Fresh normal GTK and ASan/UBSan verification remain pending; 08.012 stays
**DOING**, and the completed original-task count remains **135/661**.
