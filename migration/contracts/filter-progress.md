# Independent FilterLayer progress and cancellation

This is the bounded `15.006/native-owner-progress` migration slice implementing
the runtime/ABI work referenced by `15.006/filter-progress-start`. That original
row retains its broader uncompleted prerequisites. It does not
close the parent FilterLayer, global responsiveness, general procedure, platform,
or release gates. The progress state is transient: saved definitions, argument
shapes, original payload bytes, XCF records and completed drawable caches are
unchanged.

## Old behavior and current interface

The pinned old `afa43fae3e920210146abed514f136fd49f671b5` FilterLayer
(`app/core/gimpfilterlayer.cpp:753`) returned itself from start, retained a scalar
value, and logged text/pulse/message callbacks. Its `end()` notified its runner,
then either replaced a pending runner or updated the drawable/projection/image.
The 300 ms timeout was a separate no-runner branch, not the ordinary progress-end
path. Old `app/pdb/pdb-cxx-utils.hpp` runner stop/destruction emitted the native
progress cancel signal; the native plugin bridge then closed the plugin. No old
shared display/statusbar forwarding or serialized progress field was found in
the layer or inspected layer renderer/popup paths. This checkpoint adds useful
visible progress to the existing GTK Filter editor; it does not claim a global
statusbar implementation or byte-for-byte old UI placement.

The modern layer implements the real `GimpProgress` interface with a typed
`start(GimpProgress*, gboolean cancellable, const gchar *message)` trampoline.
The optional modern `GBytes* get_window_id` vfunc remains unset. Every C/vfunc
entry borrows the existing `FilterSlot` through the common BindingStore and
contains exceptions; there is no second implementation store or GObject owner.
An already-active start returns null. End, text, value, pulse, active/value
queries and message handling respect the active generation. Nonfinite fractions
are ignored; valid fractions are clamped. The cancel signal's run-first handler
cancels only the cancellable current progress generation. A separate checked
cancel entry rejects stale UI generation tokens.

## Independent work and owner delivery

Each configured executor captures one owned `FilterProgress` mailbox containing
only fixed-size C++ data. The scheduler resets it after the previous worker has
actually completed, before admitting another generation. Neither jobs nor the
mailbox hold a GObject, an owner pointer or a UI callback. Text/message scans
are bounded to 512 bytes and domains to 128; complete UTF-8 is retained and
malformed input is sanitized. The mailbox coalesces values/text and one latest
message rather than accumulating events. Owner reads use `try_lock`, copy one
fixed snapshot or skip that observation, and never wait for a producer.

Native byte/real Edge, Gaussian and point kernels report actual completed
strips, scanlines and chunks. Gaussian transpose reports phase boundaries.
Identity reports copied work. The owner maps bounded input preparation to
0..15%, executor work to 15..85%, and private result import to 85..100%; these
fractions represent work phases, not time estimates. A value of 100% or a helper
progress-end is never proof of successful execution or permission to publish.
The original exact result size, terminal result, child successful exit/reaping,
context, lifetime and generation gates still decide cache publication.

The existing priority-150, 2 ms fair dispatcher owns delivery. It emits at most
one fixed progress snapshot per step, normally at 100 ms intervals plus phase
transitions/start/end. A dispatch guard blocks nested owner stepping. After
progress/state observers reenter, both binding liveness and scheduler generation
are checked before continued work; old/cancelled/replaced generations cannot end
or update a replacement's session. Close invalidates state without waiting or
notifying released owners. Native calls and GTK updates do not drain the main
context. No runner executes from a GEGL process callback, and the previous
completed cache remains the only drawable source while work is pending.

The existing editor displays the progress text/fraction and the latest bounded
message, with a Cancel Update button. The button stores the surviving rendered
generation, and uses native `gimp_progress_cancel`. Every GTK setter is treated
as a reentry boundary: a render revision plus layer generation prevents an
outer stale callback from overwriting a replacement, redisplaying a cancelled
bar or publishing a newer cancel target. Retained controls after dialog close
cannot call a freed implementation.

## Private helper transport

GPF6 / `--filter-worker-v6` retains the 24-byte frame header and 344-byte request.
The six bounded progress frame kinds carry start/cancellable/text, end, text,
IEEE754 fraction, counted pulse and severity/domain/message. Header size/type/
version, payload length, canonical UTF-8, finite [0,1] values, severity, contiguous
64-bit event sequence, pulse overflow, active lifecycle and terminal ordering
are validated before publication. Pixel and terminal validation remain exact.
This is a private protocol version change, not a saved-file or PDB ABI change.

The child uses one bounded pending slot per update kind and coalesces normal
updates at 50 ms intervals. A child-local emission budget bounds lifecycle abuse;
parent arrival timing is deliberately not used as a rejection criterion because
a valid pipe backlog may arrive in a burst. The supervisor reads at most one
bounded chunk/frame per iteration and checks cancellation between iterations.
A hostile unlimited frame stream cannot grow owner memory or add UI-thread I/O.
Tests cover 4,096 queued reports after a paused receiver and cancellation during
an unlimited valid-report flood.

The helper's common-store native progress adapter forwards real callbacks from
all four existing Blinds/Small Tiles/Retinex/Convolution routes. The four PDB
init/update/pulse/text guards bypass headless suppression only for the isolated executor's typed
private adapter. Null and ordinary supplied GimpProgress objects retain the
normal headless behavior; no environment variable enables an exception. Helper sink errors are retained at the C boundary and fail the
procedure after it unwinds. Parent cancellation still terminates the process
group, retains exclusive child identity until reaping, and cleans the private
profile. Lifeline, bounded pipe buffers and unsupported Windows gate remain.

## Evidence and remaining limits

`migration/tests/filter-progress/acceptance.json` records final source/binary
identities, compact raw logs, normal and focused instrumentation results, real
worker observations and desktop checks. Focused ASan/UBSan is not whole-program
or dependency instrumentation. The compiler's registered C++ RTTI-only closure is
reported separately; LeakSanitizer is disabled.

Shared-host timing observations are not global latency certification. Tiny
kernels may finish between owner samples, so deterministic kernel checkpoint
tests complement native owner observations. Old unsupported procedures, other
platforms, no-swap alternatives, broad precision/format requirements and all
unrelated parent gates remain open. Native test profile/localization diagnostics
and any manual GTK/GDK diagnostics are recorded separately from assertions.
