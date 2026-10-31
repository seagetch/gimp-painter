# C31 review candidate: runtime icon root and Ninja alias parser

Task 308 remains DOING / NOT ACCEPTED. No C31 hosted/native result is claimed.
Same-task successor retains accepted parent e25a94e21efca6e6604ddac6f15570c049f421e7.

## Exact C30 observations

Run48 terminal receipt was re-observed after the execution environment reset.
The recovery receipt is preserved alongside this note (SHA256
bf3edd1ed2aa6df1ab3d58c9038089b05ca265b4849a1076ca77d217b6bb9e7a).
Its fresh observed_utc and recovery observation_kind are explicit. The original
receipt SHA256 was5f33a8b00576e08347c00b6c724636e8fcdb1903558c110df5b9a53c481f8047;
those original bytes are unavailable, and no exact original-tree recovery is claimed.
All eight original ZIPs were restored and verified against GitHub API hashes/sizes,
and all eight original logs were restored. The factual conclusions remain unchanged.
All eight jobs completed: four success, four failure. Fixed-focused mask passed
(exit0), historical focused control was EXPECTEDFAIL(-5). Fixed full107 passed
42 leaves then failed native_tool_icon_preview_lifetime at source line1928
(exit-6): the production icon lookup assertion, not a proven lifetime defect.
The icon fixture generation command occurred in the build log.
Matched inspection failed all14 with resource-relative-name-invalid;13 runs
were collected (12 primary), but no valid identities/comparison resulted.
The exact raw alias query/environment is not exported, so its hosted cause is
not proven by the synthetic reproduction below.
Sanitizer had20/21 assertions with leaks199965–202605 bytes; case14 stable0,
SIGABRT at12.474651s (dispatch12.470797s). Mask sanitizer passed its one assertion
but failed exit1 with ASan/LSan; historical/full107 controls were not run there.

## Narrow changes and reproduced mechanisms

- Both nested mask registrations now set G_TEST_BUILDDIR to build/app/tests.
  Actual GLib probes show the old executable-relative resolver selected
  build/app/tests/color-ownership/gimp-test-icon-theme; the shared fixture is
  build/app/tests/gimp-test-icon-theme. The strict runtime validator binds this
  exact root. Native assertions, sources, targets and deadlines are unchanged.
- The alias parser consumes Ninja's exact four-space indentation and optional
  `| ` implicit / `|| ` order-only edge markers before applying the original
  strict relative-path checks. Meson1.8.3/Ninja1.13 tiny real alias fixture
  reproduced old resource-relative-name-invalid and corrected PASS. Official
  Ninja ToolQuery and Meson generate_run_target source corroborate the syntax.
  Traversal, outside absolute paths, malformed markers and sections still fail.
- On alias-input parsing failure only, a finite catalog-alias-inputs substage
  is attached to the existing bounded runtime failure. Original error codes
  and rejection outcomes remain; no raw paths, environment or query export.

## Limits

Local unit/contract tests and synthetic GLib/Meson/Ninja probes are not GIMP
native acceptance. Fresh exact-source hosted full107, matched identities,
sanitizer validation and all original required gates remain necessary. No ptrace or babl
execution was performed. Historical failures and original policies remain.

## Original review validation

Full Brush CI unit suite:1063 tests PASS. Task policy PASS;49 Python AST checks,
9 shell syntax checks,26 protected common-file hashes and605 seal entries PASS.
An initial test attempt lacked historical Git blobs; after exact accepted-parent
read-only restoration, the entire suite was rerun successfully. No test was skipped
to obtain this result. These checks do not establish C31 native acceptance.

## Recovery revalidation

The eight functional/source-test files match their previously reviewed SHA256s.
Fresh recovered-source Brush CI suite:1063 tests PASS (36.489s); task policy,
49 Python AST checks,9 shell checks and26 protected hashes PASS. The recovery
changes only this review metadata, the reconstructed terminal receipt and their
seal hashes versus the previously reviewed C31 candidate. The new tree must be
verified independently; the original2fadb52b3e38e9026c50387f508195f80a1716c8
tree is historical, not asserted as the recovered identity.
