# Filter entry fixture startup quiescence

Task `08.018/blinds-entry-fixture-quiescence` is **DONE** for the bounded fixture
repair at exact cedf6968c2bfd1d7f7b0623be713b27e449ac3c2. Fresh normal aggregate
171/171 and the complete 35-case ordinary ASan/UBSan registration passed at the
original 120-second budget, with the actual restored provider and original
lifetime/finalization assertions intact. The final source/build/runtime seal
passed. See `../owned-generic-ordinary-acceptance.json` and its reviewed archive.
The shared acceptance explicitly records the separate invocation amendment for
three legacy Fill Brush comparisons and preserves its earlier wrapper failure;
this fixture registration itself needed no such amendment. Parent08.018 remains
open; no blanket shutdown, leak-free or broader platform claim is made.

This is a documentary descendant of the tested source. Normal compiled outputs
were subsequently disposed of after evidence preservation; future runtime
packaging requires a rebuild and fresh full revalidation.

## Retained failure

The fresh ordinary normal aggregate at public commit
`99d398b54cf86fad3e9a6cb471373f8042db152d`, tree
`21d50ee742cb0371e577033c2f1b56f73d2e8d9a`, completed all 171 registrations:
170 OK and one FAIL. `filter-entry-contracts` planned 35 cases, completed 20,
then aborted at `/filter-entry/cpp/checked_transform_reentry`. Its subprocess
exited 1 on the fatal `gimp_finalize` warning that two contexts remained.
The registration itself returned -6. `initial-failure.json` preserves its
complete sanitized result record, aggregate counts and original result-file
SHA-256. It remains a failure, regardless of subsequent successful reruns.

## Source ownership trace

At this exact baseline:

1. `app/plug-in/gimppluginmanager-restore.c:104` creates the restore
   `GimpPDBContext`, passed to actual plug-in query/initialization calls.
2. `app/plug-in/gimppluginprocframe.c:78` retains that context in each
   plug-in's main procedure frame. `app/pdb/gimppdbcontext.c:232` creates its
   owned `GimpStrokeOptions`.
3. Normal plug-in quit reaches `gimp_plug_in_close (plug_in, FALSE)` in
   `app/plug-in/gimpplugin-message.c:172`. On Unix,
   `app/plug-in/gimpplugin.c:774-778` retains the closed plug-in for a
   `G_PRIORITY_LOW` child-watch source. Removing it from `open_plug_ins`
   therefore does not prove its lifetime ended.
4. The child-watch source's destroy notifier releases that remaining owner.
   Plug-in finalization disposes the main frame, which releases its context;
   PDB-context finalization releases its stroke options.
5. The three isolated entry lifetime tests previously called only
   `gimp_gegl_exit`, `gimp_contexts_exit` and reference release. They did not
   dispatch the startup child-watch sources before synchronously destroying
   the application inside an entry callback. The context and stroke options
   can consequently outlive their borrowed application.

This is a source-supported diagnosis of missing fixture quiescence. Runtime
acceptance must establish that the two observed startup objects actually
retire and that all original entry lifetime contracts still pass. It is not
a blanket claim that every application-shutdown context warning has this cause.

## Bounded candidate

Only `app/tests/test-filter-entry-contracts.cpp` changes executable code.
The helper runs at the start of each destructive subprocess, before creating
its tested image/layer and before registering global custom transforms.
It weakly observes all current PDB contexts and their stroke options without
retaining them, invokes the ordinary plug-in-manager shutdown entry, and
requires the manager's open-plug-in list to be empty. It dispatches through
the existing 20-second bounded wait until both observed objects have retired.
It then requires no PDB contexts remain and revalidates the actual restored
bundled Blinds provider. A timeout, surviving context, open plug-in, missing
provider, warning or unchanged original assertion failure remains fatal.

All three destructive tests retain their original checks that filter, image
and application stay alive inside the callback and finalize exactly once
before the checked entry returns. No loop dispatch is added inside that entry.
No production source, plug-in search path, registration, timeout budget,
warning policy, expected error or existing assertion is changed. The original
35-case registration and actual plug-in startup fixture remain intact.

Candidate test-source SHA-256:
`d85cd3d34a8f0a1a3bb21b73bf456300679312f071b731799afa4e4192d64560`.

## Normal prepublication verification

The exact reviewed base-plus-patch candidate passed the ordinary full default
build and complete 35-case registration at its original 120-second budget
(8.75 seconds). The authenticated loopback TCP Xvfb was 1080x1920; source,
ELF, complete build freshness and all 361 staged catalogs remained unchanged.
See `normal-prepublication.json` for the exact base, patch and source hashes.
The initial failed aggregate remains rejected. Subsequent edits only record
these results in the documentation and receipt; the executable test source,
fixtures and build definitions are unchanged. This is a normal repair check,
not sanitizer acceptance or a replacement aggregate.

## Acceptance criteria (satisfied by the shared receipt)

- Complete ordinary Meson `filter-entry-contracts` registration, all 35 cases,
  on fresh profiles with actual bundled providers. Do not replace it with
  selected `-p` cases or a no-plug-in fixture.
- Complete ordinary ASan/UBSan registration, all 35 cases, because the changed
  fixture observes weak-notify callback storage and deferred child lifetimes.
  Preserve RTTI/vptr and the established explicit LeakSanitizer limitation.
- Run the existing 14 generic-stroke registrations in both configurations
  as part of the current acceptance scope, without attributing its results to
  this test-only repair.
- Preserve the initial failed aggregate and record a later whole-registration
  aggregate separately; focused repair acceptance cannot relabel the old run.

Use `migration/contracts/native-build-workflow.md`: a distinct source
worktree and ordinary fresh out-of-source Meson configuration for each mode.
Never edit recovered build internals or modify the frozen development checkout.
