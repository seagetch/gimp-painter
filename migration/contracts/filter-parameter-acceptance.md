# Bounded Linux parameter-interface acceptance

This is the Phase E acceptance boundary for
`16.023/parameter-schema-acceptance`. The final result is authoritative only
when `../tests/filter-parameter-acceptance/acceptance.json` records a pass.
It closes the parameter-interface stages A–E, not the parent executor task,
the full port, a release, or the remaining platform and precision gates.

## Failed definition transactions

The scoped allocation sweep found two synchronous failures in the assembled
A–D implementation. Failed construction of a Filter Undo could return `FALSE`
with an extra Undo item. Allocation of progress or native execution state
could fail after the definition model and revision had already been replaced.
Neither finding was a subsequently accepted definition failing asynchronously.
The previous completed pixels remained intact in these reproduced failures.

The definition setter now constructs its request, progress channel, native
descriptor, process options, result owner and callable captures privately.
Attachment and the caller's final guard run before that preparation. Geometry
and precision are captured after those potentially reentrant callbacks, and
the dispatcher is scheduled immediately before publication. No callback gap
can consume the old clean request's wakeup between preparation and publication.
The prepared request and its owned values are then moved into place.

A setup failure removes only this transaction's identifiable, still-top Undo
item when the binding and definition remain unchanged. It does not pop/apply
Undo or remove unrelated reentrant work. For the tested rejected preparation paths without independent reentrant mutation,
the accepted invariant is exact model/revision/raw/pixels/cache preservation and this transaction's Undo
item/depth preservation. Existing image dirty notifications and expiration of
previous redo/old history are **not** restored. That wider image-history
atomicity limitation remains open.

The extraction preserves the existing route parsing and execution policies.
Unsupported or otherwise preservable definitions still retain their values
and completed cache, with the existing executor diagnosis. It adds no new
filter, precision, wire field, acquisition worker, query protocol or cache.

## Allocation and process scope

The fault harness is a private Linux executable. It links wrappers for the
C++ `new`/`new[]` and `g_try_malloc`/`g_try_malloc0_n` symbols used by the linked
component. A thread-local scope injects one failure, then disables itself
before diagnostics and cleanup. Normal executables, dependencies, other
threads and upstream fatal GLib allocators are not intercepted. No production
fault framework or environment-controlled execution override is introduced.

The sweep covers all four registered descriptor constructors and freshness
checks; all four admitted route configurations; scalar, string, STRV,
double-array, int32-array and raw-array patches; and an actual GTK matrix-edit
response. It enumerates each reached allocation position through the first
uninjected success, checks exact model/cache/Undo invariants on rejected calls,
and verifies a successful retry and Undo. Descriptor owner reference counts,
plug-in-open signals, bounded synchronous completion and scoped `/proc` child
inventories establish that metadata failure does not leave an acquisition job.

Existing isolated process, scheduler and spool tests exercise worker failure,
crash, timeout, cancellation, incomplete results, cleanup and preservation of
committed output. Current native layer/progress tests and installed missing
runtime checks cover their integration with the previous completed cache.
Their exact selected cases and executable identities are recorded separately.

A separate child-only 128 MiB address-space limit rejects one 256 MiB GLib
request. `g_try_malloc` returns `NULL`; ordinary `g_malloc` terminates with the
upstream fatal allocation diagnostic. This does not exhaust the host, and the
fatal case is not described as component recovery.

Earlier Phase C documentation referred broadly to a future
“whole-application OOM” campaign. This result replaces that prospective wording
with the tested scope above: adopted descriptor/definition allocation failure
handling and existing worker failure boundaries. It does **not** establish
whole-application or whole-dependency OOM resilience. Such a broad campaign,
global RSS/latency guarantees, and upstream fatal-allocation policy remain
outside this bounded gate. No omitted acquisition IPC/cache is claimed tested;
Phase C deliberately did not introduce those mechanisms.

## Integrated runtime and evidence

The normal and focused sanitizer selections cover the assembled editor,
persistence, generation/cache/progress and reentry paths. Three additional
checked-setter regressions accept a TRUE guard that resizes the owner, changes
U8 to float precision, or pumps a nested loop while the previous request is
CLEAN. The changed definition must still complete exactly once with the
correct dimensions, sample format and pixels. Existing A–D malformed metadata,
typed-comparison, lifetime and provenance evidence is reused where unchanged.

The complete genuine-old four-route corpus is replayed once on the final
chosen runtime. Its non-installed test drivers use the relocated helper and
plug-ins. Counts distinguish final old pixels, raw ROI comparisons, analytic
native cases and geometry rejections. Corpus archives and members remain
immutable; generated modern XCF test wrappers are not old oracles.

Packaging uses the established prototype recipe and retains its separate
candidate/full-aggregate gates. The runtime includes installed helper,
plug-ins, data and the dependency closure, with licenses and pinned source
provenance. Parts are reassembled and restored before functional relocation
checks. The original source, build and dependency directories remain present;
an explicit minimal environment, all-ELF resolution, observed executable and
library paths, missing-helper/plugin failures and final manifest equality
establish the bounded no-executable/library-fallback result. GUI application-icon
lookups still attempted the absent compiled prefix and emitted warnings; the
actual windows, installed menus and filter flows succeeded. The final GUI
receipt additionally records a host glibc character-conversion module and two
unclassified first-Save file-chooser criticals. These are disclosed dependencies
and open diagnostics, not silently accepted by widening the observer policy. `/proc` samples are not a complete
resource-access syscall audit.

Actual application key/menu operations are a separate gate from linked-native
GTK tests and console checks. Successful startup requires observed windows
and completed flows; exit status zero alone is insufficient. The evidence
records create/edit/no-op/invalid input/Cancel/close/Save/Open for the four
schema editors, with an isolated disposable profile.

The package is a Debian 13 x86_64 test runtime requiring the stated host ABI,
Python and desktop services. It is not a universal Linux package or release.
The package predates the final evidence/documentation commit; exact tested
production and recipe hashes map it to that commit without claiming that the
archive was built from the later commit identifier.

## Gates that remain open

The independent GTK/GDK AT-SPI issue `34.003`, the wider dirty/redo history
limitation, unsupported routes/precisions, Windows/macOS, Wayland/tablet
equivalence, license/source distribution obligations and whole-port acceptance
remain open. Convolution's native high-precision support is not attributed to
the other three routes; their existing U8/Gray limitations remain explicit.
Focused ASan/UBSan instrumentation and disabled LeakSanitizer are reported
accurately rather than being described as whole-application validation.
