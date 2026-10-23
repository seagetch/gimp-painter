# ObjectRef GType checking — original 06.005

The existing production factories validate a live GObject pointer against
`TypeTraits<T>::type()` with `G_TYPE_CHECK_INSTANCE_TYPE` before any retain,
adopt or sink operation. An incompatible GType raises WRONG_TYPE. This is a
native GType ancestry/interface check; arbitrary invalid or freed addresses
remain outside the caller contract. Production ObjectRef code is unchanged.

The expanded `/painter/gobject/type-rejection` test covers all three factories
with NULL and incompatible live G_TYPE_OBJECT / G_TYPE_INITIALLY_UNOWNED
instances. Failed validation leaves the original reference count at 1,
preserves floating state and causes no finalization. The same failures through
the C++ boundary produce the expected native GError domain/code. The caller
then releases its still-owned reference exactly once. Existing store wrong-type
rejection remains covered separately.

`/painter/hierarchy/type-ancestry` verifies all three factories accept an exact
child type, its native base, an implemented interface and GObject. An adopt
success receives an independently acquired native reference; retain and sink
acquire their own. Every returned handle has the original pointer, count 2
while owned and count 1 after release. A base instance is rejected as a child
and as an unimplemented interface with its reference unchanged. The native
fixture's base/child implementation counters confirm final destruction.

The rebuilt native foundation and fully rebuilt common-component ASan/UBSan
suites each pass 45 cases. Sanitizer stderr is empty. The 11 component C/C++
units are instrumented; system GLib is not. LSan remains unavailable under the
verified ptrace limitation and vptr is excluded for native no-RTTI compilation.
Reference counts and finalization counters are not an LSan result.

Three pinned source obligations close only the shared handle type-check role:

- `legacy-31b15077c686c97dca6f`, glib-cxx-bridge.hpp: the old catalog maps native
  classes/interfaces to GTypes. Modern TypeTraits supplies the expected native
  registration for each used handle. The old GtkImageMenuItem entry incorrectly
  names GTK_TYPE_MENU_ITEM; it is not copied into a new type catalog. Removed
  GTK 2 types and individual feature mappings remain in their own work items.
- `legacy-fbc69246be9f2c10c3d4`, glib-cxx-types.hpp: TraitsBase provides native
  get_type, checked cast and is_instance; macros generate native class/interface
  registrations. The shared instance predicate becomes the explicit rejecting
  factory check, without reinstating old bridge macros or C++ RTTI.
- `legacy-f6b8f51e4c3dd7e45581`, glib-cxx-utils.hpp: IObject conversions call
  Traits<G>::cast, while owning constructors themselves do not check the
  expected GType. ObjectRef now validates before touching ownership. A GLib
  cast diagnostic is not treated as proof that an old factory rejected safely.

The archive contains all three exact old files, their Git blob and distinct
whole-file/addition-hunk hashes, current compiled sources, commands and results.
Only these rows' mutable execution fields become DONE. Their source identity,
action and acceptance remain unchanged. This does not declare the complete old
GTK/GIMP catalog or all feature callers migrated.
