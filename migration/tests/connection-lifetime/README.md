# Connection ownership — original 06.017

Connection already owns a native signal handler through its ID and a weak
emitter reference. Target/emitter destruction therefore does not leave a raw
object pointer to be dereferenced by close. Each operation locks the weak
reference and checks whether the handler still exists. Close detaches all member
state before disconnecting, so a closure destroy callback can replace or delete
the Connection. Move assignment publishes incoming state before releasing the
old handler. No production change was required for this acceptance.

The new native GLib test records handler calls, closure-data destruction and
emitter finalization separately. It checks four orders: emitter first, explicit
close, Connection scope destruction, and external native disconnect. Each
closure is destroyed exactly once. A surviving emitter cannot call the released
payload; an expired emitter supports connected/block/unblock/repeated close
without access to freed storage. The emitter's reference count remains one
while connected, demonstrating that this wrapper adds no strong emitter cycle.

Move assignment disconnects the displaced handler once, moves ownership from
the source, and preserves the live handler through self-move and move
construction. Moved-from and repeatedly closed handles are harmless. Existing
regressions also exercise destroy-notify replacement during close/move assignment
and a destroy callback deleting the Connection itself.

Normal and rebuilt ASan/UBSan foundation suites each pass 54 cases. All 11 common
C/C++ units are instrumented; system GLib is not. LSan remains unavailable under
the verified executor ptrace limitation, and vptr is excluded for no-RTTI native
compilation. Explicit callback/finalization counters are not a LeakSanitizer
result. Block nesting and after-handler ordering run as existing regressions;
their original06.018 acceptance is not marked complete here.

Pinned app/base/delegators.hpp hunk 01.002/000042 stores raw target/closure
pointers in Delegators::Connection and searches that target during disconnect.
Its heap delegator is owned by GClosure's destroy notify. The modern wrapper
retains this closure ownership with native GClosureNotify, uses the weak target,
and has move-only automatic disconnect. Exact Git blob, whole-file and addition
hunk hashes are verified. Only legacy-9dce6bf45ddde79bb80b becomes DONE.

Connection does not automatically track an unrelated callback receiver. A
receiver must own/close its Connection before its data dies, or provide an
appropriate weak receiver adapter. Callers still provide the exact signal
signature, exception boundary and required owner-thread discipline. This common
ownership gate does not close every feature callback or platform test.
