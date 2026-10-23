# Original WBS 07.002: copy, move and empty handles

The original condition requires self-assignment and empty handles to remain safe
without double release. Existing dedicated tests cover this condition on the
current ObjectRef implementation. All source seals and relevant PASS lines in
the current [65-case native, sanitizer and Meson reports](construction-failure/README.md)
were checked again; these measurements are reused without another runtime run.

`/painter/ref/copy-move-contract` checks GObject and GInitiallyUnowned instances:

- Copy construction acquires a reference; move construction empties its source
  without changing the count
- Nonempty self-copy and self-move preserve the pointer and count
- Copy/move assignment releases a different displaced object exactly once
- Distinct wrappers already owning the same pointee preserve balanced counts
- Empty construction, copying, moving and assignment leave empty sources or
  destinations as appropriate and release displaced references
- Floating state remains unchanged; the final owned reference releases once
- All four copy/move operations retain their compile-time noexcept contract

`/painter/reentry/ref-move` exercises both assignment forms with an actual native
weak-finalization callback. The callback observes the already-published incoming
object, then replaces it again. The outer assignment preserves that replacement.
Exact counts and separate finalizers observe one release of the displaced,
incoming and replacement objects. The general copy/move/adopt/retain test also
covers repeated reset and empty-handle operations.

Source duty `legacy-a47565c525609aa2a77b` refers to old
`app/base/glib-cxx-utils.hpp`, hunk `01.002/000047`, blob
`a1a77acd0ebcba4ad5f400bfe362628259788922`. Object's move constructor transfers
its pointer and nulls the source (line 855); IObject's copy constructor and
assignment increment the incoming reference (891–893, 943–949). These ownership
roles map to ObjectRef's explicit operations. The old assignment releases its
previous object before installing the incoming value. The current wrapper's
temporary/swap sequence preserves reentrant state; the obsolete raw-pointer
assignment interface is not reintroduced.

`reference-copy-move.json` seals this one duty and reused report identities.
No production change, new test execution, LSan, system-GLib instrumentation or
cross-platform result is claimed. Other wrapper kinds and feature callers retain
their separate source-specific acceptance duties.
