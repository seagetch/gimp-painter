# ObjectRef adopt factory — original 06.002

The existing production factory consumes an already-owned GObject reference
without adding another reference or sinking floating state. The dedicated
`/painter/ref/adopt-factory` native test now checks that original acceptance
directly; production ObjectRef code is unchanged.

For a fresh G_TYPE_OBJECT and G_TYPE_INITIALLY_UNOWNED, the returned handle
has the same pointer and count exactly 1. Floating state is checked before
any explicit test cleanup sink. Wrapper destruction produces one weak-finalize
notification. NULL produces an empty handle. A separate case calls the native
C `g_object_ref` producer first: adopt leaves the two owned references at count
2, wrapper destruction leaves the caller's reference at count 1, and releasing
the caller finally destroys the object. These cases distinguish adopt from both
retain and sink. Count inspection uses private single-threaded test objects.

The rebuilt real Meson foundation suite and rebuilt ASan/UBSan suite each pass
42 cases. The sanitizer run has empty stderr. LeakSanitizer remains disabled
because of the executor's previously verified ptrace limitation, and vptr is
excluded for native no-RTTI compilation; system GLib is not instrumented. Exact
counts and weak notifications are explicit lifetime evidence, not LSan results.

Work item `legacy-6a12bdef2f7c72ec10a3` maps the same pinned addition hunk
`01.002/000047` to this separate adoption condition. In old glib-cxx-utils.hpp,
Object(T*) at line 854 stores the supplied pointer without incrementing it, and
hold(T*) at 875 constructs that adopting owner. Its ScopedPointer base releases
through g_object_unref. This matches ObjectRef::adopt's private owning
constructor and final release. Old generic GObject ref(T*) acquires a reference
instead; other overloads and saved callables retain their separate obligations.

The archive includes the old source and its distinct whole-file and hunk
digests, already verified against Git blob a1a77acd0ebcba4ad5f400bfe362628259788922.
The source work item's immutable identity/acceptance and all other rows remain
unchanged. Only its mutable execution evidence becomes DONE. Later sink,
copy/move/type-check and caller acceptance gates are not closed by this result.
