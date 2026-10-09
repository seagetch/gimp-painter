# Typed slot lookup without implicit creation — original 06.009

The existing BindingStore::lookup searches registered slot identities and raises
MISSING_SLOT when absent. It has no registration, allocation or constructor
path. initialize, with and read use that lookup after their native owner lease
and appropriate state checks. Production implementation is unchanged.

`/painter/store/slot-lookup` uses a default-constructible implementation with
explicit constructor, live-object, close and destructor counters. Missing
initialize/read during construction invoke no callback and construct no Impl.
The same identity can then be explicitly emplaced, and only that operation
increments the constructor count. A second distinct slot of the same Impl type
remains missing: with/read during active state and read after close reject it
without constructing anything or calling its callback. The valid stored value
remains readable and unchanged.

At each phase the test checks native owner reference count, store state and
generation. Failed lookup leaves each unchanged. The sole explicit Impl remains
alive through close and is destroyed once on owner finalization. Default
constructibility makes the no-creation observation a runtime condition, not an
accidental inability to compile a hypothetical implicit constructor call.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 48 cases, with empty sanitizer stderr. All 11 component C/C++ units
are instrumented; system GLib is not. LSan remains unavailable under the verified
ptrace limitation and vptr is excluded for native no-RTTI compilation. Explicit
construction/lifetime counters are not an LSan result.

Work item `legacy-53c606ef6c50c453b661` maps pinned glib-cxx-impl.hpp hunk
`01.002/000045`. Old NewGClass constructs its Impl in instance_init, while
get_private retrieves native instance-private memory and Binder::callback
resolves it before dispatch. The new typed lookup similarly resolves previously
registered state, adds explicit missing-slot failure and does not reuse a lazy
creation path. No claim that the old get_private itself lazily constructed an
Impl is made. The pinned source, Git blob and separate whole-file/addition-hunk
hashes are verified. Only this row's mutable execution fields become DONE;
other adapter, state, borrow, callback and feature-caller requirements remain
separate. Valid owning-thread/lifecycle preconditions continue to apply.
