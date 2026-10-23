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

## Recovered native GUI verification (2026-10-11)

The restored source was built with standard Meson and the Fill tool/options GUI
test now has recoverable evidence: 32/32 TAP cases passed, Meson exit 0. The
server was unmodified Xvfb using its supported inherited loopback TCP listener,
with cookie authentication and no external-interface listener. Writable private
HOME/XDG paths avoided the previously documented fixture errors.

All 16 previously display-skipped registrations now have successful individual
runs, totaling 252 TAP cases. The record
`tests/native-loopback-gui-20261011.json` retains the qualification that Layer
needed a 180-second budget, Profile used real build-generated data, and Save
Continuation used the standard in-memory GSettings test backend. Original
failed/timed-out attempts remain preserved separately. This is not a claim that
a single default-configured aggregate run or installed-prefix smoke passed.

The separate standard sanitizer run still fails on signed integer overflow in
`app/core/gimpbrush-transform.cc:380`. Leak detection remains unavailable under
ptrace. Neither failure is changed into a passing result. 08.012 remains DOING
and the original completed-task count remains 135/661.

## Final original-task acceptance (2026-10-12)

Earlier statuses above are historical. Standard Meson normal and ASan/UBSan
each passed ruler6, Fill core40 and GTK32 (78/78), independently counted before
a later workspace reset. Final sanitizer raw logs were lost; the final acceptance
was reconstructed from recorded verified results and pinned public source,
without a new runtime run. Original08.012 closes at136/661; all independent
gates remain. See [final acceptance](tests/fill-tool-options-type/README.md).
