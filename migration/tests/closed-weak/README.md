# Original WBS 07.011: a live weak owner may already be closed

A successful weak lock establishes native lifetime, not permission to update.
`WeakRef::lock` adopts the owned reference returned by g_weak_ref_get. A queued
adapter must additionally check active lifecycle and the intended generation;
`BindingStore::with` also rejects closed/closing stores directly. Const `read`
remains available during logical close for teardown and persisted state, and
does not grant mutable access or reopen the store.

Current evidence already meets this original acceptance. The immediately prior
boundary-conversion native, ASan/UBSan and Meson reports each contain 69 passing
cases. All 25 recorded source hashes still match this checkout. This task reuses
those measurements without rerunning the unchanged suite or adding a duplicate
test. `validation.json` records the exact report hashes and current comparison.

- `/painter/source/queued-callback-invalidation`: actual native notify enqueues
  idle/zero-timeout work before close/disconnect. The closed scenario retains the
  sole external owner, dispatches the Source once, successfully weak-locks it
  (expired=0), rejects lifecycle admission (rejected=1), and leaves the model at
  41 with mutation count0. New emission after disconnect queues nothing. Native
  finalization occurs only after the retained owner is released. Positive control
  changes the model once; expired-owner, mismatch and nested-closing controls
  distinguish lifetime, generation and state.
- `/painter/store/lifecycle`: direct mutable entry after close returns CLOSED;
  the surviving Impl remains readable at its prior value, with close1/destroy0
  until final owner release. This is independent of queued admission.
- `/painter/store/close-transitions`: both initially constructing and active
  stores invalidate generation before close hooks, reject direct entry while
  closing, and reject even the current generation while closed. Repeated close
  cannot reactivate them or destroy retained Impl early.
- `/painter/ref/weak`: real reference counts distinguish weak observation from
  temporary strong locking; final release expires later locks, including native
  floating objects and moved/reset handles.

The single source duty `legacy-eedaf9eaab471cc55723` maps old utils hunk
`01.002/000047`, app/base/glib-cxx-utils.hpp blob
`a1a77acd0ebcba4ad5f400bfe362628259788922`. Full pinned bytes are already in
`gtk-binding/caller-census.tar.gz`, member `source/app/base/glib-cxx-utils.hpp`.
The old source has owning Object/IObject wrappers at848–894 and raw-pointer
BoundMethod/ConstBoundMethod at953–986; it has no GWeakRef/lifecycle-generation
primitive. The current weak and store composition supplies this common migration
condition; old owning or raw wrappers must not be mislabeled weak handles.

No production or test code changes are needed for this task. This does not close
all feature-specific raw callable, revision or queued-owner audits, and does not
claim arbitrary cross-thread GTK access, LSan, instrumented system GLib or other
platform acceptance. Historical report seals remain unchanged.
