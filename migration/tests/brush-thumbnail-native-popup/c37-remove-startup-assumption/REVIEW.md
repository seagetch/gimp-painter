# C37: withdraw unsuccessful startup precondition

Review-only correction of public C36 e7a943ebc88869bd250a714fb5467d9da68817e8,
tree 9a84e136aa0d2b1f2f27eab2dbd97b6e4981f433. Task308 remains DOING / NOT ACCEPTED.

## Observed regression and correction

Run54 (41320949207) ended with four successful and four failed jobs. Both normal
Brush aggregate scales passed cases1–13, then case14 aborted because C36 assumed
exactly one application image window, but the list was empty. Prior fixture cleanup
calls close_shell, then gimp_display_delete; deleting the final shell destroys its
GimpImageWindow. Empty application window lists are therefore a legitimate aggregate
state. The private-target case does not require an application image window.

Restore the entire Brush include byte-for-byte from C35
(ef5f08e68c1f0692b40b938c9ea4c08763db1483), removing the added precondition and its
call. Do not add a zero-window skip, synthesize an image window or add another timing
allowance. Remove only the obsolete helper-model scaffolding. Replace its tests with
exact C35 source and private-case identity checks, retaining the original compiled
3.1-second negative and unchanged 3-second readiness-helper hash checks. Historical
C36 documentation and run receipts remain evidence, not current behavior.

## Separate latency failure remains

Run54 isolated sanitizer setup succeeded in 535560us, four frames / three stable
comparisons. Nevertheless the original first private wait took 12171110us, with
12165616us in one dispatch and stable=0. The startup barrier did not fix this failure.
Diagnostic GimpImageWindow check-resize/layout spans (10809729us / 11563051us) overlap
that dispatch; they are nested debugger observations, not additive CPU timings or
proof of an exclusive cause. The private target's own layout was about9.5ms.
No production latency or leak repair is claimed. Full native validation, remaining
199793–202433-byte process leaks, matched comparison and babl evidence remain open.

## Boundaries and validation

No production, workflow, protected-common, sanitizer, leak, timeout or acceptance
policy changes. All26 protected files, the21-case manifest and original private
body/readiness predicate must remain exact. Source/synthetic tests do not substitute
for hosted native aggregate and isolated sanitizer proof. No local ptrace or babl
execution and no GitHub publication is performed in this worktree.

Publication constraint: existing runtime seal requires a candidate's sole parent
to be accepted task307 e25a94e21efca6e6604ddac6f15570c049f421e7. A normal child of
C36 cannot satisfy it. This correction does not weaken that policy or independently
authorize replacing the published branch. Root must resolve publication authority.

## Completed local validation

The complete Brush source/synthetic suite passes1112 tests. The standalone owner
suite passes13 tests (also included by suite discovery); six protected workflow
test scripts pass. WBS and diff whitespace validation pass. Applying the two new
restoration assertions to exact C36 source produces two expected failures. All26
protected hashes match; no native execution or runtime acceptance is claimed.
An attempted broad tools/tests discovery failed on duplicate module naming before
running tests; the six exact workflow script invocations above avoid that discovery
collision and pass, with the failed invocation retained in local proof.

Independent review found no required source correction and independently passed
167 focused tests including seal/orchestration regressions. See
[LATENCY-INVESTIGATION.md](LATENCY-INVESTIGATION.md) for the evidence ceiling and
bounded next diagnostic specification, not an implemented latency fix.
