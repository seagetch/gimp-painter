# Typed slot registration — original 06.008

The existing BindingStore::emplace validates the owning thread, construction
state, declared native owner GType and unique typed-slot identity before
constructing an implementation. It reserves an in-progress identity, holds the
native owner during construction and rechecks state before publication.
Production store code is unchanged.

`/painter/store/slot-registration` explicitly checks wrong native owner type,
a duplicate installed slot, a new slot after activation, and both new/duplicate
slots after close. Rejected calls never construct or destroy an Impl, never
add a lasting native reference, and preserve the installed value, store state
and generation. Rejected slots remain absent. One valid implementation remains
alive until the owner's normal final release. The test shares the existing
native PainterFixture TypeTraits declaration through a small C++ test header;
production type declarations and C ABI do not change.

The same execution includes existing multiple-slot isolation and constructor
failure regressions. Constructor-driven duplicate registration and activation
are rejected; closing during construction rejects publication and cleans the
unpublished Impl; loss of the caller's last owner reference is protected by the
construction lease. These defensive cases do not authorize feature constructors
to emit callbacks contrary to the construction contract.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 47 cases, with empty sanitizer stderr. All 11 component C/C++ units
are instrumented; system GLib is not. LSan remains unavailable under the
verified ptrace limitation and vptr is excluded for native no-RTTI compilation.
Explicit lifetime counters are not an LSan result. The independent standalone
runner also passes 47 cases and its existing negative borrow-escape compilation
checks; its source-hash list now includes the shared test trait header. The
task-specific archive already captures that header through include traversal.

Work item `legacy-15d2efcd06129448566f` maps pinned glib-cxx-impl.hpp hunk
`01.002/000045`. The old NewGClass template binds CStructs and Impl, registers
native instance-private storage, and constructs one Impl in instance_init.
The modern SlotSpec binds owner and implementation types together, with the
slot's unique C++ type defining identity inside the shared store. Runtime
checks reject incompatible owner types, duplicate identities and registration
outside construction. The exact pinned Git blob and separate whole-file/hunk
hashes are verified. Only this source row's mutable execution fields become
DONE; other property, vfunc, interface, lifecycle and feature obligations remain
separate. Owner-thread and explicit construction/installation rules still apply.
