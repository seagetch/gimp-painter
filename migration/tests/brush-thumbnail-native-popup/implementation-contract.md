# Brush actual-thumbnail native popup — implementation contract

Status: root-approved bounded leaf `29.010/brush-thumbnail-native-popup`; implementation in progress, not accepted. Root approved the scoped architecture and evidence-backed press/spacing policies. Prepared 2026-10-25 in the dot cloud task. Root remains the sole publisher.

## Provenance and isolation

- Original isolated detached worktree: `/workspace/shared/gimp-brush-popup-next`.
- Original implementation base: `ef6640451c1c7ac50a7a354e8b57ecfe1ebc8bbb`.
- Current source integration: `/workspace/shared/gimp-common-layer-rebuild`, on root-accepted immediate pre-Brush baseline `e25a94e21efca6e6604ddac6f15570c049f421e7` (tree `8634eeef49c81aaa5c25192cb76d749ff680683f`). Only the bounded reviewed Brush delta and a new pending task row are integrated; accepted task-307 source/evidence remain preserved. Final candidate commit and all rebased runtime acceptance are pending.
- Source plan verified SHA-256: `039541c0c5ebf3570f94af5d8c5af85d2dc326263981e252aef27e9841b20cc9`.
- Legacy source: `afa43fae3e920210146abed514f136fd49f671b5`.
- No commit, push, deployment, desktop access, CI acceptance, or task completion is implied by this document.

## User-visible scope and decisions

1. Change the **actual Brush resource thumbnail** used by `Canvas::update_toolbar` → `gimp_tools_create_painter_toolbar_gui` → `gimp_painter_paint_toolbar_new` → `paint_toolbar_resource(options, FALSE)`. This is not the compatibility compact adapter or the adjacent Brush menu.
2. Clicking this thumbnail opens a self-anchored native popup containing a live brush grid on the **same GimpContext**, tool size/aspect/angle, separately labeled **resource spacing**, a 256-pixel brush preview, and generated-brush shape/radius/spikes/hardness/aspect/angle controls. The inline tool size remains. Layout must fit the logical 540-pixel width of the required scale-2 portrait screen; a bounded scroller permits all controls to remain reachable.
3. Selection and editing mutate the canonical context and canonical brush immediately. Escape, outside click, confirm, switching A→B, reopening, and toolbar destruction do not roll back live changes and do not explicitly or implicitly save through this popup. No draft brush or staging context is introduced.
4. Existing current link flags remain authoritative: the native PaintOptions model owns linked geometry updates; the popup does not add unconditional size resets. Following run 23, generated radius linkage synchronizes after mask regeneration, not from a stale pre-dirty mask. A cached completed radius and selected-frozen pending-thaw state preserve radius-only behavior, queued notifications, freeze/thaw and reentry; current-selection, GUI/link and disposal guards remain required. Immediate selection resets and all other linked-property callbacks are preserved. Final linked size must equal the regenerated brush's authoritative geometry, not merely match a stale control. Test link-on/link-off selection and radius edits. This remains a deliberate modernization relative to the old unconditional UI size reset.
5. Generated controls require a writable generated brush. Resource spacing permits live memory edits on any selected brush, including disk-read-only and non-generated brushes, exactly as the old Tool Options spacing handler did. NULL selections cannot receive resource edits. Preserve native setter-specific dirty behavior: spacing emits spacing-changed/notify without explicitly dirtying, generated mutations dirty the brush. An earlier proposal described the legacy spacing behavior as unsafe; that description was unsupported and is withdrawn. Disk writability is not a blanket memory-editing prohibition.
6. The existing adjacent Brush details menu, including tool spacing/hardness/force/link and lock controls, remains intact as a declared current extension. The normal vertical dock, stock selectors and Dynamics thumbnail remain unchanged. Dynamics gets a separate task.
7. Native outside-dismiss propagation remains FALSE; Escape closes. Stock selectors elsewhere retain private staged context and consumed TRUE policy. Thumbnail wheel input is consumed without changing resource, matching the old popup button; popup grid scrolling remains normal. Match old pointer button-press opening and FALSE return via a private opt-in GtkButton subclass, without chaining to GtkButton's default press handler (which could steal the new popup grab). Keyboard/programmatic clicked activation may additionally open when no host exists, without duplicate pointer-companion reopening. No global GTK class/vfunc mutation.
8. Following run 20, the fresh Brush host alone constrains its preferred/edge-flipped position to the monitor before mapping, so a form that fits a HiDPI portrait screen does not remain off-screen after the legacy flip. Existing retained-host/common placement remains unchanged. This is an explicit accessibility correction, not exact legacy edge-placement equivalence.
9. The raw selector explicitly shows the current canonical brush description, synchronized on retarget, notify and post-regeneration `name-changed`. Its selector reference and observations share the existing popup-scoped lifetime, with reentry checks after label writes. No `GimpContainerEditor`/`GimpDataEditor` is constructed. NULL, A/B selection, changed generated dimensions, rename and title-triggered retarget/destruction are tested; old-runtime title/layout identity is not claimed.

## Architecture and minimal production surface

### A. `app/tools/gimppainterbrushpopup.cpp` / `.h` (new)

Export one C factory: `GtkWidget *gimp_painter_brush_popup_button_new (GimpPaintOptions *options)`.

- The button's thumbnail uses the current property-bound view on canonical `options::brush`; it does not reuse/reparent a dock widget.
- Fresh popup body construction uses current `GimpContainerIconView`/`GimpContainerView` APIs with the canonical context; no `GimpContainerPopup` and no nested stock selector are needed in this body.
- Tool geometry uses ordinary current property widgets. Resource controls use a popup-scoped typed C++ controller registered through `BindingStore`, with `ObjectRef`, `WeakRef`, `Connection`, exact callback signatures, and C-boundary exception handling. Do not instantiate `GimpBrushEditor` or `GimpDataEditor`.
- The controller retains the actual observed brush while active. Retarget first advances its epoch and disconnects the old `Connection`, then changes its retained target, installs new observations, and synchronizes view/adjustments without writeback. Release the old target only after its observer is detached.
- Every resource UI callback leases its target, verifies current epoch, current context selection and field-specific eligibility, and writes only changed values through native setters. After a synchronous model call can reenter, do not touch prior target or controls without revalidating controller/epoch. `close()` first makes the controller inert, then disconnects context/resource/control observations and releases held target. Destruction is idempotent.
- Fresh body identity on every open simplifies close semantics and avoids an inactive editor holding a resource. No save API is reachable from the controller, its switch path, or teardown. Explicit Save remains the ordinary existing resource action/API, verified as a positive control rather than a new popup button.

### B. `app/tools/gimppaintertogglepopup.cpp` / `.h`

Add a narrow opt-in **fresh-view factory** path to the accepted host controller; do not change the retained-body semantics of any existing caller. Proposed internal C interface has a model, a create-view function and a stable host name; implementation may refine the name but must retain this contract:

- Factory model is weak between openings and leased during construction. Factory creates a fresh body after the prior host/body is retired, not before.
- Existing retained-body factories keep their exact behavior and tests. The new Brush path self-anchors and names its host `painter-brush-thumbnail-popup`.
- On cancel/confirm/destroy/unmap, retire the fresh body's binding before releasing it; destroy the fresh content rather than returning it to a retained slot. Reentry during construction, host signals and body destruction must not install stale content or reopen a retiring host.
- After sizing the fresh Brush form, revalidate host/controller/owner liveness, compute the old preferred/flip position and clamp it to monitor bounds before the first map. The independent Brush anchor oracle retains full bounds checks; other retained native helpers do not adopt this policy.
- Preserve the existing deferred stock-child lifetime mechanism for retained callers. Brush content intentionally has no nested `GimpContainerPopup`; do not weaken shared child-cancellation safety to simplify fresh mode.

### C. `app/tools/gimppainterpainttoolbar.c`, `app/tools/meson.build`

Return the new factory only for the Brush branch of `paint_toolbar_resource()`. Keep Dynamics stock creation/config bindings unchanged. Add the new C++ file to the tool source list. No change to `gimpdataeditor.c`, `gimpbrusheditor.c`, vertical dock factory, paint-option model, or generic compact adapter is required.

## Required evidence and tests

Implement a focused `app/tests/test-painter-brush-thumbnail-popup.inc` included/registered by `test-painter-compact-options.c`; reuse actual same-shell toolbar discovery and XTest helpers, not an isolated hand-built button. Additional manifest/CI registration belongs to this task only if required to execute these tests; preserve candidate 307's accepted jobs and evidence.

1. **Actual-entry final-test negative:** run the final actual-thumbnail test on root-pinned accepted-task-307 pre-change source `e25a94e21efca6e6604ddac6f15570c049f421e7`. `ef664045…` remains the original implementation base, not the final accepted baseline. Reject a stock `GimpContainerPopup` by required extended content and same-context wiring, not just `GTK_WINDOW_POPUP` (already true before the fix). No production helper symbol is needed by the baseline oracle.
2. **Pointer/keyboard/host:** real XTest click on the actual on-canvas thumbnail with official default single-window TRUE; self-anchor position/flip/grab, live selection visibility, keyboard continuation, Escape, outside FALSE, no rollback, thumbnail wheel no selection change. Prove stock dock child TRUE/staging separately without altering it. For that case alone, invoke the genuine native Tool Options window strategy before resource/options/parent snapshots; require mapped/viewable/sensitive stock controls and native dockable/editor/model identity. Do not force child visibility, reparent widgets or waive the original policy assertions. Do not describe programmatic selection as pointer proof.
3. **Resource model:** writable generated A/B with actual temporary `.vbr` files; A→B→A; resource spacing versus tool spacing, shape and all generated geometry; current link flags; non-generated and read-only resources; NULL/removal where model-supported. Count resource notifications for actual changes, no-op changes, external updates and retarget sync. For linked tool properties, retain enabled global-brush sharing and compare the full ordered raw notification trace/cardinality against a no-popup exact-native-setter control from identical initial state; compare selection retarget separately. Require one effective canonical-value transition and one resource notification per edit, not an unsupported single raw tool notify. This replaces the three raw-count assumptions exposed by run 22; it does not alter production propagation or weaken final canonical-value checks. No image/Undo mutations for UI-only work.
4. **No-save oracle:** serialize fixture A/B first, snapshot exact bytes and dirty state, edit A, switch to B and assert A remains dirty with unchanged bytes **at the switch boundary**. Then test Escape, outside, confirm, close/reopen, direct host destroy and toolbar destroy independently. Live values survive; disk bytes do not change. Keep ordinary dock data editors away from these fixtures so their independent autosave does not confound the test. Invoke ordinary explicit resource save as positive control; it must update bytes and clear dirty state. Use a read-only fixture to prove generated geometry does not mutate while spacing retains its legacy live-memory behavior. Do not globally alter a save vfunc to count calls.
5. **Ownership/adversarial:** stale A notify while B selected; stale controls after retarget; same-target reselect; callback-triggered selection or toolbar destruction; rapid Escape/reopen; source/context/parent destruction orders. Weak refs prove old body/host/controller targets are released when no other owners remain. Two independent horizontal toolbars and the real dock share only canonical options, never widgets. Each reopen is fresh and reads latest selection; no duplicate listeners.
6. **Regression/visual gates:** the staged lower-controls test extends the exact focused manifest from 20 to 21 without removing any prior case. Preserve all 132 prior positives, for 153 unique cases (127 compact + 26 independent toolbar-window), plus separately required adjacent gates. Require focused sanitizer with exact instrumented scope and four actual 1080×1920 screenshots: actual-entry and lower-controls bottom state at scales 1 and 2. Inspect pixels for clipping, selector visibility and preview. This is not old-runtime pixel equivalence. The original 20-case native-build receipt remains historical; native compile/relink and all runtime gates for the current 21-case source are pending.

## Known blockers and next action

Root approved implementation, including legacy press-phase and read-only-spacing details. Run 22 exposed a raw-notification cardinality assumption; run 23's corrected differential trace then exposed a genuine native linked-size ordering defect. The reviewed post-regeneration model repair retains the final current-mask invariant; exact-test RED/GREEN and native005 closure passed, and run24 normal geometry passed on C5. Run24 then exposed the missing genuine Tool Options dock precondition; its stock-case-only fixture correction has exact-source native006 normal compile/link/header and core/library-contract evidence, but still requires fresh CI validation; it has no local GUI or sanitizer acceptance. The run23 sanitizer extra-window identity remains unproven; only exact GTK tooltip type-plus-hint is excluded, with diagnostics and strict non-tooltip uniqueness retained. Other sanitizer findings remain separately scoped. Local AF_UNIX restrictions remain: do not attempt a bypass or claim local Xvfb/GUI validation. Local compilation may use `/workspace/shared/gimp-common-layer-env/environment.sh`; actual UI execution and screenshots require ordinary authorized GitHub Actions later. Root alone will publish and judge CI evidence. The new leaf and related `24.015/brush-popup-signal-owner` remain open until the named tests pass and evidence is reviewed.


## C7 Choice ownership and sanitizer partition draft

Task 308 remains **DOING**. Based on published C6
`b7760d05893b7bc7403b09f4400bf1b58ab6d189` / tree
`51e4c547520fd9b7181b8013e6e695b9eba9d163`, this draft adds only the reviewed
Choice wire ownership fix (`gimpprotocol.c`, `gimpwire.c`) and its exact four-case
test. `choice-wire-normal/receipt.json` preserves portable ordinary native007
old-provider short-read RED and fresh four-case normal GREEN (107 offsets
137–243). Normal success is not sanitizer/leak acceptance; the isolated normal
relink reused twenty unchanged native objects, not a full project rebuild.

The sanitizer job now runs every one of the21 Brush leaves individually with
fresh profiles and the unchanged540s per-invocation limit. Its strict aggregate
requires all21, preserves parent/child exits and leak diagnostics, and allows
neither skips nor suppressions. Normal full21 at both scales, baseline3, common
and prior positive gates remain unchanged. A separate immutable C5 provider
build overlays only the exact test and requires the precise wire LSan RED;
arbitrary failure is rejected. Candidate Choice4 sanitizer GREEN remains required.

The external babl leak is unresolved. No dependency, lock, suppression or
babl fix is adopted. `run25-observed-in-progress.json` is only a historical
in-progress observation of C6, not C7 results. Final full native, CI, leak and
portrait/on-canvas acceptance remain pending. No publication is implied.


### C7 raster fixture correction after C6/run25 normal failure

Run25 normal Brush full21 aborts after14 passing cases at both scales in
`noneditable-null`, with the exact `gimp_data_get_identifier` internal/file/image
identity assertion. Source inspection links registered data to tag-cache identity
lookup: the synthetic non-writable raster had none of those identities. The
four-line fixture-only correction marks that raster internal before registration.
All generated/read-only/raster/NULL assertions remain intact. There was no
backtrace; this is not evidence of stock-dock lifetime corruption.

`run25-normal-failure/receipt.json` preserves both failures and four separate
1080x1920 capture-case passes; those do not make the full21 gate pass. This
worker has not visually reviewed the pixels. Run25 sanitizer remains pending
at this observation. Native006 now covers only the historical pre-correction
include, not the new `0c5866f1` fixture: fresh ordinary compact compile/link
is pending. Babl lifecycle proof is a separate pending proposal; no dependency
change or leak acceptance is adopted. Task308 remains DOING.


### Additional bounded validation

`compact-normal-c7/receipt.json` records successful ordinary compilation of the
exact0c5866f1 fixture-bearing compact-options translation unit and relink with
the fixed Choice provider. The depfile verifies the include; other normal objects
and archives were reused. There are556 compiler warnings, no baseline warning
comparison, and empty link stderr. No test/GUI entrypoint was executed: this is
not a full C7 rebuild or runtime proof and supersedes the compile-pending note
above only for this bounded translation-unit/link scope.

The independent hosted `babl-owned-doc-proof` CI experiment is now staged under
`ci/`, with exact source/patch provenance. It has not run. It makes no production
dependency or lock change and cannot substitute for GIMP acceptance.


### Final C6/run25 result (supersedes earlier in-progress observations)

Run25 is terminal **FAILURE**, not task308 acceptance. Both normal full Brush
runs passed14 cases then aborted in the malformed raster fixture. The sanitizer
full Brush run passed14 cases then hit the540-second limit. Supplementary
protocol and geometry gates exited1 with765 bytes/6 allocations and135 bytes/19
allocations respectively; historical Choice2 passed, not the new Choice4 gate.
Common candidate and three controls succeeded. All six artifacts were verified.

`run25-terminal-summary.json` preserves exact identities, outcomes and image
review: all four Brush and16 common original portrait images were inspected.
No obvious layout regression was found in those captures; separate image/case
passes do not substitute for failed full suites. These are C6 observations, not
C7 runtime evidence. C7's sanitizer partition, repaired Choice4, raster fixture
and separate babl proof still require hosted CI execution and review.


## C8 diagnostic-only draft after run26

Task308 remains DOING. Run26 on C7 `8571efb42ea44b657651d454c3a03bea0aa702e7`
is terminal failure despite normal full21 at both scales and previous62 passing.
All21 isolated sanitizer cases ran: case14 failed the stable-geometry assertion
(observed0);20 case bodies passed but their processes failed with leak reports.
Choice4 and geometry7 were clean; protocol12 still exited1 with765bytes/6
allocations. The independent babl proof failed before lifecycle validation
because lcms2 was unresolved. Its code, job and inputs are unchanged here; no
babl/lcms repair, dependency adoption, suppression or local replay is attempted.

The shared stable-geometry helper now reports bounded timing/callback/geometry
observations, caller function/line and a last-eight failure ring. Exact geometry
equality, stable3 threshold,3-second deadline and1ms polling remain unchanged.
No raw addresses or environment are added by this instrumentation. Measurement
overhead exists: a later pass alone would not prove the cause. A separately
labelled fresh normal scale1 private-target-release run controls process state.

Two additional `/brush-exit-diagnostic/` cases run in independent fresh profiles
and processes in normal/sanitizer modes: harness-only is a no-op body sharing
ordinary GUI startup/exit; fixture-no-popup constructs and finishes the regular
Brush fixture without opening a popup, with before/after visibility assertions.
Main and shutdown remain unchanged. Diagnostic manifests/results explicitly
mark nonacceptance; leaks, exit errors, timeouts and child diagnostics still fail.
All21 sanitizer leaves and both normal full21 invocations remain mandatory.
The unchanged previous62 selector `/painter-compact-options` excludes the new
namespace automatically; no old cases are dropped or suppressed.

`run26-allocation-family-attribution.json` records saved-stack attribution:
Mesa108124bytes and fontconfig86742bytes are common to all20 leak reports, not
proof of popup causality, intentional caches or harmlessness. Five GIMP colors
(480bytes in representative stacks) have concrete source ownership defects:
overwritten subclass-init padding colors, a renderer creator-reference leak,
and an omitted color-config owned-field release. Fixes and focused normal proof
are separately pending review and are NOT integrated in this draft. Baselines
must be compared by full allocation stacks and object paths, not total-byte
subtraction; actual-entry includes interactions beyond merely opening a popup.
No global teardown, FcFini or Mesa change is introduced.

No C8 compilation, GUI, sanitizer or hosted CI has run. C7 native/normal receipts
are historical inputs, not exact C8 include build evidence. Current work is
source and synthetic/static gate validation only; final ownership changes and
fresh normal compile proof remain separate pending decisions.


### C8 reviewed ownership integration (supersedes initial diagnostic-only scope)

The independently reviewed three-line production correction is now included:
clear the existing owned padding color before subclass-init replacement; release
the renderer's creator reference after field ownership is retained; clear
color-config's owned out-of-gamut color during finalization. Global GUI shutdown
is still unchanged, so exit diagnostic baselines retain the same teardown.

`five-color-normal/receipt.json` preserves exact old config2RED/fixed2GREEN weak
release evidence. Historical old/new display output compared57 property values
identically and verified current-color lifetime; renderer6types times3rounds and
renderer lifetime passed both versions. Neither establishes release of overwritten
padding colors or private renderer colors, and no480-byte process reduction is
claimed until hosted stacks verify it. The focused Meson tests exercise only the
current provider: their PASS is not a new old/new57-property comparison.

Two supplementary targets/case manifests (config2, display/renderer2) run in
normal and sanitizer modes without GUI/display, in addition to all existing
mandatory gates. Candidate build closure is32targets; baseline remains3.
The new app test subdirectory is entered directly after existing `app/tests`
and reuses its build conventions; all26common protected files remain unchanged.
The immutable original-provider C5 control and dependency locks remain unchanged.
No babl/lcms repair is attempted. Fresh C8 compact compile/link evidence is pending;
no C8 GUI or sanitizer execution or publication has occurred in this preparation.


### C8 bounded native validation completed

`c8-native-validation/verification-receipt.json` now supersedes compile-pending
notes for the bounded scope below. It verifies all10 changed native/build inputs,
fresh Meson setup and introspection, and the exact compact translation-unit
compile/link (no compact executable entrypoint or GUI execution). Both new
focused targets compiled/linked with fresh target-specific Meson recipes and
passed all2+2 cases headlessly;57display values also match the archived old
baseline. Three changed color production objects/config provider were compiled.

Normal dependency archives/support objects were reused; this is not a full C8
rebuild or full-source GUI/link/runtime proof. Compact used its previous normal
target recipe and reused application libraries. The new target recipes substitute
those historical dependency paths and the exact changed color objects/provider.
Compact compilation reports560warnings (557mixed declarations,1missing declaration,
2misleading indentation); new color target compilation is warning-free. Snapshot
configuration reports missingGit/version metadata and existing Meson diagnostics.
No GUI, sanitizer, private-renderer-color lifetime proof or hosted CI is claimed.
Task308 stays DOING; the unchanged independent babl proof failure remains open.


## C9 selector-only CI repair after run27 positive gate

Task308 remains DOING. On published C8 `dd1c3642c6d0f96830025c0a55ac9f60e92eb465`,
the dedicated display-renderer test returned exit0 with TAP `1..0`; its strict
exact-case gate correctly rejected it. The dispatch used GLib `-p /ownership`,
but both cases are nested below `/ownership/display` and `/ownership/renderer`.
That nonrecursive selector executes neither leaf. C9 invokes the dedicated
two-case executable without a selector; the unchanged exact manifest still
requires both cases and rejects missing/extra cases, failed exits and leaks.

`run27-selector-regression/receipt.json` records the failure and bounded proof:
actual system GLib selection reproduces old0/new2; the unchanged strict checker
rejects0/accepts2; the new orchestration regression is RED on the old dispatch
and GREEN on fixed normal and sanitizer dispatch. No native production/test
source, Meson target, common/accepted307 file, shutdown, dependency, or babl
proof code/job is changed. Native C8 input identities therefore remain unchanged,
but this synthetic selector proof is not fresh hosted GIMP runtime acceptance.
Normal Brush21 passed at both scales in run27; sanitizer terminal evidence was
pending when this draft was prepared. No final acceptance is claimed.


## C10 canvas finalization and separate phase observation (review-only)

Task308 remains DOING. This candidate starts from published C9
`8cb5cd9cc345b222ae2bfed018cc34c760822186` (tree
`af422fb74bd635b66fcd07ec3af29b9daab6c616`). Run28 terminal evidence and
fresh hosted C10 validation remain pending; this is not final acceptance.

Canvas ownership is a narrow production fix: release the final owned
`GimpCanvas::padding_color` in finalize, then chain the parent. Borrowed config,
layout and all global shutdown behavior remain unchanged. The two supplementary
GTK tests cover replacement, final owned-copy release, caller-input independence,
and unrealized/realized hide-unrealize-realize destruction. They run as a whole
dedicated binary under portrait Xvfb in both normal and sanitizer modes; an exact
two-case manifest rejects zero, missing or extra tests and preserves strict exits
and leaks (120-second registration, existing3x GUI multiplier:360seconds). They are counted separately from the existing21 Brush/132 prior cases.

An independent normal-only old-provider target compiles a byte-exact C9 canvas
implementation directly, using the identical regression and other current app
archives. Compiler inputs and GNU link-map cross references must establish that
the direct historical object supplies the canvas APIs and that the fixed archive
member was not extracted. Both old leaf cases run separately in fresh portrait
Xvfb processes/profiles, requiring the precise final-copy weak-reference assertion
and SIGABRT; arbitrary aborts, missing cases, skips and timeouts do not qualify.
Only the historical control uses Meson should_fail (expected-failure wrapper
success is not child success or proof by itself). Its unchanged120-second
registration is not multiplied; both raw child failures and provider linkage
are independently checked. Candidate gates never use expected-failure semantics.
The old control is never a sanitizer pass or acceptance substitute. Native old
RED/new GREEN remains to be demonstrated by hosted CI; local GTK/GEGL development
headers are absent, and no native compilation/runtime result is invented.

The separate phase timing observation adds bounded weak-pointer-based signal
probes without changing exact geometry, stable-three readiness, the three-second
deadline, private-target checks, or shutdown. Global emission hooks alter signal
fast paths and impose overhead even when an object is filtered out. Draw's TRUE
accumulator may omit the after-handler; incomplete/nested intervals are not full
draw duration measurements. A diagnostic pass cannot establish root cause.

No common/accepted307 protected bytes, dependency lock/helper, workflow, or blocked
babl proof/generation artifacts are changed. No local GUI/sanitizer or remote
publication was performed while preparing this candidate.


## C11 precise corrections after run29 canvas gate failures

Task308 remains DOING; this review candidate starts from published C10
`148d18c1950d18eea9d5d5d43a48800eac28924e`. Run29 normal Brush21 passed at both
scales and common gates passed, but supplementary canvas checks failed. Sanitizer
terminal evidence remains pending at preparation; no final acceptance is claimed.

The provider checker correctly rejected three compiler-input paths: Meson built
the private `$src` worktree, while the caller passed original `$repo`. C11 passes
`$src`, preserving exact source-path/hash/object/API/binary checks. No basename or
same-bytes fallback is admitted. A maximum of six necessary compile-source/output
path observations identifies future attribution failures without exporting argv,
compiler defines or environment. Duplicate inputs still fail closed.

The canvas test also incorrectly equated GObject weak notification with
finalization. GTK destruction runs disposal and notifies weak observers while
a strong reference can keep memory alive. The corrected `canvas_disposed` count
is1 after destroy and remains1 after final unref. The actual owned-color oracle
is unchanged: `last_gone == 0` before the final canvas unref, then `== 1` afterward.
Both candidate and historical-provider targets compile the identical corrected
test. Production finalize code, old implementation, phase instrumentation,
strict2-case gates, normal/sanitizer settings and all original gates are unchanged.

`c11-preparation/receipt.json` retains the exact failures, compile excerpt, and
bounded proofs. The driver regression is RED on old code/GREEN on repaired code;
the provider test rejects identical bytes in the wrong checkout. A local system
GObject probe demonstrates disposal notification before final owned-data release.
These are not corrected native GTK Canvas2 or actual oldRED/newGREEN execution.
Normal run29 compile_commands was not uploaded, so no real compiler database
replay is claimed. Hosted revalidation remains required. No workflow, protected
common, accepted307, dependency or blocked babl-generation bytes are changed.


## C12 isolated after-acceptance stack diagnostic

Task308 remains DOING. Published C11 `31d6739e5f68cb2ae01b5ac1238c3c62d213e901`
(run30) established genuine normal Canvas2 GREEN and both exact historical-provider
RED assertions. The unchanged production finalizer and corrected shared regression
therefore have native normal evidence, retained under `c12-preframe-diagnostic/`.
Normal Brush21 passed at both scales and common gates passed; sanitizer terminal
evidence remains pending at preparation. This is not full task/leak acceptance.

C12 changes no native source or acceptance gate. Only after the original sanitizer
runtime and integrity checks finish, a separate nonacceptance workflow step may
install GDB from the existing official signed Debian repositories and observe one
fresh private-target-release invocation. The original driver exit bit is retained
even if handoff, installation or observation is unavailable. No permissive
sanitizer flags, leak suppressions, tracing privileges or security-policy changes
are introduced. There is a single attempt, with finite install/tool/controller
budgets; unavailability is inconclusive, not a reason to bypass restrictions.

An owned same-build receipt binds the exact source commit, executable, compiler
inputs and original sanitizer settings. The wrapper checks the exact Meson
executable and leaf arguments. It restores only the known locked dependency
setup and whitelisted original sanitizer/UI settings, then uses a new profile,
scale1 portrait1080x1920 Unix Xvfb, D-Bus and native-oncanvas session. No generic
environment snapshot or arbitrary command is trusted. GDB installation happens
after acceptance and can change system dependencies, so this diagnostic's closure
is explicitly distinct from the earlier acceptance process.

The reviewed controller launches its own GDB/inferior, never attaches, and samples
at most8 bounded stacks of32frames. It retains controlled function/file/line fields
only, no raw MI, argument values, locals, addresses, environment or core dumps.
Raw Meson/install output stays private. Inferior cleanup requires verified child
ownership/pidfd termination. Unsupported tracing, missing symbols, early exit or
cleanup failure remain inconclusive. The first matching wait-entry thread is not
automatically the failing phase; debugger pauses perturb wall-clock deadlines.
No result from this step validates the3second assertion or supplies a leak verdict.

Controller and integration tests are static/synthetic/ordinary-child cleanup only;
no local GDB/ptrace or native GUI experiment was performed. The existing fullnormal
and strict sanitizer checks, common/accepted307 sources and blocked babl job/code
remain unchanged. Fresh hosted diagnostic evidence and final acceptance are pending.


## C13 observability-only update after terminal run31

Task308 remains DOING. Published C12 `97ed2cca019bc8542a453ebcccd433977c6834e2`
finished run31 with normalBrush21 at both scales, common gates and exact canvas
normal2GREEN/old-provider2RED passing. All21 sanitizer invocations executed:
20 reached their assertions but exited1 for leaks; case14 aborted, not timed out.
The sanitizer gate and full task remain failed/unaccepted.

The separate existing hosted debugger returned eight interrupted stacks, all at
its32-frame cap, and `Cleanup incomplete`. Its report cannot identify the exact
cleanup substage or prove the failing callback/wait invocation. C13 adds only
observability to that existing controller: fixed-enum cleanup-stage results and
one sanitized entry stack plus at most8 sampled stacks, each capped at96frames
(max864rows). Thread IDs are positive decimals capped at20digits; serialized
reports are capped at1MiB, otherwise replaced by a small inconclusive report.
Fields identify truncation and selected stack-frame presence, but
`same_wait_invocation_as_entry_proven` remains false. Presence is not causality.

Cleanup behavior/order, initial2second pidfd wait,2second debugger grace and
3second final wait are unchanged. The controller180second budget, original
ASAN/UBSAN settings, first-wait breakpoint, owned-child requirements, official
package/bootstrap integration and all native acceptance gates are unchanged.
An extra entry debugger command and deeper stacks perturb observation timing;
no result validates the3second geometry deadline or yields a leak verdict.

A separate local PTRACE_TRACEME reproduction was denied and was not retried,
rerouted or moved to CI. That reproduction is not included here. This update is
not a security/access change or a replacement tracing experiment; it changes
only the already-running hosted diagnostic's bounded evidence. Local validation
uses synthetic MI and ordinary owned children, never ptrace/GDB/GTK execution.

`c13-observability/` retains terminal run31 and its structured diagnostic evidence.
No production/native source, workflow, shutdown, dependency, common/accepted307,
blocked babl artifact, sanitizer setting, or failure gate changes. Fresh C13
observation and final acceptance remain unproven.


## C14 bounded cleanup correction after terminal run32

Task308 remains DOING. Published C13 `f90d5743ec8934956e15f67fddf4541366d3ad1f`
finished run32 with the positive and common/control gates passing, while the
sanitizer and separate babl proof jobs failed. No final acceptance is claimed.
The existing hosted diagnostic verified an owned pidfd and signaled it, but its
initial two-second readiness wait timed out; it closed that descriptor before
GDB exited and was reaped. Child termination therefore remained unverified.
This does not establish survival after debugger exit or a kernel root cause.
The hosted runner has ended; there is no outstanding termination action.

C14 retains that SAME verified pidfd through existing debugger shutdown, then
performs one final bounded two-second readiness check and closes it in finally.
Only actual readiness can establish child termination; debugger exit alone is
never success. Unknown inferiors, debugger failure and final readiness timeout
or error remain incomplete. Initial signal/wait results are retained alongside
the new fixed-enum final-wait result. Initial2s and debugger2+3s waits remain;
total cleanup waits are at most9s after the180s sampling deadline.

This is a cleanup-only controller correction, not a production performance fix.
Native GIMP, geometry predicates/deadlines, sanitizer options, workflow, runner,
96-frame/864-row bounds, output cap, ownership authority and acceptance gates
are unchanged. No new PID signal, attach or security permission is introduced.
Synthetic MI and ordinary-owned-child tests pass; the delayed-child test uses a
fake debugger and injected no-op signal, not ptrace. Real GDB cleanup success
remains unproven until the existing hosted diagnostic runs this controller.

The separately denied local PTRACE_TRACEME reproduction was not retried,
rerouted or transplanted into CI. Blocked babl generation is untouched.
`c14-cleanup/` retains terminal run32 and its bounded structured diagnostic
receipts. Stack observations are not proof of a production root cause or that
later samples belong to the same wait invocation. Full acceptance remains open.


## C15 focused GimpFrame attribute idempotence, native validation pending

Task308 remains DOING. Published C14 `6a98286b36fa48f78b67591daf051a327919072a`
ended run33 with failed sanitizer acceptance. Its existing isolated debugger
proved owned-child cleanup completed; that is diagnostic cleanup, not a native
acceptance pass. The first sampled stack includes GimpFrame margin/style lookup
inside the first Brush geometry wait. Either private-toolbar or dock frames can
exist; no sampled widget/window identity resolves which. Later capped samples do
not prove the same wait invocation, repeated dispatch or an infinite loop.

Source review identifies a narrower redundant invalidation: the frame previously
reassigned an equal singleton full-range bold Pango list on every style update.
GTK invalidates label layout and notifies even for an equal assignment. C15 skips
only an equal list; different weight, extra attributes or partial ranges are
still fully replaced. Existing bold-property behavior, font/indent cache reset,
alignment, label replacement, style lookup and margin recomputation stay intact.
Global utility, margin setters, font caches, geometry predicate and deadline are
unchanged. This fix is not declared to cure the complete case14 delay.

The new dedicated native target runs all exact seven regression cases under the
existing portrait Xvfb setup, separately in normal and strict sanitizer scopes.
It runs the whole binary, not a shallow parent filter. Exact case manifests reject
zero/six/wrong cases, skip/77, nonzero exits, sanitizer and runtime diagnostics.
Existing full normal21 at both scales and all21 separate sanitizer invocations
remain required; this focused target never replaces them.

The old normal-only control compiles exact immutable C14 frame source with the
SAME regression source, applying only two target-local compiler mappings to give
old new/get_type APIs unique c14 names. No stand-in utility or source rewriting
is used. This is a bounded implementation comparison, not a complete historical
GIMP build or historical dependency parity claim. Mandatory provenance checks tie exact sources/headers/compile commands,
objects, GNU cross-reference maps and ELF symbols to both binaries. The new old
control runs after the candidate normal log exists; both raw Meson logs supply
the full actual executed environment. Only allowlisted loader variables are used
for postrun loader-resolution checks at the exact metadata-verified Meson build
working directory, preserving relative/empty path semantics; only environment
hashes are exported. Binary,
map and provider hashes must remain identical. This is exact-environment loader
resolution, not observation of a live process module; missing/malformed evidence
fails closed. No inferred intro-tests environment fallback is used. The test calls only new; canonical frame imports in old objects
or executable are rejected, while canonical production DSO exports are expected.
The unchanged real label utility comes from the verified production library.
For Linux, that library link additionally writes a map/cross-reference file;
The Linux candidate test link also writes its own map. These change link
commands for provenance only, not symbol/optimization
semantics. Existing link options and non-Linux behavior are preserved.

Only the exact unchanged-style leaf is run against old source. Meson expected
failure is insufficient: the strict control requires actual SIGABRT and the
specific count==0 notification assertion with observed3, exact executed binary
identity, and no other diagnostics. Missing display, timeout, unrelated abort or
unexpected pass all fail. Candidate acceptance never uses expected-failure mode.

Local GTK development dependencies are unavailable. Static/synthetic validation
is not native compilation or execution; oldRED/new7GREEN, actual ELF/provider
proof and strict sanitizer outcomes remain pending hosted validation. No local
GDB/ptrace experiment, security change or denied reproduction retry/reroute is
included. C14 controller, workflow, sanitizer settings and blocked babl artifacts
are unchanged. `c15-frame-style/` retains source rationale and terminal run33.


## C16 exact GLib log-domain parser repair

Task308 remains DOING. Published C15 `b671825309326bdb55571652e43ab78cc83fd92d`
run34 produced passing normal results for all seven new frame cases. The old
implementation reached the intended `count == 0` assertion with observed3,
in `assert_unchanged_update`, and exited with SIGABRT. Its source/object/map/ELF
provider precheck passed. The strict case parser nevertheless rejected both
assertion streams because GLib emitted the configured `LibGimpWidgets:ERROR:`
prefix, while the parser recognized only bare `ERROR:`. Aggregate control failed;
this is not a retrospectively accepted old-RED gate.

C16 changes only two parser regexes to admit that optional literal log-domain
prefix. Exact source filename, function, count3, selected leaf and SIGABRT remain
required. Wrong domains, extra errors, wrong assertions and all provenance/runtime
checks still fail closed. No native source, Meson target, runtime invocation,
acceptance case count, timeout, sanitizer setting, workflow or controller changes.

The retained real stdout/stderr reproduce old-parser rejection and corrected
parser acceptance. The uploaded case projection omits the original raw command
and environment, however: the bounded replay reconstructs the intended command
from the proven provider path/leaf and uses a synthetic empty environment. This
is parser-contract evidence, NOT full raw Meson/provider/environment replay or
historical native-control acceptance. The original raw JSONL is referenced by
hash but was not uploaded. Fresh hosted complete control proof is still required.

`c16-frame-parser/` preserves exact normal seven-case results, the rejected old
case, provider receipt and qualified replay. At preparation run34 sanitizer was
pending; no terminal or full acceptance result is invented. Publication must wait
for terminal run34 observation. Protected/common/accepted307 and blocked babl
artifacts are unchanged; denied tracing reproduction is not retried or rerouted.


## C17 matched pre-click diagnostic (not acceptance)

Task308 remains DOING. Run35 terminal failure established normal Brush21 at
both scales and normal frame7 plus genuine intended old frameRED, but sanitizer
leaks and case14 SIGABRT still prevent acceptance. See `c17-matched-preclick/`.

A separate ordinary hosted job builds four fresh isolated cells: accepted307
`e25a94e21efca6e6604ddac6f15570c049f421e7` and production-equivalent current
`b671825309326bdb55571652e43ab78cc83fd92d`, each normal and ASan/UBSan.
C16 changed only parser code relative to that current source. Exact shared observer,
startup harness, external locked packages, data commit, plugins and catalogs are
verified. Objects are not shared between cells. Intended source graphs differ;
raw link argv are NOT identical: only the recorded current widgets GNU cref/map
output argument is normalized as an output-only inherited recipe difference.

Twelve serial fresh-process primary samples (three per cell, fixed balanced order)
precede two fresh current original-case14 fidelity probes. All use portrait Xvfb
1080x1920, scale1, on-canvas UI, exact leaves and unchanged sanitizer settings.
The original stable-frame predicate and fixed3s deadline remain; no extended
observation can promote the primary verdict. Existing acceptance21 cases, baseline
RED controls and deadlines are unchanged. No skip77 or zero-case success is accepted.

Each run archives only its owned mutable build test output and starts with an absent
pluginrc witness and fresh profile. The actual generated plugin cache is hashed
immediately after its process, then retained in that private profile. All font and
loader inspection is deferred until after all14 measurements. Available font/config
closure is not proof of the actual live font selected. Missing or unmatched inputs,
cache, runtime provider closure or truncated evidence invalidate comparability.

The identical reviewed exec wrapper merges stderr into stdout before exec, retaining
native exit/signal. Complete measurement/TAP bytes before an exact sole LSan report
can qualify descriptive metrics as late-EMITTED-report observations only. This does
not establish allocation/fault onset, harmless leaks, or a clean measurement; native
failure remains failure independently of collector status. No patched GLib source
identity is inferred from version alone. An aborted fidelity probe leaves the whole
comparison inconclusive; primary statistics and fidelity metrics remain separate.

The separate job requires40GiB free and has180min overall,120min build and50min
measurement bounds; these are caps, not guaranteed fit. No partial mixed-build fallback
or automatic budget enlargement. Only bounded structured diagnostic receipts upload;
raw Meson/environment/private paths remain private. Four-build native correctness,
actual resources and real runtime comparability remain pending hosted execution.
No local native/GDB/ptrace reproduction, blocked babl repair, production change or
acceptance relaxation is introduced. All26 protected common files remain exact.


## C18 seal-map partition repair

Run36 never reached the six original native gates: all six exact job logs report
`Source/bundle maps overlap or self-reference config` and exit2. The separate
matched job failed its seal stage and skipped its four builds/measurements; its
redirected private seal log was not uploaded, so no additional error text is
invented. The independent babl failure is outside this repair.

C17 mistakenly added six `tools/brush-ci/matched-preclick/` inputs to the source
map although the complete gate-bundle map already pinned all six. Matching hashes
and map completeness did not imply disjointness. The existing guard correctly
failed closed. C18 removes only those redundant source entries: all six bytes and
hashes remain sealed in the complete bundle. No guard, native source, invocation,
workflow, acceptance condition, sanitizer setting or diagnostic behavior changes.

Regression tests now inspect the actual candidate's map partition and complete
bundle, and reject matching-hash overlap and config self-reference synthetically.
Every future prepublication closure must also invoke the real seal CLI on the
complete prospective/final activated candidate tree in an isolated local Git
checkout, with the actual accepted parent/constants and no mocked gate internals.
Selected hosted metadata is explicitly simulated for this static policy test;
it is not hosted/native acceptance. Retain input tree/config/helper identities,
exit/output hashes and clean-checkout readback. Exact C17 full-tree replay produces
the same exit2/no-output failure. Corrected and final activated full-tree GREEN
receipts remain external to avoid self-referential tree/evidence hashes.

Task308 remains DOING; prior sanitizer failures and C17 diagnostic native proof
remain unresolved. Protected26/common/accepted307 and blocked babl bytes stay exact.


## C19 exact public dependency-link inventory

Run37 confirms the repaired seal passed on the real host. The separate matched
job then stopped at baseline-normal build identity with `escaping-inventory-link`;
measurement was skipped. That error originates in the extracted dependency-root
inventory, not source/build-object path validation. The receipt omitted the link
name, so the actual first offending path remains UNKNOWN. Other original jobs
were still running at preparation; no terminal native result is invented here.

Metadata-only inspection of two official Debian archives, each SHA-256 equal to
the existing immutable lock, proves absolute public-file links for Python's
`sitecustomize.py`, OpenSSL's `openssl.cnf`, and its generated public CA bundle.
These are legitimate possible triggers, not proof of which triggered run37.
`c19-public-link-inventory/` retains archive/link/counterpart identities and the
actual failing receipt. No archive was installed, extracted, or executed.

C19 admits only those three reviewed link paths and literal targets under exact
locked archive identities. Target files and parents must resolve canonically
without symlinks; files must be regular. Package counterparts must match whenever
present; the generated public certificate bundle has no archive counterpart and
its observed content remains part of the all-four-cell equality proof. No generic
`/etc`, source-root, or build-root exception is introduced. Directory links to
certs/private are not traversed; their target-content closure is not claimed.
Public-file reads are bounded at16MiB with size/change checks; no private-key
inventory is performed. Unknown
outside links still fail and now identify only a bounded dependency-relative path.
Wrong literals, changed archives, escaping/symlinked parents or targets, missing or
nonregular files and counterpart/content mismatch remain failures.

The comparison is still unrun. No native source, build recipe, workflow, sanitizer
setting, timing predicate or acceptance gate changes; protected26 and blocked babl
bytes remain exact. Full-tree real seal CLI closure remains mandatory before
publication, with explicit local-simulation qualification. Task308 stays DOING.


## C20 exact public X11 color database

Run38's bounded inventory error now identifies `usr/share/X11/rgb.txt` exactly.
The matched comparison stopped before measurements. The separately failed positive
job encountered a pinned-snapshot TLS unexpected EOF before compilation; that
transport failure is unrelated and no network/security policy changes here.
Other original jobs were pending at preparation; no terminal outcome is invented.

The exact existing locked x11-common archive proves the literal `/etc/X11/rgb.txt`
symlink and its regular17394-byte packaged color database counterpart. This is
archive metadata/member hashing, not installation, extraction or execution.
Unlike the prior run37 unknown first path, the package link matches the observed
run38 relative offender. Native/runtime success is still not established.

The helper changes only two lines: the exact archive identity and fourth public
file entry in the existing policy. Every parent/final component still uses
O_NOFOLLOW; the target must be regular, bounded at16MiB and stable, equal to the
exact package counterpart, and its hash must agree across all four cells. Wrong
archive, sibling path, literal target, host symlink/escape or counterpart fails.
No general root/directory allowance, private content traversal, suppression,
production/build/workflow/native gate or timing change is introduced.

`c20-x11-public-color-database/` retains the real failure, exact archive proof and
whole-lock archive symlink research summary. The summary explicitly distinguishes
verified inspections from any unresolved archives and lists additional metadata
classes. All560 archives were hashverified and scanned:3541 link records,254
absolute targets and zero lexical relative escapes; zero unresolved archives.
The sole additional class is tzdata localtime -> /etc/localtime. NO new exception
and NO host-target inspection is adopted: existing canonical /usr,/lib allowance
may handle standard resolution; otherwise the bounded failure remains. Host
resolution is explicitly UNPROVEN. Observation is not permission: no other
file-class exception is adopted, and metadata is not runtime-content closure.
Full-tree real seal CLI proof remains required before publication. Task308 DOING;
protected26/common/accepted307 and blocked babl bytes remain unchanged.


## C21 bounded matched build diagnostics

Run39 passed both baseline-normal and current-normal build identities and their
after-build overlay checks, demonstrating progress beyond the earlier RGB-link
blocker. The next baseline-sanitizer cell failed before its identity receipt.
The public status only says four-builds/exit1; private configure/compile logs
were not retained. The exact failing command and cause remain UNKNOWN, not a
proven production or sanitizer regression. None of the14 measurements ran.

Preparation now records finite per-cell stage checkpoints and preserves the
original build-cell shell exit at the checkpointed stage. This is not necessarily
the underlying tool exit: process-substitution target selection can fail before
the count check, and checkpoint collection can fail before its stage tool starts.
The companion collector reads only a bounded
64KiB tail from an exact owned log path, exporting fixed categories and at most
32 hashed tail lines, never arbitrary prose, source snippets, paths, arguments
or environment. This conservative classification replaces unsafe blacklist
redaction. Missing, unsafe, changed, truncated and unclassified evidence stays
explicit. Categories are observations, not root-cause attribution. A running
checkpoint after uncatchable termination does not invent a completed exit.
Collector failure never turns a failed native tool into success.

All changes are before measurement, not between timing samples. Production,
build arguments, dependency policy, source overlays, sanitizer settings, timing
predicates,14-run order, original acceptance gates and workflow are unchanged.
Synthetic checks are not native proof; real complete-tree seal CLI checks remain
required before publication. Run39 original sanitizer was pending at preparation.
Task308 remains DOING; protected26/common/accepted307 and blocked babl unchanged.


## C22 diagnostic baseline build compatibility

Run40 passes both normal build identities, then fails baseline-sanitizer during
configuration. The retained43-byte line hash exactly confirms the public message
`Program gimp-console-3.0 gimp-3.0 found: NO`. The final238-byte Meson error text is
not recovered; its private random-root prefix is unavailable and known relative
format variants do not match. We do not invent a full historical error transcript.
Source inspection finds the exact e25 sanitizer branch disables GIR, while the
old root requires both runnable host binaries and GIR, incorrectly falling into
native-GIMP lookup. Current root already decouples these conditions. Prior ld:
classification matched the suffix of meson.build:, so those labels did not prove
a compiler/linker fault.

Only private diagnostic baseline worktrees receive an exact pinned two-hunk root
Meson compatibility overlay, for BOTH normal and sanitizer. It reproduces current
runnable-binary/GIR handling (including the guarded macOS GIR references) without
transplanting the current color-test subdir. Current roots remain untouched.
Original/after root hashes, exact source revision, owned path, arm and policy are
checked; unexpected/missing/extra edits fail. Accepted307 and all production C/C++
remain unchanged. This is baseline production source with MODIFIED BUILD METADATA,
not an unmodified baseline build comparison. The arm-specific descriptor and this
qualification are retained in top-level comparison results, not hidden in docs.

Native build options and sanitizer settings remain exact; no installed substitute
GIMP, weakened sanitizer, object mixing or recipe-normalization relaxation is
introduced. The14-run order, deadline predicates, observers, original acceptance
gates/workflow and blocked babl remain unchanged. The separate run40 common canvas
mask failure is not integrated or waived here.

Build diagnostics now use anchored compiler/linker headers and exact fixed public
native-GIMP lookup/error templates. Only finite enums and hashes are exported;
no free-form identifiers, source text, paths, commands or environment. Source and
filesystem synthetic tests are not native proof: local Meson executable/module
is unavailable, so no actual Meson configure/compiler/GUI pass is claimed. Exact
full-tree seal CLI proof remains required. Run40 was pending at preparation;
Task308 remains DOING with all26 protected/common files unchanged.


## C23 canvas mask-detach draw window

The model can lose its mask before the low-priority cached row refresh. Drawing
now requires both a current model mask and identity with the retained cached mask
before querying edit-mask state or drawing its icon. This source-supported window
explains the run40 assertion; no historical native backtrace is claimed. Run41's
unchanged-source pass does not disprove an intermittent window.

The existing test retains all original assertions and all107 case IDs. Immediately
after detach, before draining the main loop, it checks model mask NULL, cached
strong mask not finalized, and synchronous red layer pixels; the original drain
and finalization count1 assertions remain. The two changed production/test files
were not among the26 protected common files; accepted307 remains immutable.

Supplementary Linux proof targets reuse the same normal/sanitizer build and exact
patched test/support sources. Normal runs fixed focused1, fixed full107 and an
exact old direct tiles provider expected to SIGTRAP at the intended mask assertion.
Sanitizer runs fixed focused1 under unchanged strict settings. All10 public APIs,
actual compile objects, maps, ELF providers, headers, binary and loaded-library
resolution replay are checked, with exact raw Meson environment/cwd and pre/post
hashes. Only the old normal test is expected-fail; unrelated failures, leaks, skips,
zero cases and unexpected passes are rejected. This is bounded provider substitution,
not full historical build parity or live-module observation. Native proof remains
pending; local synthetic/static and source-seal checks do not establish it.

Focused proof failures retain bounded per-run status, log hashes and finite labels,
not raw prose or environment. Separately, the run41 matched baseline-sanitizer
object-sanitizer-mismatch now exposes bounded relative object/source identity,
recipe hash, row position and finite sanitizer-option labels. The original
predicate, verified:false and failed exit remain unchanged; these are recipe
observations, not instrumentation proof. No compiler option or matched source
revision/order changes are introduced.

Existing107/common and Brush gates, deadlines, sanitizer policy, workflow, protected
files and blocked babl are unchanged. The matched four-build/14-run experiment and
its frozen production inputs/order are untouched. Task308 remains DOING.


## C24 mask provider failure observations

Run42 linked both mask proof executables but rejected the fixed provider at
`wrong-archive-api-provider`, before focused native tests. Exact archive member
bytes, object/ELF definitions and test/support map anchors had passed. The saved
artifact contains no GNU map/ELF witness: a thin-archive representation mismatch
is a hypothesis, not a demonstrated cause, and historical witness replay cannot
be claimed. No production correction is inferred from this proof-parser failure.

Before the unchanged provider guard, bounded observations now cover all10 expected
API entries in both maps: finite parse/representation labels, line hashes, known
build-relative identity labels and comparison booleans. Unknown tokens remain
hashes/categories; no arbitrary map prose, addresses, paths or environment is
exported. Both available target maps cover all10 expected API names; first2
matching lines per API are sampled, and a count of3 means >=3. Successful
receipts also contain this bounded witness, not a complete raw map export.
Zero observed entries with unreadable status do not prove absent symbols.
Synthetic thin-archive forms exercise diagnostics and remain rejected
unless the original strict predicate accepts them. No alternative-provider
acceptance or blanket fallback is introduced.

All native source, compiler/linker settings, exact oldRED/fixedGREEN predicates,
107-case gate, original acceptance workflow, matched experiment and blocked babl
remain unchanged. Native mask proof is still pending. Task308 remains DOING.


## C24 exact native PNG generator role

Run42 identifies the built colorsvg2png object as having no sanitizer option.
Exact pinned upstream gimp-data source deliberately declares this executable
`native: true`, `b_sanitize=none`, `install: false`. Its observed normalized
compile recipe is identical in the two normal cells and failing baseline
sanitizer cell. This is a build-time PNG generator, not the tested host program;
actual target metadata was not retained from run42 and is not inferred by name.

The new proof recognizes ONLY this exact pinned source/declaration/recipe after
actual Meson build-machine target and source binding, Ninja object/link ownership,
and generated-resource checks. Unknown or ambiguous metadata fails closed. Its
object/executable must not enter host link inputs. The two required generated PNGs
and two generated resource C files are hashed and compared across all four cells;
the generated host C objects retain the strict mode-specific sanitizer rule.
Every other built object retains its original rule. This does not alter compiler
options, upstream production sources, dependencies or the four-build/14-run order.
Graph/command provenance is not live execution observation; hosted proof remains
pending. No arbitrary native-target or name-only exemption is introduced.


## C25 generator Git-output delimiter

Run43 stopped at generator-submodule-mismatch. The new role guard compared raw
LF-terminated `git rev-parse` stdout to the bare pinned40hex commit, while the
existing source identity and shell checks already normalized that same pin. The
new unit fixture incorrectly mocked stdout without LF. Normalize exactly one
terminal LF; do not strip arbitrary whitespace or change the pinned revision.
Wrong hashes, extra lines, double LF, CRLF and surrounding whitespace stay RED.

A read-only command against the fetched exact gimp-data object produced41bytes
(hash plus LF). Replaying those actual bytes makes the old guard RED and corrected
guard GREEN with all remaining role checks exercised by synthetic fixtures. This
uses FETCH_HEAD because that evidence repository has no checked-out HEAD: it is
not retained run43 HEAD output, a full historical CI replay or native proof.
Mismatch receipts now expose expected SHA, observed SHA only if strict40hex,
output hash/capped length and finite shape, never arbitrary stdout/environment.

All other generator source/target/object/link/resource checks and content pins,
mask provider gates, native sources/compiler flags, matched inputs/order, workflow,
26protected files and blocked babl remain unchanged. Hosted proof is pending and
Task308 remains DOING.


## C26 combined Git delimiter and thin-archive provider proof

C25 was reviewed and sealed but never published or accepted; its frozen proof is
preserved. This candidate combines that exact one-LF Git-output correction with
the separately reviewed mask-provider serialization repair, from published C24.

Run43's bounded actual witness records a thin archive and all10 fixed API defining
entries as the direct exact compiled tiles object, not archive(member) spelling.
All10 old entries identify the exact old fixture. The wrapper-only fixed parser
rejected this form. The selected ar member SHA hashes its NAME, while the compiled
object SHA hashes BYTES: their difference is not an identity failure. Existing
checks already require one ar member resolving to the exact compiled object and
`ar p` bytes equal to that object's bytes before generating the witness.

Allow direct defining-object spelling ONLY for an actual thin archive and exact
compiled provider path. Archive LOAD/OUTPUT, unique member and byte equality, all10
API definitions and the old direct-provider checks remain required. Regular
archive rules do not change; wrong objects, duplicates and ambiguous evidence
remain failures. No basename-only or arbitrary provider fallback is allowed.

Replay uses the actual bounded run43 witness to reconstruct representative map
entries. The raw full map/ELF was not retained, so this is not historical full
provider acceptance or native oldRED/newGREEN proof. All native/build settings,
26protected files, existing strict case/assertion/leak gates, matched source/order,
workflow and blocked babl remain unchanged. Task308 remains DOING.
