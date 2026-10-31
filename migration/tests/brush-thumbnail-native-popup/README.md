# Brush actual-thumbnail native popup — acceptance pending

Bounded leaf: `29.010/brush-thumbnail-native-popup`. Related signal-owner obligation:
`24.015/brush-popup-signal-owner`. Neither this leaf nor its parents are complete.

Accepted immediate pre-Brush baseline: `e25a94e21efca6e6604ddac6f15570c049f421e7`
(tree `8634eeef49c81aaa5c25192cb76d749ff680683f`).
Original isolated implementation base: `ef6640451c1c7ac50a7a354e8b57ecfe1ebc8bbb`.
Legacy source: `afa43fae3e920210146abed514f136fd49f671b5`.

## Scope

The actual registered horizontal Brush thumbnail now has a fresh, self-anchored
native popup with a same-context grid, tool size/aspect/angle, resource spacing,
256-pixel preview and generated brush geometry. The adjacent modern Brush menu,
Dynamics, generic compact adapter and ordinary vertical dock are not converted.

The resource controller uses canonical live setters, not a `GimpDataEditor`:
there is no save on resource switch or disposal. The normal dock editor's autosave
is unchanged. Generated geometry is writable-only; legacy resource spacing still
permits memory edits on disk-read-only brushes and retains the native setter's
non-dirty behavior. Pointer opening remains the legacy press phase; no global
GTK class is modified. GIMP 3 link flags remain authoritative instead of the
old unconditional tool-size reset. These are not claims of full old UI identity.

## Required acceptance evidence

- Focused C11/C++14 production/test compilation, genuine header inventory/probes
  and final-source application/test linkage.
- Final actual-entry test overlaid onto the exact pre-change base must reject
  the stock popup for missing extended content/canonical wiring. A native window
  alone is insufficient because the stock selector already uses one.
- `/brush-thumbnail-native-popup` under official default single-window TRUE,
  with actual XTest thumbnail press/release and grid selection distinguished
  from programmatic model/lifetime probes.
- Exact `.vbr` bytes and dirty flags unchanged at the resource-switch boundary,
  Escape, outside, confirm, host/toolbar destruction and reopen. Ordinary
  explicit factory save must still write as the positive control.
- Generated/resource/tool geometry, current link flags, read-only/raster/NULL,
  A→B→A/stale observer, notification cardinality, close/reentry/fresh-body/ref
  balance, independent peer/dock and image/Undo invariance.
- Final aggregate including all existing compact/independent toolbar and
  accepted popup suites; focused sanitizer with exact instrumented scope.
- Actual 1080×1920 scale-1 and scale-2 portrait captures, visually inspected
  for control reachability, preview and grid clipping. No old-runtime pixel
  equivalence is inferred.

Local AF_UNIX restrictions prevent GUI/Xvfb execution here; they are not bypassed.
Ordinary authorized GitHub Actions will supply the runtime/visual gates. Compilation
alone is not runtime acceptance. The root task alone owns publication and CI review.

See `implementation-contract.md` for architecture, lifetimes and deliberate
old/current differences. Exact evidence files and hashes will be recorded only
after each check actually runs; there are no placeholder success results.

## Current accepted-baseline integration (runtime acceptance pending)

Root approved staging the test-only lower-controls extension on 2026-10-25.
The reviewed source delta and matching progress row (18 files) were integrated
on accepted task 307 at the baseline above. `lower-controls-staging.json` records the current source hashes,
original checkpoint identities and pending gates. The original frozen checkpoint
manifest remains `7c0352d399d73c43746827a0b1455b1a8b41dd42b880e5422a512e0cdcf93388`;
its source files have not been relabeled as new runtime evidence. Only this leaf's
pending row was inserted into current `tasks.md` and `progress.tsv`; accepted
task-307 documentation and common-popup production/tests were preserved.
The original 20-case native-build receipt below is historical and unchanged;
its test-object/link result does **not** cover the current 21-case include.
The integrated normal-build receipt `native-build-integration-001.json` records
11 executable links and 455 header probes before the first CI attempt; it is not
GUI acceptance. The first published candidate and its failed runtime attempt
are recorded below. This leaf remains incomplete.

The added `/brush-thumbnail-native-popup/lower-controls-actual-input` uses the
actual toolbar entry, physical wheel input on the outer scrollbar if content
overflows, and physical numeric-entry edits of radius, spikes, hardness, aspect
and angle. It checks full control/preview visibility and canonical edits without
implicit save, image/Undo or dock changes. Real descendant hover reveals a GTK
overlay scrollbar before strict hit/wheel input; no adjustment or overlay state
is directly set. If content naturally fits, visibility is required without
pretending scrolling occurred. Owned child/adjustment/frame-clock observations
and guarded waits do not replace runtime lifetime validation. Native thumbnail
keyboard activation and DND remain unverified and are not added to this scope.

### Scrollbar semantic-entry readiness revision

The current test-only revision also requires the physical pointer to hit the
actual scrollbar and, when that scrollbar has GTK's `overlay-indicator` class,
requires its `hovering` class **before** the existing stable-frame/strict aim.
The original wheel gesture, thresholds, numeric-entry path and pointer assertions
are unchanged. Preparation sends only real motion and dispatches pending events,
bounded to three seconds; it adds no sleep, button, CSS or adjustment setter.
Host/child liveness is checked before geometry/style reads and after dispatch.

This addresses a source-backed analogous readiness risk, not an observed Brush
failure: common-popup run 11 demonstrated stable collapsed-bar frames before
hover expansion. GTK's `indicator_set_over()` updates the hovering class and
queues resize; see the [GTK 3.24.49 source](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkscrolledwindow.c)
(`indicator_set_over`, `check_update_scrollbar_proximity`). Static review and
C11 syntax comparison passed for that revision. Run 20 later exercised part of
the flow, with the failures and limited lower-controls result recorded below.

## Automatic CI gate (wired; acceptance pending)

`cases.json` freezes the **21** focused case IDs registered in the existing
`painter-compact-options` executable. After a clean native build, run that
registration with `--test-args '-p /brush-thumbnail-native-popup'` at scale 1 and
scale 2 in ordinary Xvfb `1080x1920x24 -nolisten tcp` plus D-Bus sessions and fresh
application profiles. `tools/brush-ci` supplies the focused case, Brush-specific baseline and four-image
validators. The additive workflow jobs retain all three common jobs and use
automatic runtime seals from committed source/bundle hashes, exact requested
`GITHUB_SHA` equal to clean checkout HEAD, and immediate accepted parent e25.
No externally provisioned runtime-pin file is required.

Capture actual-entry and lower-controls-actual-input separately at each scale:
set only `GIMP_BRUSH_POPUP_CAPTURE` for actual-entry, or only
`GIMP_BRUSH_POPUP_BOTTOM_CAPTURE` for lower-controls-actual-input. Clear both
variables between invocations. Required caller-selected outputs are:

```text
brush-thumbnail-actual-entry-scale1.png
brush-thumbnail-actual-entry-scale2.png
brush-thumbnail-lower-controls-scale1.png
brush-thumbnail-lower-controls-scale2.png
```

All **four** must be raw 1080×1920 PNGs, with actual scale and case markers, and
must be decoded/visually inspected for grid/preview identity and lower-control
clipping. A flag, PNG header or file's existence is not visual acceptance.
Accepted task 307 has its own recorded run-18 evidence and 16 reviewed images.
On the final Brush candidate, rerun that unchanged common gate: overlay paint
gate 6 cases at both scales, common popup 58 cases at both scales / 16 images,
and all 13 adjacent registrations (360 cases in the accepted run). These four
Brush images do not replace any common captures or establish those reruns.
The historical common 17-file manifest stays
`04b3053c24509ad643b6782197bd80abcf5a74582c3e474500a01323e2c87e9f`.

Preserve the **132** prior positives: six separate compact invocations using
one prefix each (`/painter-compact-options`: 62, `/standard-toggle-popup`: 17,
`/transform-native-popup`: 8, `/bucket-native-popup`: 6, `/ink-native-popup`: 6,
`/clone-native-popup`: 7), plus all 26 `painter-toolbar-window` cases unfiltered.
Do not remove legitimate current-code `baseline-*` cases or add skips. One
prefix per invocation avoids cumulative TAP plans from repeated `-p` options.
The final exact unique total is **153 = 127 compact + 26 toolbar-window**,
including 21 Brush cases, not 153 cases per focused invocation. Other adjacent
accepted gates remain separate and must not be weakened.

For the baseline negative, overlay only the final `.inc` and its include/register
calls into the exact pre-change test main at accepted baseline
`e25a94e21efca6e6604ddac6f15570c049f421e7`. Each runtime seal binds the exact
published candidate, not a prospective SHA embedded in its own source commit.
The test deliberately finds the old
stock Brush thumbnail without any new factory symbol/name requirement and first
asserts extended content/canonical wiring after release. Its independent saved
press-boundary assertion also rejects a clicked-only extended popup. Verify the
failure is that intended assertion, not build/startup/input setup. Record the
baseline source, overlay and final-test hashes. No baseline production code may
be copied from the candidate to make the oracle run.

## First task-308 CI attempt — failed, 2026-10-25

[`ci-attempts.json`](ci-attempts.json) preserves all six terminal job results,
original artifact IDs/ZIP hashes and inspected-image observations for candidate
`b6eee4ea1f944010e8f009e040cdb4d7561b3e1e`, tree
`1cc25159b8ba67e195bfdc98ab695badf385abfa` (immediate parent e25).
[Workflow run 20 / attempt 1](https://github.com/seagetch/gimp-painter/actions/runs/40202648847)
is the **first Brush task CI attempt**, not its twentieth attempt.

All six automatic seals passed. The unchanged common candidate gate passed
6 overlay cases × 2 scales, 58 common cases × 2 scales, all 13 adjacent
registrations / 360 cases, headers/source checks and 16 inspected raw images.
The common-306 and Brush-stock missing-extended-body controls produced their
intended REDs. The historical controlled-order job remains diagnostic-only.

The Brush positive job failed: scale-1 actual-entry reached the extended popup
but failed the thumbnail lookup assertion; scale-2 opening failed to find its
host. The separate lower-controls scale-1 case passed real numeric edits and
no-save assertions, but naturally fitting content generated no wheel events.
Only its one PNG out of four required Brush captures was produced; the visible
selector title was `(None)`. The prior-132 aggregate also failed: 127 cases
passed, Transform baseline-perspective failed, and its remaining four cases
were not reached. These partial results do not satisfy 21/153 or visual acceptance.

The sanitizer job failed during native noGI Meson configuration before build
or runtime. The separately reviewed native-path fix has real configure-only
proof in `native-no-gi-config-001.json`: sanitizer/noGI chooses the in-build main
binary with no GIR dependencies, while normal GI/console chooses the in-build
console with all four GIR/typelib targets. Genuine cross fallback is unchanged.
That fix and subsequent fixture/title/input work were **not** present in run 20;
all final-source native, behavioral, negative, sanitizer and image gates require
fresh evidence. No task acceptance is claimed.

## Reviewed run-20 source repairs — awaiting new runtime evidence

The Brush test now queries the thumbnail's actual `brush` property: the shared
finder's NULL argument means an unbound property, not a type-only wildcard.
The real main window is fitted to the logical portrait monitor before tool
setup, rather than relying on a window manager to honor maximize. Initial
painted geometry and the actual native pointer hit are checked before the
unchanged physical press/release; the stock missing-body RED remains first.

The raw selector's visible name is now explicitly synchronized with the current
canonical brush description, including NULL, retarget, rename and dimensions
after generated-mask regeneration. Its observation belongs to the existing
scoped Brush controller; no autosaving editor is introduced. The existing two
reentry cases additionally switch context or destroy the toolbar synchronously
during the title write. This visible description is an explicit current UI
choice, not a claim about the old-2.8 title layout.

Only the fresh Brush host now constrains the old preferred/edge-flipped position
to the monitor **before mapping**. A fitting 477-pixel form can otherwise flip
off the left edge of a 540-logical-pixel monitor. The retained host, shared popup
placement function and all protected common tests remain unchanged. The Brush
anchor oracle independently computes the constrained position and keeps full
capture-bound assertions; it does not reposition the invoker to manufacture a
passing screenshot.

The prior Transform fixture fits its real window before tool selection/state
snapshot and parks the real pointer away from the toolbar before a first
`GTK_WIN_POS_MOUSE` operation dialog can appear. Native hit/grab and operation
dialog geometry are recorded. Dialog occlusion is a source-supported hypothesis
for run 20's Perspective failure, **not a demonstrated cause**. No operation
dialog is hidden/canceled and no original action or parent assertion is removed.

At the run-20 repair stage, all 21 Brush registrations, all prior 132 case IDs
and original Brush/Transform assertions were retained. Four translation-unit syntax checks and the exact
accepted-base test overlay with its **old** Transform include pass; these do
not constitute a native link or runtime pass. `lower-controls-staging.json`
records current source identities. Final normal build, both-scale behavioral
tests, the new-test baseline RED, sanitizers and all four new images remain
required. Previous native/CI receipts retain their original source identities.

## Run-22 notification oracle correction — test-only, requalification pending

Candidate `0deda7a442c3f70dc66c746633c6a2011d09e983` reached the same
`geometry-retarget-links` assertion after nine passing focused cases at both
scales and under sanitizers: raw `notify::brush-size` count was 12, not the
assumed 1. This result is not a completed 21-case pass. Separate actual-entry
and lower-controls invocations at both scales and all 132 prior positives
passed on that candidate; those results do not execute this newer oracle.

Native global-brush sharing forwards linked updates from each brush-core tool
through the options manager. Its unconditional GObject property assignments
can notify even when the value is unchanged. The source therefore does not
support treating one resource edit as exactly one raw tool-property notify.
No production controller, property propagation or global-sharing policy is
changed by this correction.

The geometry case now records a no-popup control using the exact native radius
setter, and a separate native A→B retarget control. It restores identical
selected brush, resource value, link flags and tool value, reopens through the
case's existing native-widget clicked helper, and compares the complete ordered
raw notification values and cardinalities. Both paths must produce exactly one
effective canonical-value transition; a resource edit must produce exactly one
resource-property notification. The four other link flags use the same strict
native-control comparison. No count is hard-coded to 12, global sharing stays
enabled, and final canonical geometry/reset-policy checks remain mandatory.

Exactly three old raw-count assertions (size counts 1/2 and the other-link
count 1) are replaced by this stronger differential oracle. All other prior
Brush assertions, all 21 case IDs, all prior-132 tests, and the physical-input
and screenshot cases remain unchanged. This is not additional XTest coverage.
Syntax-only checks, including the accepted-A test overlay with its original
Transform include, pass. Native003 and new normal/negative/sanitizer CI evidence
are still required; neither source analysis nor the prior failed run is a
runtime confirmation of the native fan-out explanation.

## Run-23 model ordering fix and tooltip diagnostics — acceptance pending

Candidate `f643820a556119fcb5d2fdd224c6bd3d75aa3600` established the native
fan-out in the normal geometry case: both the no-popup native control and popup
edit produced the same twelve raw size notifications, all with value 63.
The next invariant correctly failed because the regenerated brush mask was
27×27. The final current-mask assertion is **retained unchanged**; matching a
stale native control is not sufficient for linked-size correctness.

Generated radius setters notify before rebuilding their mask, while the native
identity transform-size fast path reads that mask. PaintOptions now observes
completed generated-brush `dirty` events instead of writing the stale size on
radius notification. Its cached completed radius distinguishes radius edits
from unrelated dirtiness; a selected-while-frozen brush records pending thaw.
The callback requires the same current brush, waits while frozen, and advances
its cache before any reentrant size write. Selection resets, GUI/link gates,
other linked properties and global sharing remain intact. Retarget/disposal
disconnect the observer. No forced size write is added to the popup.

The existing headless `painter-brush-geometry` `/painter/geometry/defaults` case
now covers direct setters, queued GObject notification, freeze/thaw, link-off,
stale/retargeted brushes, nested edits, reentrant freezing and disposal, under
both native and legacy geometry. Its seven case IDs remain unchanged. The
**exact new test against the old model** failed for the intended 63-versus-27
assertion before the reviewed model fix was adopted; it did not fail at setup
or attempt GTK/GDK display initialization. GREEN/full native005 and fresh CI
remain required for this revision.

Separately, the run-23 sanitizer invocation passed actual-entry, then found
multiple newly visible top-level windows during the no-save-escape reopen.
The old log did not identify their types, so a tooltip cause is **unproven**.
The fixture now logs the new-window set before asserting uniqueness and applies
the already accepted common-test exclusion only when **both** the exact type
is `GtkTooltipWindow` and the GDK hint is `TOOLTIP`. Stock `GimpContainerPopup`,
application dialogs, menus and every other extra window still count and fail
uniqueness. No tooltip is disabled/hidden and no wait or input rule is relaxed.

The bounded Brush/model payload now contains 22 files; separately reviewed
channel/choice lifecycle fixes and their native/CI receipts are outside that
manifest. This section does not claim those sanitizer findings are resolved.

## Run-24 stock-dock fixture precondition — acceptance pending

Candidate `da070df3717723e75f0757113c2da2768949d676` passed the first twelve
focused cases at both scales, including linked geometry and the other link
flags. It then failed the stock-policy case's unchanged mapped-button
precondition, before opening the stock popup. The native Tool Options manager
intentionally creates a cached options GUI even when its dockable is not open;
obtaining that GUI does not establish a selected, mapped dock. The logs do not
identify a particular hidden ancestor or prove a production mapping defect.

Only the stock-policy fixture now opens `gimp-tool-options` through the genuine
window strategy/dialog factory after selecting Paintbrush and before taking
resource, option and parent snapshots. It waits for the real stock button to
be mapped/viewable, logs bounded ancestry diagnostics, and requires native
Dockable/ToolOptionsEditor ancestry, canonical options identity and sensitivity.
It does not reparent widgets, force child visibility, directly select a notebook
page or construct an artificial window. Cleanup destroys only a dockable created
by this fixture and balances its retained reference. All 399 C5 assertions
and 21 IDs remain; the stock cancel/confirm, canonical/staged-context, ownership
and no-save body is unchanged apart from choosing this prepared fixture. Other
cases still use the original fixture path. This adds no XTest coverage.

The preceding native005 receipt records normal compile/link/header closure and
model7/protocol12/choice2/enum passes on its exact source union. Run 24's normal
geometry case also passed the retained final current-mask invariant. Those
receipts are historical inputs. The preserved native006 receipt matches all 35
reviewed source pins, including this exact fixture (e3c71187). Its normal compact
translation unit compile/relink and 19-root-target closure passed; 455 header
checks and normal model7/protocol12/choice2/enum contracts passed. No fresh full
project rebuild, GUI execution or sanitizer acceptance is claimed. Fresh
normal/negative/sanitizer/visual CI checks remain required.

Run 24 (40215992854, attempt 1) is terminal failure: Brush positive and
sanitizer jobs failed; common candidate and the three controls passed.
`ci-attempts.json` preserves the exact C5 failure and parent-exit leak evidence;
it is not runtime evidence for this C6 fixture. No Choice, dependency or
partition draft repair is adopted here; task 308 remains DOING.

Run 24's sanitizer log explicitly identifies excluded `GtkTooltipWindow` windows
with the TOOLTIP hint while the actual Brush host remains counted. Nine cases
passed before the aggregate 540-second timeout; the next geometry case had not
reached its raw trace diagnostic. This is not a sanitizer pass or proof of a
model stall, and does not retroactively identify run 23's unlogged extra window.
Timeout and companion host-leak investigations remain separate; this stock-dock
repair changes neither timeout nor sanitizer policy.

## Historical 20-case build-only checkpoint — 2026-10-25

`native-build.json` records the frozen 20-case source hashes and actual completed
checks: three clean production object compilations, test-object compilation,
focused executable relink, and 218 C11 + 237 C++14 genuine header probes with
validator PASS. The focused link reuses unchanged, source-compared support
archives; it is **not** a clean full Meson build. The new test include emits 87
mixed-declaration style warnings under the existing C11 compilation flags.
Established header ignored-qualifier warnings are disclosed in the report.

`retained-controller-comparison.json` proves the existing retained-body host
implementation is byte-identical to the base after excluding two new include
directives; fresh-view code is appended. This source comparison is not a runtime
regression pass. `historical-header-registration.json` records the separate
historical/current-duty registration check, not test execution.

All 20 behavioral tests, final-test negative, sanitizers, full runtime aggregate
and portrait screenshots remain **unrun** in this checkpoint.

## Historical first 21-case normal-build checkpoint — 2026-10-25

The separate `native-build-21-normal.json` receipt has SHA-256
`a94c3f1b42e0430d6021452ace0f6c2bdfdb75cc14eb20a9385e4f9da08645db`.
It records a fresh, normally configured focused Meson build and 455 header
probes for the prior 17-file manifest
`7eded283556ed285c732cf3afe8364eff7b4c9141f5fc33c6ac8d80f84816dd4`,
whose test include was `1a094ca8607b0b4c386b0fecbd85c2eed821d25c64115f8699965513c4c7e770`.
That receipt, source snapshot and Library v1 remain historical and unchanged.
They do not cover the subsequent scrollbar semantic-entry test revision and
do not establish a full-project aggregate, accepted-307 rebase or GUI success.

## Historical hover-readiness normal build — 2026-10-25

The separate `native-build-21-hover.json` receipt has SHA-256
`0310350b5f31c24c53944f4207d99511c19bb4afa00a8123a7d5b8ac436b04f8`.
It records normal incremental compilation/relink and 455 header probes for
the frozen 17-file checkpoint `7c0352d3…` with test include
`3530073d40848eb371918c8c2be979fcf2404ef47c8708baa9d709498ae9f77e`.
That receipt is pre-rebase evidence on original base `ef664045…`; its normal
build reused the earlier fresh configuration. It is not accepted-baseline
build, GUI, negative, sanitizer, visual or full-project acceptance evidence.

These historical checkpoints do not cover later run-20 fixes. Final-source
normal linkage and all runtime/negative/sanitizer/visual gates remain required
after those changes; syntax checks and static review do not replace them.


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


## C27 Meson metadata and matched-log contract repair

Run44 passes normal Brush/common gates and the C26 thin-archive provider identity
checks. Mask proof still stops before native execution because pinned Meson1.7
intro-tests does not export should_fail. Pin the complete unchanged declaration
source plus actual version/schema/root/index. Preflight explicitly proves only
source declaration; exact old EXPECTEDFAIL/SIGTRAP/single mask precondition and
fixed OK/0 remain indispensable after execution, with all focused/full gates.

All four matched builds now complete, but no valid comparison exists: Meson
appends -python3 to the logbase when the unchanged wrapper is selected, while
the collector read the unsuffixed nonexistent file. Use the exact emitted path,
retain valid phase order after read-loop EOF, and emit only bounded regular-file
SHA/size/allowlisted verdict receipts. Raw environment/stdout is not published.
Recognize only exact Meson1.7 default normal sanitizer option strings; explicit
strict sanitizer options and LSAN absence remain unchanged.

Both contracts were replayed using actual Meson1.7 minimal projects, including
OK0/FAIL7/SIGABRT-6 preservation and explicit sanitizer options. This is not GIMP
native or hosted14 proof. Production sources, all26 protected files, deadlines,
case counts, assertions, leak policy, matched inputs/order and workflow are
unchanged. Task308 remains DOING. See c27-meson-contract/receipt.json.


## C28 bounded inspection diagnostics

Run45 normal Brush/common gates pass, but paired mask tests never run because an
inspection fails. Its exact failing stage/output was not retained. Add only fixed
stage, native code, bounded output sizes/hashes and six finite stderr categories;
keep every existing rejection. Do not infer a loader cause or a native fix.

A separate proven downstream environment mismatch is corrected: require exact
pinned Meson1.7 injected normal ASAN/UBSAN/MSAN defaults, existing strict sanitizer
values and no LSAN override. A real tiny Meson fixture verifies that contract,
not GIMP native behavior.

Matched12 primary metrics are present, but all14 runtime inspections remain
unverified with inspection-unavailable. No baseline/current comparison is valid.
Add finite stage/error labels and allowlisted ResourceError codes only, preserving
condition/evaluation order, native outcomes and all existing EvidenceError codes.
No arbitrary exception text, environment, path, command or raw output is exported.

Underlying inspection causes remain unknown pending hosted diagnostics. Native
sources, all26protected files, deadlines, assertions, leak policy, matched inputs
and workflow remain unchanged. Task308 remains DOING; no acceptance is claimed.
See c28-inspection-diagnostics/receipt.json.


## C29 exact Meson loader prepend and legacy resource names

Run46 mask preflight exits127 before paired native tests. Reconstructing the
canonical missing-GEGL loader message matches its full211-byte recorded hash.
Pinned Meson1.7 introspection contains the build-library prefix only; actual
execution prepends that prefix to inherited dependency paths. The old helper
replaced the inherited tail. Reconstruct only the known prepend with strict
absolute unique build-local prefix checks and exact inherited bytes. Predicted
environment hashes are recorded, not claimed equal to native hashes. Actual
paired hashes must equal one another, and pre/post loaded-provider equality
remains required. A real tiny fixture reproduces old127/new0, not GIMP acceptance.

Matched runtime source/data rejects four unchanged tracked MyPaint filenames.
Keep the original relative-name grammar; admit only those four exact literals
with fixed Git-proven SHA256/size under source/data. Require regular nonsymlink
files and exactbytes; reject all other names and mutations. Real source inventory
reaches577files. No asset is renamed, changed or omitted. Later runtime checks
remain required, so partial metrics do not establish a baseline/current comparison.

Native sources, all26protected files, workflow, deadlines, assertions, leak policy
and matched inputs/order remain unchanged. Paired native proof and qualified
comparison remain pending. Task308 stays DOING. See c29-loader-resource-contract.


## C30 bounded native failure observation and catalog producer witness

Run47 actually observes focused fixed OK/0 and old EXPECTEDFAIL/-5, but full107
fails/-6, so the aggregate is NOT ACCEPTED. Its exact failed leaf/assertion was
not retained. Add only original107 allowlisted case identities, source-validated
assertion locations/macros from fixed TU/includes, finite flags/counts and hashes.
No raw message, value, arbitrary source path, environment or address is exported.
A validated source location is a diagnostic hint, not causal proof; all native
acceptance conditions and failure statuses remain unchanged.

All14 matched runtime receipts reject catalog-helper-mismatch because the frozen
producer omits helper_sha256, not because observed helper bytes differ. The driver
bridge checks the exact immutable regular nonsymlink helper before/after execution,
keeps the unchanged stage_build_locales result, rejects a preexisting producer
field, then adds the witnessed hash. This detects before/after mutation, not an
atomic TOCTOU guarantee. The frozen helper and strict consumer are unchanged.
Five tests execute that exact heredoc using the real helper and minimal .mo data.

Native production/test sources, registrations, all26protected files, workflow,
deadlines, assertions, leak policy and matched arms/order are unchanged. Full107
and qualified runtime comparison remain pending. Task308 stays DOING; no native
aggregate acceptance or comparison is claimed. See c30-native-observation-catalog.


## C31 review candidate: icon fixture root and alias grammar

See [C31 bounded review and run48 terminal observations](c31-runtime-fixture-parser/REVIEW.md).
Both reproduced fixture/parser defects are corrected locally; finite alias substage
diagnostics preserve rejection and privacy. Exact C31 native/hosted validation
is pending; task308 stays DOING / NOT ACCEPTED.
