# WeakRef ownership and expiration — original 06.006

The existing production WeakRef owns a GWeakRef state, not a strong GObject
reference. `lock()` uses native `g_weak_ref_get` and adopts the returned owned
reference. Default, reset, moved-from, null-owner and expired handles return
empty. Production wrapper code is unchanged.

The expanded `/painter/ref/weak` test measures native reference counts using
ordinary G_TYPE_OBJECT and G_TYPE_INITIALLY_UNOWNED instances. Creating multiple
weak handles, destroying a scoped weak handle and moving a weak handle all
leave the sole strong reference at 1. The first lock raises it to 2; a second
independent lock raises it to 3 and releases back to 2 on scope exit. Floating
state is unchanged by acquisition. Dropping the original owner leaves the
strong lock alive at count 1. Resetting a weak observer does not release that
strong lock. Final strong release finalizes exactly once, and repeated locks
of the remaining weak handle return empty. Reset after expiration is safe.

A separate two-object case verifies move assignment replaces only the weak
observation, leaves both native owner counts at 1, clears the source and
survives self-move. The moved destination locks the new object, not the old
one. Both objects finalize once when their actual owners release them.
Compile-time checks keep WeakRef move-only with noexcept moves. All count
inspection is on private objects on one thread.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 45 cases, with empty sanitizer stderr. This strengthens the existing
case rather than adding duplicate test names. The 11 C/C++ component units are
instrumented, system GLib is not. LSan remains unavailable under the verified
ptrace limitation, and vptr is excluded for native no-RTTI compilation. Explicit
reference counts/finalization counters are not an LSan result.

Work item `legacy-f9662dfed577adaac38f` maps old utils hunk `01.002/000047`.
The pinned header has owning Object/IObject wrappers and raw-pointer-storing
BoundMethod/ConstBoundMethod objects; it contains no GWeakRef primitive. The
new weak wrapper makes deferred lifetime acquisition explicit. This closes the
shared weak-handle implementation criterion, not all raw saved-callable or
asynchronous caller migration. Those callers still need weak/generation checks
and the specified owner context. No arbitrary cross-thread GTK safety is
inferred from GWeakRef. Exact old source, Git blob and distinct file/hunk
hashes, compiled current sources, commands and results are retained in the
archive. Only this row's mutable execution fields become DONE.
