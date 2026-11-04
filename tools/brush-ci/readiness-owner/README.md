# Case14 owner-readiness diagnostic (never acceptance)

Run51 native case14 received its first target after-paint at7,127us; target
update/layout/paint completed by6,839us. One main-context dispatch consumed6,028,452us (it may include that first frame),
leaving stable0 (one baseline frame, no geometry changes). Existing
probes only observe target window/clock. Separate debugger samples repeatedly
show sizing/SVG/ASan unwind, but truncate before owner/wait frames. This diagnostic
identifies which already-existing toplevel/clock overlaps the long dispatch,
including target legacy check-resize outside its frameclock; it
is not a rendering-speed repair, leak verdict, or acceptance workaround.

## Immutable primary / generated diagnostic

No primary source file or protected26 pin is edited. `generate.py SOURCE OUT`
copies the complete original compact-options main test TU, redirects only the
Transform include to a generated copy, and inserts owner begin/dispatch/finish
calls in that copy. Overlay anchors must be unique. The original3s deadline,
nonblocking main-context iteration,1ms sleep, stable>=3 assertion and all normal
case behavior remain textually unchanged. Probe setup precedes the timer; no
GUI work is prewarmed or drained. Instrumentation adds overhead, which is explicit.

`meson.append` is a registration recipe ONLY for an authorized separate diagnostic
source/build overlay. It must never be appended to the primary protected source.
Target `readiness-owner` includes both orientation companions and existing test
libraries, is not installed, and has build_by_default:false. Select only
`-p /brush-thumbnail-native-popup/private-target-release`. Integration and private
receipt closure are owned by the separate integration review; do not assume an
unconfigured primary build has this target.

## Probe contract

At most16 existing toplevels, target first; clipping explicitly reported. Roles:
1 target private window;2 original compact-options harness;3 GimpImageWindow;
4 other ordinal. No dynamic names, titles, addresses or arbitrary strings export.

Each owner gets a GtkContainer::check-resize probe and a frameclock layout probe.
Shared clocks are deduplicated; owner rows retain the shared clock-slot index.
Missing clocks/unavailable signals are explicit hook=0 coverage gaps. New windows
or replaced clocks after snapshot are not followed. This is bounded existing-owner
coverage, not proof that an unobserved later window did no work.

Emission hooks enter before preexisting handlers; connect-after observes exit.
Signal arity, void return and NO_HOOKS flags are validated. Each probe has stable
static storage and weak object pointer; destruction never leaves an untracked
object access. Finish removes global hooks first, disconnects live object handlers,
then removes live weak registrations. Nested signals track depth and only count
outermost duration. Unpaired entries/orphan exits remain explicit, not repaired.

Callbacks only increment counters/timestamps and fixed32-entry ring; no allocation
or logging in signal callbacks. Same ring includes dispatch begin/end slot32.
Summary rows preserve aggregate duration when older events fall off the ring.
Records print after original target probes finish and before stable assertion.
Only first invocation is instrumented; subsequent waits are unchanged.

## Collector contract

`collect.py` uses the unchanged original Controller's strict owned-child/pidfd
lifecycle; no attachment, inferior expression/argument reads, calls, memory dumps,
new capabilities, or sanitizer changes. It requires diagnostic binary name and
exact expected hash. Initial breakpoint stack must contain the selected case.
Eight samples retain the existing96-frame cap and half-second cadence. Because
known dispatch lasts6s, same child may then resume to a separate noinline done
breakpoint after record flush, within the same180s total cap. No new child/retry.
Original test3s deadline is never changed. Finish breakpoint occurs before original
stable assertion; diagnostic records are never a native pass even if collected.

Only fixed OWNER_DIAG numeric rows enter the report, raw or target-stream MI.
Debugger-console strings and all other raw stdout are discarded. Row/field limits,
integer syntax, indexes, phases, roles, ring count and completion shape are checked.
Missing/invalid records or cleanup failure => inconclusive. Debugger wall time and
records are diagnostic, never acceptance timing or leak-clearance evidence.

## Safe local verification

```
python -B tools/brush-ci/readiness-owner/test_owner.py
```

Eight tests pass in current environment:
- generated source differs only by specified additions, originals unchanged;
- overlay anchor drift fails closed;
- record parser rejects arbitrary strings, console spoof, addresses, malformed or
  incomplete records, and duplicate completion;
- real installed GTK/GDK types confirm both signals hookable, zero args,void;
- exact probe compiled with real GObject signals tests nested enter/exit, shared
  clocks,16-owner/32-event bounds, destroyed owner+clock, disconnect and one-shot;
- mocked controller completes after eight samples in the same child;
- timeout/cleanup failure remain inconclusive.

Synthetic C uses minimal stable64-bit Linux GLib ABI declarations because local
headers are absent. It is normal execution, not ASan/LSan. No GtkWindow/full GUI,
GIMP compilation, debugger/ptrace, babl diagnostic or hosted run has been executed
for these components. Full isolated build closure and hosted execution remain
separate gates; no claim that source-only integration is compiled or accepted.


## Separate build integration (local proposal; native execution unverified)

The existing post-primary diagnostic workflow slot is replaced, not duplicated,
and only runs after the sanitizer primary driver FAILS and runtime seal succeeds.
The diagnostic outer slot is explicitly proposed to increase10minutes to20minutes;
8 debugger samples, primary3second native deadlines and assertions remain unchanged.
This costs potentially10extra minutes after a failed primary run, not a new
acceptance allowance. Root will disclose this in replacement approval.
Primary app/tests/meson.build, primary build artifacts and all26 protected common
files remain unchanged. Only a fresh exact-current-commit diagnostic worktree
receives the exact Meson appendix; a separate fresh Meson sanitizer build uses
shared immutable locked dependency payloads and existing matched resource targets.
No object/compiler/link command replay or primary object reuse is introduced.

The primary receipt is verified before/after as a PRECONDITION ONLY. A separate
private receipt hashes the modified registration, committed source/submodule,
generated probe bodies, fresh binary/objects/libraries, Meson metadata/recipes,
compiler tools, locked package archives/extracted tree, locale outputs and helpers.
The wrapper rechecks it before execution; public summary is hash-only/finite.
Dependency host-link policy is the existing matched inventory policy; this is
not a claim to hash all host libraries. GDB is installed only after failed primary,
so diagnostic system dependencies may differ. Primary integrity is rechecked on
ordinary failure exits, too. Phase checkpoints survive a build timeout as running,
not a success. Original driver outcome is never replaced.

IMPORTANT: no actual fresh GIMP diagnostic build or native debugger execution
was performed locally. This is proposed configuration, not hosted evidence.
The former10minute slot is not practically supported by run51 primary compilation
(~7minutes to compact target link) plus180second observation/cleanup. Proposed
20minute diagnostic-only slot still has UNVERIFIED feasibility and can time out.
No native deadline extension, ptrace-policy change, security bypass or unsupported
compilation shortcut is included.

Meson configure and compile each receive only the REMAINING portion of one absolute
15minute preparation budget starting at runner entry, with5second timeout kill grace;
there is no per-command reset. Observation admission is checked AFTER expensive
private identity verification and immediately before controller exec. It reserves
180seconds controller,9seconds cleanup and30seconds outer margin within1200seconds.
After981seconds elapsed, no controller/debugger is launched and a finite inconclusive
budget receipt is written. The preparation cutoff leaves additional time for identity
checks; overruns remain inconclusive, never waive the native3s stable-frame assertion.

## C35 transport correction after run52

Run52 verified a fresh separately compiled/identity-checked diagnostic and cleanup,
but exported zero owner rows. Real g_test_init prefixes g_print with TAP comments;
the probe now uses printf for its fixed numeric records, preserving fflush and the
strict grammar. The synthetic real-GObject proof now initializes that actual harness.
Quoted MI target chunks are joined only to newline with a4096-character bound;
truncation/malformed chunks/mixed raw-owner transports fail closed. Console/log
streams never contribute bytes. Last observed stop exports only a finite reason and
finish-breakpoint-match boolean, not raw MI; it is not necessarily the final stop.
Thirteen owner tests cover these changes. Full native/hosted capture remains pending;
run52 raw transport and finish identity were not exported, so exclusive causality
cannot be proved retrospectively. See c35-owner-record-transport/REVIEW.md under the
Brush migration evidence. Original timing, assertion and diagnostic budgets unchanged.

## C38 preconstruction phase extension

The generator now also redirects the Brush include to a separate generated copy.
It begins bounded original-GimpImageWindow activation/state/style/resize/layout
and theme observation before private construction. Twelve fixed phase markers
cover context copy, factory, map, idle drain and first wait. The independent
PHASE_DIAG stream retains first256 events; missing coverage, destroyed owners,
changed clocks or clipping are inconclusive, never a native acceptance pass.
See migration/tests/brush-thumbnail-native-popup/c38-construction-phase-observation/REVIEW.md
for limits, the two different timestamp origins and deferred internal size hooks.

## C41 leaf-width observation (diagnostic only)

Run57 still failed primary case14 at12.282541s after early icon-theme settlement.
Its samples show SVG decoding and Pango sizing during inherited GTK layout; no
observed original-window style event excludes neither descendant style work nor
earlier invalidation. This addition measures a narrower part of that workload,
not a production fix or acceptance result.

The same separate diagnostic TU installs three typed forwarding thunks immediately
before the first geometry wait's original timer: GtkImage preferred width,
GtkCellRendererText preferred width, and GtkCellRendererPixbuf legacy get_size.
Each calls the exact saved base original once. Only pointer-equal static inherited
slots are changed; arbitrary overrides remain untouched and coverage is partial.
Height slots are never modified because GTK baseline dispatch compares their
identity. No public sizing API is called by a wrapper. Exact upstream3.24.49 base
methods were reviewed for self-redispatch; Debian patch-byte identity is not claimed.

Setup audits at most128 types per family without initializing classes. Existing
static inherited slots and later copied thunks are restored; direct installed slots
are handled even if the final descendant audit clips. Dynamic/plugin-backed classes
are not accessed through unpinned vtables. Missing bases, dynamic classes, class
bounds, restore conflicts, clock/counter/parent bounds or unmatched calls reject
the diagnostic. Unknown overrides alone permit a captured partial observation with
coverage_complete:false. The collector terminates its owned child at the existing
finish breakpoint; it never resumes GUI operation after a rejected restore audit.
Privately cached thunks cannot be restored, but remain exact inactive passthroughs
until that child is terminated. No whole-process cache-restoration claim is made.

Callbacks use fixed counters/timestamps, bounded parent traversal and numeric type
checks. They do not allocate, log, look up icons or properties, query sizes, retain
widgets, or access a widget after forwarding. Roles0..4 mean unknown, target,
harness, GimpImageWindow, other. Role3 combines all image windows, including later-created instances. The phase
snapshot alone cannot establish unique original-window ownership.
Per-bucket outer totals overlap across categories. The union row measures outermost
spans across all three families without adding nested spans. Debugger pauses remain
inside wall durations; these are not primary performance timings.

Coverage excludes height requests, work before the preferred-width vfunc (GTK
ensure-style/resize), direct static calls such as pixbuf render's get_size, custom
overrides, and uninstrumented late classes. Low totals do not exclude any of those
paths. Even coverage_complete:true means this declared leaf-slot scope, not all
layout work. GType child enumeration can allocate a sibling list outside the timer;
its GLib allocation is not claimed bounded, although visits/storage/output are.

The fixed21-row LEAF_DIAG protocol contains one summary,3family audits,15metrics,
one union and one completion marker. The parser rejects invalid order/counts,
private strings, numeric overflow, mixed transport, incomplete restoration and
inconsistent summary/metric fields. Records are included only in the existing
non-acceptance diagnostic; primary3s/stable-frame/leak/protected26 rules are unchanged.

## C42 per-image identity extension

The same existing width thunk now profiles weak snapshot identities, rather than
assuming that run58's89 calls represent89 distinct cold images. Setup visits existing
GimpImageWindow trees through gtk_container_forall, including internal notebook
labels. Limits are16hosts,128 toplevel-list entries,4096 queued nodes,256 images,
64 tree levels and128 ancestor steps. The setup queue itself is weak; finalization
or reparenting during virtual enumeration rejects a complete snapshot. Strong
widget ownership is never added. GLib list allocation and GTK's internal sibling
callback count are not claimed bounded, only stored/processed traversal work.

Each image ordinal keeps snapshot and first/last-entry numeric metadata and separate
first-observed/repeated inclusive call metrics. Fields describe storage, logical/
pixel size, visible/mapped/child-visible/realized state, state flags, scale and fixed
ancestor/host categories. No icon/widget names, paths, addresses, size queries or
icon loads are requested. The real scale getter follows successful bounded parent
validation. All metadata precedes forwarding; exit tokens contain no object pointer.

Weak-cleared ordinals are never reused. GtkWidget destroy observed and weak-alive
are reported independently. Untracked calls may be later images, replacements or
other hosts; they are not unique/new-image counts. First observed is not first-ever
or cold, and repeat width requests do not prove decoding. Inclusive nested spans
overlap. Mapped/visible state is not painting or occlusion evidence.

IMAGE_DIAG has bounded ordered summary/window/identity/metadata/metric/completion
records, at most1557 rows. Missing semantic ancestor types permit explicit partial
capture; other faults are inconclusive. Image entry/exit totals must match the same
GtkImage leaf wrapper. Maximum metadata/metric serialization is tested within the
unchanged1MiB output budget, leaving space for the existing bounded observations.
The report and wire caps, original timer/assertion, class restoration and verified
owned-child termination are unchanged. No production or primary test file changes.


## C43 bounded requested-icon equality

Separate `icon.inc` and `icon_records.py` observe exact requested-name byte equality using run-local IDs, never raw names, paths or hashes. Fixed256-name/128-byte/128-config bounds and explicit overflow preserve diagnostic-only scope. Direction and use-fallback are scalar metadata; no icon lookup or style query is introduced. Requested names do not prove resolved asset, cache behavior or cold loading. Tracked means live weak-snapshot membership; parent IMAGE_DIAG flags and semantic coverage also apply. Original primary3s/leak gates and owned-child controller remain unchanged. See `migration/tests/brush-thumbnail-native-popup/c43-icon-request-diagnostic/REVIEW.md` and the independent review.
