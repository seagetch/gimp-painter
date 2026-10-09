# BindingStore synchronous call leases — original 06.012

initialize, with and read retain a native owner for the entire synchronous
callback. Since that owner owns the store and its registered implementations,
reentrant close and release of the caller's native reference cannot destroy
the Impl while its callback is on the stack. The lease releases on both normal
return and exception unwinding. Production code is unchanged.

`/painter/store/call-leases` exercises eight paths: construction initialize,
active with, active const read and already-closed const read, each returning
normally or throwing a controlled exception. At callback entry, the owner has
two references. The callback closes the store and drops the caller's reference;
one lease reference remains. It observes a live native owner, live Impl,
exactly one close, no destructor and a readable value on the Impl still in use.
After return/unwind, both native owner and Impl have finalized exactly once.
The test never uses old store or Impl borrows after the call has ended.

This verifies memory lifetime during synchronous reentry. It does not make a
logically closed object mutable or permit an Impl borrow to escape. Existing
compile-time borrow checks, close/generation gating and asynchronous job
ownership retain their separate roles.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 51 cases, with empty sanitizer stderr. All 11 component C/C++ units
are instrumented; system GLib is not. LSan remains unavailable under the verified
ptrace limitation and vptr is excluded for native no-RTTI compilation. Explicit
reference/lifetime counters are not an LSan result.

Work item `legacy-c4941a88412032986d75` maps old glib-cxx-impl.hpp hunk
`01.002/000045`. Old Binder::callback obtains get_private(obj) and directly
invokes the Impl member; that common dispatch supplies no local strong owner
lease. The new shared call paths explicitly protect the owner/Impl while a
synchronous callback runs. This closes the shared protection criterion, not
every old vfunc adapter, feature callback or asynchronous capture. Exact old
source, Git blob and separate whole-file/addition-hunk hashes are verified.
Only this source row's mutable execution fields become DONE. Owning-thread,
hierarchy and feature-specific lifetime requirements remain separately tracked.
