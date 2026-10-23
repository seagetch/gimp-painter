# ObjectRef retain factory — original 06.001

The existing production `ObjectRef<T>::retain` satisfies the original condition:
acquire exactly one reference when receiving a valid borrowed GObject. This
checkpoint adds a dedicated native acceptance test and closes the one associated
source obligation; it does not change the factory or claim all handle users are
ported.

`/painter/ref/retain-factory` uses actual GLib GObjects, not a mocked ref function.
For G_TYPE_OBJECT and G_TYPE_INITIALLY_UNOWNED it checks the original pointer,
exact count 1→2, unchanged floating state, and count 1 after the original caller
releases its reference. The retained object stays alive until reset and emits
one weak-finalization callback. A second reset does not finalize again. The
floating object's original caller can sink its own reference after retain,
without adding a third reference. NULL produces an empty handle safely.
Ref-count inspection is limited to these private, single-threaded test objects.

The real Meson foundation target rebuilt and passed all 41 cases. A separately
rebuilt ASan/UBSan foundation executable also passed all 41 cases with empty
stderr. The C bridge/test fixtures remain compiled as C and linked with the
actual C++ implementation. LeakSanitizer is disabled due the previously verified
executor ptrace restriction, and UBSan vptr is excluded for the native no-RTTI
build. Weak notifications and exact counts are explicit lifetime observations,
not a LeakSanitizer claim. System GLib itself is not sanitizer-instrumented.

## Source-specific mapping

Work item `legacy-c7d76423937f98cbd063` refers to addition hunk `01.002/000047`
in [glib-cxx-utils.hpp at the pinned old commit](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/glib-cxx-utils.hpp).

The old generic GObject `ref(T*)` at lines 1084–1086 constructs IObject, whose
constructor at 887–889 calls `Object::incref()` and hence g_object_ref. It owns
one additional reference and does not sink floating state. Old `hold(T*)`
adopts instead. Other overloads, including GValue/list views, and saved
BoundMethod callables have separate borrowing rules; there is no blanket claim
that every spelling of old ref is owning or borrowed.

Current retain performs its type check before one g_object_ref, stores the
result in the private owning constructor, and releases it once through reset or
destruction. This is the matching GObject acquisition path. Adopt, sink,
copy/move, wrong-type behavior and delayed-callable lifetimes retain their own
WBS acceptance obligations.

The archived old source is 35,597 bytes and matches Git blob
`a1a77acd0ebcba4ad5f400bfe362628259788922`. Its whole-file SHA-256 is
`0f21fae6457846eda2b35e742eb3d56435b3f41b88ac5a4d0633ae25d80ede98`.
The separate addition-hunk payload, including each leading `+`, matches ledger
SHA-256 `3ce98d09f1e7941aa2c25efda3df96329948aea77c01d50ea52e434b041e1193`.
These two kinds of digest must not be interchanged.

`report.json` and `evidence-manifest.json` seal the source, commands, native and
sanitizer output, and old-source provenance. Only this work item's mutable
execution fields become DONE; its source identity and original acceptance are
unchanged. Other duties assigned to the same old header remain separate.
