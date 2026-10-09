# ObjectRef sink factory — original 06.003

The existing production `ObjectRef::sink` calls native `g_object_ref_sink`
after its type check. Its original acceptance is now verified directly with
GObject and real GTK 3 widgets; production factory code is unchanged.

`/painter/ref/sink-factory` checks NULL, pointer identity, and native
G_TYPE_OBJECT / G_TYPE_INITIALLY_UNOWNED ownership. A floating reference is
consumed at count 1 and loses its floating flag; an already-owned object gets
one additional reference, count 1 to 2. Releasing the nonfloating caller's
original reference leaves the wrapper alive. Reset finalizes once, and a
second reset has no effect. Private single-threaded objects make counts exact.
The real Meson foundation suite and rebuilt ASan/UBSan suite pass 43 cases
each; sanitizer stderr is empty.

A separate compiled probe directly includes the same production header and
uses real GtkButton/GtkBox subclasses. Eight cases pass with GTK 3.24.49,
GLib 2.84.4 and `G_DEBUG=fatal-warnings` on an existing display. They cover
floating and nonfloating sink, either owner's release order, container add
before and after sinking, child removal with either wrapper lifetime, and
container destruction before and after wrapper release. Destroy-signal and
GObject finalizer counters are independent: a destroyed widget may stay alive
through a wrapper, and every object finalizes exactly once. This is component
ownership evidence, not full application GUI acceptance.

Work item `legacy-bfc808c9a4d58febb203` belongs to pinned old addition hunk
`01.002/000047`. The archived old glib-cxx-utils.hpp has adopting Object(T*) /
hold(T*) and reference-incrementing IObject / generic GObject ref(T*), but no
sink factory or g_object_ref_sink call. The new explicit sink factory supplies
the native GTK floating-transfer contract required by this WBS item; an old
sink implementation is not claimed. Other old ref overloads and saved
callables retain their separate obligations. The old file's Git blob and
separate whole-file/hunk digests are verified. Only this work item's mutable
execution fields become DONE; its identity/acceptance and other rows stay
unchanged.

The evidence archive includes exact test source, commands, output, production
header/source hashes, native binary hashes, and the pinned old source. No
screenshots or personal profiles are included. The GTK probe is not sanitizer
instrumented. The common foundation sanitizer build instruments its 11 C/C++
units, not system GLib; LSan remains disabled for the verified ptrace
limitation and vptr is excluded for native no-RTTI compilation. Counts and
finalizers provide explicit ownership evidence, not an LSan result. Copy/move,
type checks, feature callers and platform gates remain separate.
