# Original WBS 07.001: retain, adopt and sink acceptance

This verification item checks the three explicit GObject ownership factories.
The dedicated tests already added for original 06.001–06.003 satisfy its exact
count and release condition. The current 65-case native, ASan/UBSan and Meson
runs in [construction-failure](construction-failure/README.md) execute all three
tests. Their complete source seals and PASS lines were rechecked against this
commit. The measured results are reused without changing historical hashes,
duplicating the tests or claiming another execution.

| Test | Acquisition and intermediate ownership | Final release |
|---|---|---|
| `/painter/ref/retain-factory` | GObject and GInitiallyUnowned: same pointer, 1→2, floating state unchanged; original caller release leaves 1 | Reset triggers exactly one weak finalizer; repeated reset has no additional effect |
| `/painter/ref/adopt-factory` | Fresh owned reference stays at 1, floating state unchanged; separately produced owned reference stays at 2, wrapper release leaves caller at 1 | Wrapper destruction or final caller unref triggers exactly one weak finalizer |
| `/painter/ref/sink-factory` | Floating reference becomes nonfloating at 1; nonfloating object becomes 2, original caller release leaves 1 | Reset triggers exactly one weak finalizer; repeated reset has no additional effect |

All factories also accept NULL as an empty handle. Counts are inspected only
while the private, single-threaded test object is alive. Zero references are
observed through the weak finalizer; no freed object is dereferenced to read its
count. The floating adoption case explicitly sinks the already-adopted reference
for cleanup after checking that adoption itself preserved its state.

The source duty `legacy-ffa437d99bd68c45f014` belongs to old
`app/base/glib-cxx-utils.hpp`, hunk `01.002/000047`, Git blob
`a1a77acd0ebcba4ad5f400bfe362628259788922`. The exact source already retained in
the [06.001 evidence](object-ref-retain/README.md) supplies adopting `hold(T*)`
and reference-incrementing generic GObject `ref(T*)`. It has no explicit sink
factory. The new sink path intentionally exposes GTK/GLib's native floating
ownership contract. This does not assign owning semantics to every old `ref`
overload or accept the separate delayed-callable and feature-caller duties.

`reference-factories.json` records exact source/report seals and the one ledger
update. System GLib is not sanitizer-instrumented and LSan was not run. Earlier
GTK widget evidence remains historical; it is not presented as a new GUI run.
Copy/move, invalid types, slot lookup and cross-platform checks remain separate
original WBS items.
