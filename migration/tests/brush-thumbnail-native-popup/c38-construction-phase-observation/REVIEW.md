# C38: bounded construction-phase observation (local review only)

Base: published C37 9c875df0d1 (tree87ce34b44edb782e399850af006cf65887646d8b).
Run55 is terminal (five successful, three failed jobs). Its receipt is retained
beside this review. No production latency change, C38 native proof, publication
or task acceptance is claimed.

## Question and evidence ceiling

Run54's initial application-frame barrier succeeded in0.536s but the later private
wait failed in12.171s. The separate debugger capture localized overlapping10.81s
check-resize /11.56s layout to the original GimpImageWindow, with the private target
layout only9.6ms. C37 withdrew the unsuccessful barrier and introduced no bypass.
The capped96-frame SVG/preferred-width samples do not identify the initiating
invalidation or internal widget. Mapping the new private GtkWindow could change
original-window activation/backdrop style; this remains one hypothesis, not a cause.

## Narrow diagnostic implementation

Only the existing separately built, after-failed-primary diagnostic changes.
The generator copies the Brush include as well as the Transform include. Exact
unique substitutions add markers before/after context copy, factory, map, idle drain
and the first wait. Twelve ordered phase markers include observation start/finish.
Removing the inserted snippets recovers the original source bytes. No primary or
production source is edited. Original first-wait3s predicate and assertions remain.

Before construction, enumerate only existing GimpImageWindow instances, at most16.
Observe activation notification, state flags, style-updated, check-resize, frameclock
layout, destroy and default icon-theme changed. Emission hooks observe RUN_FIRST signals after their class closures (including
state-flags-changed and style-updated), and RUN_LAST before the class closure
(check-resize). They are not universal pre-handler timestamps. Notify and destroy
use validated typed handlers because GTK marks those signals NO_HOOKS. Signal shape/property validation precedes connections.
Store only fixed numeric roles, ordinals, state flags and monotonic timestamps.
Callbacks neither allocate nor print. Preserve the first256 events; overflow is
explicit clipping rather than silently losing the first invalidation. Weak pointers
do not extend owner or clock lifetime. Disconnect hooks/handlers before export.

Clock identity is sampled at phase markers and finish. A temporary replacement
between checkpoints cannot be excluded. New windows after the initial snapshot are
not followed. Notification delivery order is observed, not the exact mutation time.
Style/resize notifications do not measure individual descendant size-request
cost; slow RUN_FIRST class work can precede its observed hook. Shared clocks can
produce one observation per owner for the same emission; do not add their times. Existing owner span records remain a separate wait-relative time domain;
phase records begin before construction and are phase-relative. Do not subtract
timestamps across these two origins without using their explicit phase ordering.

The fixed PHASE_DIAG numeric protocol has independent strict bounded parsing.
Capture requires all12 ordered phase markers, monotonic event times, exact record
shape and complete coverage. Missing hooks, zero owners, destroyed owners, changed
clocks or clipping are explicit inconclusive results. The combined diagnostic is
captured only when both existing owner and new phase records are valid. It still
cannot supply native acceptance, even when captured before the failing assertion.

## Scope and safeguards

Same diagnostic process, existing180s controller/eight96-frame samples, same20min
outer diagnostic budget, unchanged cleanup and original primary result. No second
build/job/retry, prewarm, focus suppression, cache mutation, delayed deadline or
leak waiver. The existing separate diagnostic build seals both generated includes,
phase helper and parser in its identity receipt. All26 protected sources unchanged.

Internal statusbar/menubar source bracketing is deliberately deferred: that would
expand the libapp source overlay and its identity policy. Phase/signal evidence can
first establish whether style invalidation occurs and which construction interval
precedes it. Run55 evidence has been checked below before proposing another diagnostic build.

Publication remains root-controlled and requires specific authorization under the
existing accepted-parent seal. This local preparation neither rewrites the public
branch nor changes that seal rule. Task308 remains DOING / NOT ACCEPTED.

## Local validation completed

Full Brush source/synthetic suite:1134 tests pass. Exact phase helper:5 tests
cover12 real-GObject lifecycle/signal scenarios without a display; GTK window
getters/enumeration are modeled, and installed GTK signal metadata is inspected.
Strict parser:15 tests pass, including destroy/alive contradiction rejection.
Existing owner suite:13 tests pass; bootstrap integration16 tests pass. WBS and
whitespace validation pass. All26 protected hashes and three primary test sources
match C37 byte-for-byte. These are not a native GTK build or runtime proof.

Independent review corrected the RUN_FIRST ordering description and requested
destroy-record consistency, now enforced. No pointer/title/raw debugger payload
is added. Native feasibility and actual invalidation attribution remain unproven.

## Run55 decision update (evidence-only revision)

Public C37 run41561860476 completed five successful and three failed jobs. Normal
Brush aggregate passes all21 cases at both scales, confirming removal of C36's
zero-window assertion regression. Isolated normal first wait is206366us/stable3.
The isolated sanitizer first wait still fails at6763268us/stable0, with6759694us
in one dispatch. Twenty other leaf assertions pass but processes leak
172234–201378 bytes. No sanitizer acceptance or leak repair is established.

Raw owner sample from the sanitizer artifact records window2 role3 (mapped
GimpImageWindow) check-resize6130191us and frame-layout6528865us total. These overlap;
do not add them. The16-owner snapshot is clipped because other generic toplevels
fill the bounded list. The private target has no recorded resize/layout entry
before this wait ends. This is separate debugger instrumentation, not native cost
attribution or proof that excluded windows did no work.

The existing trace begins at first wait and cannot show whether copying context,
constructing the toolbar or mapping the private window initiated the original
window's invalidation. C38 therefore remains a justified next *diagnostic*: it
starts before construction and filters its16-owner phase snapshot to original
GimpImageWindow instances rather than all hidden generic toplevels. It preserves
the existing owner trace and its limitations, rather than treating clipped owner
coverage as comprehensive. Additional phase clipping or absent owners still fail
closed as inconclusive. No production fix follows from these observations alone.

Matched identity is comparable but comparison remains ineligible/inconclusive;
all six sanitizer repetitions fail their primary criterion. Its baseline includes
a declared diagnostic Meson compatibility overlay, so normal descriptive timings
are not a pristine-baseline causality or significance result. The separate babl
job remains blocked by lcms2.pc search-path failure. These obligations remain open.

This revision changes only retained evidence, review/WBS prose and derived source
seal hashes. The already independently reviewed C38 diagnostic implementation,
parser and tests are byte-identical to staged tree
dfabf412d837f7fefa403cc4d855c681a99c0c2e. Its existing1134-test result remains code
evidence; refreshed seal/WBS checks verify this evidence-only revision.
