# Native GInterface registration — original 06.016

Class registration and interface implementation use separate native mechanisms:
G_DEFINE_TYPE/G_DEFINE_TYPE_WITH_CODE for the object and G_IMPLEMENT_INTERFACE
for its typed interface initializer. The shared BindingStore contains C++ state;
it is not an alternate GType or interface registry. Production already follows
this contract, so this task strengthens the existing native C fixture rather
than introducing another registration adapter.

The hierarchy fixture verifies that PainterReadable is an interface and is not
instantiatable, has the GObject prerequisite, and is installed only on the child.
The base remains a native object class with no implemented interface. The child
retains its correct parent, inherited constructed vfunc and distinct property
getter. Its implementation interface has the exact interface/implementing-type
identity and differs from the default interface vtable. The default read slot
stays NULL before and after child initialization and destruction; the child read
slot is the exact C-linkage C++ trampoline. Native interface lookup retrieves
that same vtable, with no parent implementation in this fixture.

The existing real C virtual dispatcher calls through that slot into the shared
store. It returns both base and child state, contains a deliberate C++ exception
as GError, and recovers for a subsequent valid call. Existing ancestry tests
accept implementing objects and reject the nonimplementing base without taking
ownership. The rebuilt normal and common-component ASan/UBSan suites each pass
53 cases. The registration checks extend an existing case rather than duplicating
it. All 11 component C/C++ units are instrumented; system libraries are not.
LSan remains unavailable under the verified ptrace limitation, and vptr is
excluded for native no-RTTI compilation.

A source inventory records eight current native interface registrations:
CloneLayer/Pickable, FilterLayer/Pickable and Progress, ProcedureProgress/Progress,
Mybrush/Tagged, GuideUndo/Initable, MybrushOptions/Config and MybrushEditor/Docked.
Their interface initializers are separate from their class initializers. This is
registration source evidence, not a new runtime pass for every feature method.

The sole source obligation legacy-20e8ee3abfc2eddf2e2e covers pinned
app/base/glib-cxx-impl.hpp hunk 01.002/000045. Its add_interface_iter constructs
GInterfaceInfo and adds interfaces after class registration. Modern native macros
retain this distinction and typed callback assignments. Exact old blob and hunk
digests are verified; only this obligation's mutable execution fields become
DONE. Feature-specific interface behavior, derived interface override chains and
remaining platform acceptance retain their separate gates.
