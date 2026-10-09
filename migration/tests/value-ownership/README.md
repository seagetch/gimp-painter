# GValue ownership and assignment — original 06.020

Value owns an inline GValue and unsets its payload once. ValueView borrows a
const native GValue pointer without taking ownership; copy explicitly creates
an owned value using native g_value_copy semantics. An external C stack or heap
GValue shell stays with its producer. It must be unset and, for heap storage,
freed by that producer. The wrapper never frees an external shell. This explicit
split replaces the old HoldValue/ManagedValue/IValue ownership flags.

Native tests cover both stack and heap C GValues. The borrowed pointer identity
and unchanged object reference count are checked; borrow destruction does not
unset anything. An explicit copy owns a distinct GValue and reference, survives
producer unset/free, and releases the object once. Empty/null input and repeated
reset are safe. A copied string remains unchanged when its source is modified.

A registered native boxed type records each allocation, copy and free. Its
owned copies, displaced assignment target, copy assignment, self-copy, self-move,
move assignment/construction and empty assignment are checked at each stage.
Every allocated payload is freed exactly once, including the displaced type.
Existing object/string reassignment tests remain. Finalizer reentry now covers
copy assignment as well as move/reset, preserving the replacement Value created
by the old object's finalizer. Inline GValue storage requires no separately
allocated wrapper shell; boxed copy behavior is determined by its registered
native copy function, not an unconditional deep-copy promise for all GTypes.

The rebuilt native and common-component ASan/UBSan suites each pass 57 cases.
All 11 common C/C++ translation units are instrumented; system GLib is not.
Explicit allocation/finalization counters are not a LeakSanitizer result. LSan
remains unavailable under the verified executor ptrace limitation, and vptr is
excluded for native no-RTTI compilation. Borrowed pointers cannot outlive their
source, and invalid arbitrary C storage is outside the wrapper contract.

Pinned glib-cxx-utils.hpp hunk 01.002/000047 contains g_value_finalize ownership
flags and CopyValue. Its assignment bodies overwrite the previous GValue without
unsetting it; the move assignment also discards the allocated source shell. The
current Value uses copy/move temporary-and-swap, and reset detaches member storage
before native unset so finalizers may reenter safely. No production change was
required here. The tests establish the replacement contract; this checkpoint
does not claim a freshly executed legacy failure oracle.

Exact old blob, whole-file and hunk digests are verified. Original06.020 and its
already-listed06.020/value-assignment child are accepted together for this one
wrapper. Only their two source rows become DONE:
legacy-50dc658d93c7efe01274 and legacy-9bb512eb16965e63a1ee.
No new child task is added; the original WBS count increases by one. Other value
users, feature behavior and platform gates retain their own acceptance.
