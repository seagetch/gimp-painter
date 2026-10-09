# ObjectRef copy/move — original 06.004

The existing production implementation satisfies the original reference-count
contract: copying acquires one owned reference; moving transfers the pointer
without acquiring a reference and empties the source. Assignments publish the
new handle by temporary/swap before releasing the displaced owner. Production
ObjectRef code is unchanged.

The dedicated `/painter/ref/copy-move-contract` test uses both G_TYPE_OBJECT
and G_TYPE_INITIALLY_UNOWNED. It checks pointer identity and exact reference
counts after copy/move construction, copy/move assignment, nonempty self-copy
and self-move, copying/moving empty values, distinct wrappers holding the same
pointee, and replacement of a different owned object. Displaced objects finalize
once. Floating state survives copy/move unchanged. The remaining reference is
released once at the end. Compile-time checks verify the four operations remain
noexcept. Counts are read only on private single-threaded test objects.

The existing `/painter/reentry/ref-move` regression is expanded to exercise
both copy and move assignments. A displaced object's finalization callback
observes the already-published incoming value and replaces it again. The final
handle keeps that replacement; the incoming owner is preserved for copy and
emptied/released for move. Explicit counters and counts verify exactly-once
release of the old object, incoming object and replacement. This confirms that
returning from the outer assignment does not overwrite reentrant state.

The rebuilt real Meson foundation and fully rebuilt common-component
ASan/UBSan suites each pass 44 cases. Sanitizer stderr is empty. The component's
11 C/C++ units are instrumented, system GLib is not. LSan remains disabled under
the verified executor ptrace limitation and vptr is excluded for native no-RTTI
compilation. Explicit lifetime counters are not an LSan result.

Work item `legacy-4fe03f5e208bd2392516` maps pinned old hunk
`01.002/000047`. Object(Object&&) transfers `src.obj` and sets it to NULL;
IObject's copy constructor calls incref, and its copy assignment replaces the
pointer and increments the incoming reference. These ownership roles map to
modern ObjectRef copy/move. Old assignments release the previous pointer before
publishing the new one, whereas the unified wrapper explicitly protects
reentrant finalizers through publish-before-release. No obsolete raw-pointer
assignment interface is introduced. The archive preserves verified old source,
Git blob, whole-file and hunk digests. Only this source work item's mutable
execution fields become DONE. Type checks, borrowing, other handle kinds,
feature callers and platform gates remain separately tracked.
