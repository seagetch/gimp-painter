# Matched pre-click control — proposal only, no production edits

## Question and frozen inputs

C15's focused idempotence assertion succeeds, but run34 did not demonstrate a
case14 speedup. Fresh-process NORMAL already passed (first wait207388us,
longest iteration153190us); ASan first wait was12970767us with longest
iteration12966932us. Do not call the normal result merely a warmed-suite result.

Compare production:
- baseline e25a94e21efca6e6604ddac6f15570c049f421e7,
  tree8634eeef49c81aaa5c25192cb76d749ff680683f;
- current b671825309326bdb55571652e43ab78cc83fd92d,
  tree4469df41e161e3f855918125932e377757a1c937.
Both pin gimp-data a31e5dea4f572fb5f33b06f5c4b86db5f30643dc.
Future source pins require explicit re-review, not an implicit moving HEAD.

Verified byte-identical at those versions: app/tests/tests.c,
gimp-app-test-utils.c, gimp-test-session-utils.c,
app/tests/gimpdir/sessionrc-native-oncanvas, and
migration/baseline/debian13-package-lock.json.

## Probe files / production isolation

Add the same test-matched-preclick.c and matched-preclick-phases.inc to
app/tests in two dedicated source worktrees; append the same meson.append stanza.
No production file is overlaid. Do NOT use or alter prepare_brush_baseline.py or
its existing baseline-RED worktree/seal. Record exact test-overlay diff and hashes,
require the only source modifications to be these files and the Meson append.

The standalone executable uses the same shared tests.c initialization and the
same initial unused1100x130 harness window as painter-compact-options. It omits
unrelated test registrations (which do not construct the fixture). This difference
from the original harness is explicit; both comparison arms use this identical
small executable. Compare current observer timing against original fresh case14
as a fidelity check before interpreting a baseline delta.

Fixture copies case14 through its first pre-click wait: private tool options,
context-property copy, parent=NULL assertion, two same generated brushes and
weak observers, set brushA, registered horizontal factory, standalone window,
show_all, same idle drain, direct-child thumbnail selection. Name-or-stock-container
fallback is exactly the existing case14 selector. e25's stock thumbnail is valid;
no new popup type, symbol, content assertion, click or reveal operation is needed.
Both production versions eagerly construct auxiliary popup children.

The five existing phase probes are copied VERBATIM from the current Transform
helper, not from production: weak object filtering, global emission hooks,
connect-after handlers and32-event ring are unchanged. Both arms get identical
probes, and global-hook timing perturbation remains a limitation.

## Primary result is unchanged

/matched-preclick/primary preserves the original loop exactly: stable<3 and
monotonic<deadline, nonblocking main-context iteration, then1ms sleep. Deadline
is set3s after begin_updating/queue_draw/request_phase, as before. Callback uses
exact same origin/allocation comparison; the first frame establishes a baseline,
then three unchanged comparisons are required.

Record original predicate stable>=3 at loop exit; do NOT silently strengthen it
to elapsed<=3s. A dispatch can overshoot3s and original code can technically pass
if that dispatch also supplies enough stable observations. Report elapsed and
overshoot separately, never conceal it. No-frame measurement asserts failure.

/matched-preclick/extended is OPTIONAL and must run in a separate fresh process.
It freezes the exact same primary result, then observes until stable3 or absolute
30s measurement cap. Later stability NEVER changes primary_pass. That cap is not
an acceptance deadline. A GLib dispatch is not preemptible by this loop; Meson's
180s external process timeout bounds a stuck dispatch/startup. This mode is not
part of the required12-run primary matrix.

Measurement exit0 means collection executed, not acceptance. All output is
explicitly acceptance_eligible=0. Original Brush acceptance and3s assertion stay
unchanged. No leak flag, suppression, deadline, warmup or debugger change.

## 2x2 / three fresh repetitions

Four cells: e25 normal; current normal; e25 address+undefined; current
address+undefined. Three fresh process/profile/Xvfb/D-Bus repetitions per cell.
Serial runs only. Alternate arm order across repetitions (baseline/current,
current/baseline, baseline/current); alternate normal/sanitizer block order too.
No extra run that warms the same process/profile. OS filesystem caches are not
claimed cold; do not drop caches or change host settings. Pairing/order and
thread CPU versus wall measurements help separate scheduling effects.

Every primary invocation selects ONLY `-p /matched-preclick/primary`; require
exactly one matching test, no skip/77, no timeout, one summary and observed frames.
Retain native exit/sanitizer verdict separately. A post-measurement LSan failure
must remain failed; never call it acceptance success. Timing may be qualified as
collected before exit failure, but any sanitizer fault before/during measurement
invalidates that comparison. No suppression is used to obtain a clean result.

## Match runtime and build closure

Use the existing locked Debian13 compiler/dependency flow and exact existing
Meson debugoptimized options (libunwind=false, libbacktrace=false, auto features
and other options unchanged). Sanitizer cells add ONLY b_sanitize=address,undefined.
ASAN_OPTIONS exactly:
halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1
UBSAN_OPTIONS exactly:halt_on_error=1:print_stacktrace=1
LSAN_OPTIONS absent. Normal cells use the existing normal runtime policy.

Same portrait1080x1920x24 Unix Xvfb, scale1, native-oncanvas sessionrc, menu/icon
fixtures, locale/font/icon catalogs, empty fresh writable profile roots, D-Bus,
plugin search and sanitizer scope. Reuse the existing environment allowlist;
never dump or generically replay an environment. Pin exact GTK/GEGL/Pango/etc
package closure and compiler/linker identities, not merely version labels.

IMPORTANT: startup plugin inventory must match. Existing current union builds
common plugins whereas the3-target baseline-RED build may not. Enumerate the
allowed common plugin target set and build it for BOTH arms; verify same relative
names/registration inputs. Likewise build/stage all required locale catalogs for
both arms, or prove their source/resource hashes identical before reuse. Reject
unexplained missing catalogs, plugin lists, session/schema/font differences.

## Reuse without mixed objects

One configured build per (production version, instrumentation mode), reused for
its three process repetitions: FOUR builds, not twelve. Compile matched-preclick
and its transitive libraries plus required menus/icons/plugins/catalogs; not the
whole34/36-target acceptance union. Suggested target nucleus:
matched-preclick; menus/image-menu.ui:custom; menus/dockable-menu.ui:custom;
plus the matched plugin/catalog closure determined above.

Current normal and sanitizer builds may reuse their OWN production libraries if
they are the exact pinned source/tree, options, toolchain, dependency closure and
paths and include the observer as a declared test target from configuration.
A future committed diagnostic-only target is preferable to mutating a frozen
acceptance worktree/build. Any post-acceptance additive build needs new diagnostic
receipt and proof production binaries/libraries remained identical; existing
acceptance receipts must not be rewritten.

Do not transplant candidate libraries/objects into e25, copy Meson builddirs
between worktrees, relabel a normal binary sanitized, or reuse baseline-RED's
restricted overlay directory. Separate baseline normal/ASan builds can share
read-only downloaded dependency archives, not compiled project objects. Reuse of
an existing e25 NORMAL build requires the same strict identity/closure proof and
a separately permitted diagnostic target/overlay; otherwise build a fresh one.

## Public evidence, not private dumps

Record commit/tree/submodule/test-overlay hashes; selected nonsecret compiler and
Meson properties; hash manifests for compile_commands, build.ninja, actual ELF
binary/providers, locked dependency packages and runtime resource inventory.
Record cell/repetition/order and relative elapsed/CPU/frame metrics. Public report
must omit environment dumps, absolute private paths, raw wall-clock timestamps,
GDB output and process memory. Keep ordinary harness stdout/stderr private and
extract only the fixed MATCHED_PRECLICK/MATCHED_FRAME records plus finite test
status; independently preserve sanitizer failure status without sanitizing it
into success. The observer exports at most64 frame observations and32 phase events.

Decision: similar slow sanitizer first-iteration distributions on e25/current,
with quick normal arms and eventual stable frames, support preexisting startup /
instrumentation cost. A repeatable candidate-only excess supports a production
regression investigation. Different probe fidelity or resource inventories make
comparison inconclusive. Neither conclusion automatically changes acceptance3s.

Native GTK development metadata is absent locally. This is uncompiled/unrun
source and integration guidance, not observed normal/sanitizer or baseline results.
No remote changes, production edits or rejected ptrace reproduction were performed.
