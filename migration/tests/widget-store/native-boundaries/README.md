# 08.017 bounded native session, preview, DnD and MyPaint boundaries

Public base: `0f71e730f3d9decb30335afca606d775f98c464a`, tree `66c8d9cd1adce6268700f7fbbcbb144da86b6f30`. The only production change in this
subset restores `gimp_session_write_boolean` in its native implementation and
header. Its fourth explicit C11/C++14 header route is additive. Tests grow from
canvas 49 to 55 and editor 14 to 16, preserving every previous registration in order.
An uninstalled test fixture copies the exact shipped vector-install icon layout
without modifying the pinned gimp-data submodule. Original tasks stay 140/661;
all 93 original 08.017 source duties and 08.017 itself remain TODO. Work ledgers
are byte-identical.

## Verified scope

Both native and standard ASan/UBSan passed all eight selected registrations, 434 cases per mode. Both header gates passed 365 compile probes and 16 route controls; the separate fixture-generator registration passed 18 controls per mode.

Helper absence on the public baseline is established from the exact preimage
implementation/header and a bounded 2605-file current app source census. An
undeclared and undefined callable cannot support a dependent runtime red; none
is invented. The restoration preserves the original signature, true/false
spelling and paired session class references. The class balance is source
proven, not measured by runtime reference counting. The old 30-file caller census
is explicitly bounded. Original session_save does not call this helper and
writes yes/no directly; current session_save retains that separate convention.

The literal case checks both exact helper spellings and scanner round trips in
32 iterations. The reader/writer case uses two boolean pairs and four fresh GLib
test subprocesses. For each pair, the parent writes actual helper true/false
output into a newly owned profile; the first child loads it through the real
production startup session_init and saves actual yes/no output. The second
fresh child reloads that saved session/dock pair and saves it again. Each child
asserts opposite initial config values before GUI initialization, then asserts
the parsed values, real saved syntax and dockrc creation. Deferred startup work
is settled before observation/save. The parser is not reinvoked on a populated
GUI dialog factory, where production session_init would append session entries.

A test-local child exit hook keeps normal shutdown writes inside the owned
profile. The parent checks both child exits and saved outputs, preserves its
config, restores the test-control environment and removes only its fresh profile
trees without following symlinks. Each child has a 30-second trap bound; the
ordinary 60-second canvas registration timeout remains unchanged. Earlier
in-process fixture attempts are historical failures, not the final contract.

Native renderer tests assert background and clipping/offset results,
retain old Cairo storage across replacement/destruction, and wait for actual
red-to-blue drawable preview pixels. Renderer/icon final storage release is
observed through qdata destruction counters. The tool icon must exist by its
exact production name before rendering; a missing-image fallback cannot pass.
The icon case also restores the captured visibility preference before close.
This proves named-icon availability/rendering/lifetime, not a golden raster.

The DnD case uses a backend-created GDK context, GTK-allocated selection, real
source/destination registrations and signal-bound dispatch with info 23. It
covers tool/group payload resolution and individually expected invalid-format/
UTF-8 warnings, preserving fatal-log policy. It cleans up clipboard ownership,
metadata, drag state and temporary group/selection state. It is not physical
drag input or an inter-process transport acceptance.

MyPaint cases call the actual registered dock constructor and GtkTreeSelection,
replace/detach real contexts, observe native factory add/remove callbacks and
retain old/new GTK model storage across close. Internal transient brushes have
native collection identities. Selection/draft and membership are restored;
real options selection history remains. Internal resources do not establish
file-save/delete behavior, global model-history reset, new close-during-selection
reentry, or the separately open legacy MyPaint PDB selector contract.

## Failure and fixture provenance

Four initial native focused failures remain separately documented, including
the original test and executable hashes. The session reader/writer terminated
with a fatal dock-columns assertion after its environment cleanup. Source and
failure ordering support the startup-idle diagnosis, but no backtrace was
collected. The first correction settled already queued startup work before config changes,
without altering production code, assertions or fatal warnings; later ordered
failures prevented accepting this in-process approach.
Both MyPaint cases initially reached the native identifier assertion because
transient resources lacked internal/file/image ownership. The fixture correction
uses the existing native internal-collection API before insertion.

The strict icon case initially failed because the ordinary uninstalled icon
layout did not expose the requested shipped icon. The correction follows the
pinned production vector asset recipe, with 1,191 byte-identical destinations,
source/recipe manifests, ownership checks and fail-closed verification before
and after each final run. No custom artwork, synthetic fallback, icon assertion
waiver or gimp-data pin update is introduced. Shipped ancillary bitmap files
are part of that vector recipe; vector-icons=false behavior is not accepted.

A later full related native run exposed a separate order-dependent reader/writer
failure: after ten earlier canvas cases it reached a fatal invalid-display
assertion in gimp_display_get_shell. The editor registration still passed all
16 cases. This preserved mixed result is distinct from the startup dock-columns
critical and prior focused greens. An attempted in-process correction then
owned an ordinary native image/display through parser/save and config
restoration. It preserved assertions, case order, startup settling and fatal
warnings. A second ordered run on that revision still
failed after ten canvas cases, this time with gtk_widget_get_mapped rejecting an
invalid widget; the editor again passed 16 cases. That result is separately
preserved and prevents treating the first correction as sufficient. Diagnostic
instrumentation is excluded from the source allowlist. The final gate requires
successful corrected related suites against the final candidate. Source review
identified session_init as a production startup-only path before live mode
observers/display restoration; it appends dialog-factory session entries. The
final fixture therefore replaces populated-GUI reload with the fresh-process
startup/save/reload flow above. This does not claim to repair or completely
explain the separately observed GTK live-window-switch assertion. A diagnostic
stack reached adjustment/allocation callbacks but not the application caller;
its diagnostic source is excluded. The final fixture uses the supported
lifecycle without suppressing fatal diagnostics.

The first ordered run with that startup fixture passed the session case and
43 canvas cases, then failed the later overlay_input_and_restore initial-hidden
assertion; editor again passed all 16. This third mixed result is preserved
separately. It establishes successful session execution in that run without
turning the failed registration into a suite pass. Source review traced that
failure to the icon test changing the shared painter-canvas-ui preference through
the normal action callback. Its shell really is destroyed; later new shells
read the leaked default. The final fixture restores captured local visibility
and shared preference before close, checks both, and asserts icon storage is
still live before close and released exactly once on close. It preserves the
later overlay assertion and adds no production change.

Initial failures, corrected focused results and final aggregate evidence have
separate identities. No assertion above is relabelled as an observed ASan memory
error or a reproduced production defect. Focused passes alone are not the final
aggregate gate.

## Limits and publication

This is eight selected Linux GTK3 runtime registrations, not the full aggregate,
physical input, historical visual parity or full-platform acceptance. Native
keeps the ordinary no-RTTI policy. Standard ASan/UBSan retains RTTI/vptr,
ordinary registered timeouts and normal fatal warnings. Leak detection is off;
third-party dependencies are uninstrumented. Header probes establish compile
closure only, not link ABI or runtime acceptance. The existing historical
header inventories remain byte-identical.

`source-contract.json` keeps eleven related original rows, exact payloads,
provider routes, owners and individual remaining requirements.
`all-original-duties.json` preserves all 93 rows and their sibling duties from
the preceding published layer report. No old comment, debug, missing API or
unused type becomes accepted through this bounded subset.

The manifest carries exact blob SHA-1, SHA-256, byte length and mode for each
allowlisted payload. The source allowlist has nine files: six exact preimages
and three new icon-fixture paths. Root must verify those preimages and absent
paths against the declared public base. Reports use positive allowlists and
include no raw logs, machine paths, process/display identifiers or authentication
material. The local freezer never builds, runs application tests, edits live
source, advances ledgers or publishes remotely.
