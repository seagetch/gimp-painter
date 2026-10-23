# Original WBS 07.009: invalidate already queued callbacks

The existing tests canceled a Source before dispatch, checked store generations
synchronously, and checked weak references independently. They did not prove that
an already attached callback actually enters after invalidation without changing
its model. This task adds that combined regression, without production changes.

`/painter/source/queued-callback-invalidation` connects separate native GObject
emitter and receiver objects. A real notify callback attaches a Source to a
private GMainContext. The Source intentionally remains alive after the receiver
closes and disconnects, so suppression cannot pass merely by canceling delivery.
The queued ticket owns a WeakRef and generation, never a strong receiver or Impl.

Both idle and zero-timeout sources run five scenarios:

- Active positive control: one queued callback actually dispatches and changes
  model value 41 to 42 exactly once.
- Closed owner: closing advances generation once and disconnects native signals;
  the already queued callback still enters, locks the surviving owner, rejects
  admission, and leaves value 41 unchanged. A new emission queues nothing.
- Finalized owner: native weak notification and Impl destruction both happen
  before dispatch; the queued callback enters, fails its weak lock, and never
  accesses the store.
- Mismatched generation: the owner stays active with its original generation;
  a different ticket is rejected. This isolates generation equality from state.
- Closing reentry: the close hook iterates the real context after disconnect;
  delivery observes State::closing and makes no mutation.

Each scenario requires one delivery, one ticket destruction, no second delivery,
an inactive completed Source, and one close, Impl destruction and native
finalization. The queued owner refcount remains one before close, proving the
queue does not retain it. The adapter checks weak owner and lifecycle generation
before a protected `with` borrow. `Connection::close` alone does not invalidate
an arbitrary independent queue while its owner remains active. Feature-specific
revision/supersession rules remain separate.

`native.json`, `sanitizers.json` and `meson.json` each record 68 passing tests and
25 exact current source hashes. The standalone runner compiles C fixtures as
C11 and C++ adapters as C++14. ASan/UBSan cover this component; LSan, instrumented
system GLib, whole-application and cross-platform coverage are not claimed.
Run `tools/test_painter_foundation.py --build-dir DIR --report FILE` and repeat
with `--sanitize`; the existing Meson `painter-foundation` target runs the same
fixture. `validation.json` records this task's seals and source duty.

The one legacy duty is `legacy-eb0569e7338d2c0a2a18`, hunk `01.002/000042`,
app/base/delegators.hpp, blob `f615f41b4fdebbebc865e17888eadc38a0c45e58`.
The complete fixed source already exists in gtk-binding/caller-census.tar.gz,
member `source/app/base/delegators.hpp`: invocation lines 32–48, raw receiver
65–74, disconnect 135–143, closure registration 167–191. It implements synchronous
signal plumbing, not a separate event queue. The modern Source plus weak-owner
and BindingStore generation admission composition provides the queued boundary.
Representative adapters already compose these primitives in
`gimppaintercanvasui.cpp`, `gimppainterlayertiles.cpp`, `gimpfillbrushtool.cpp` and
`gimppaintermybrusheditor.cpp`; this shared regression does not close their
independent feature behavior obligations.

Historical report hashes remain fixed. Their source mismatch after this test
addition is reported separately from this current task's passing evidence.
