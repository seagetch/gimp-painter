# RTTI separation (original WBS 04.011)

This contract records the legacy RTTI boundary and the new API's independence from
C++ RTTI. It does not require disabling RTTI in every upstream C++ target, porting
every feature, or completing the separate legacy-adapter removal task.

## Frozen source evidence

The source is `afa43fae3e920210146abed514f136fd49f671b5`, fixed by
`migration/baseline/baseline.json`. The [frozen-source audit](../inventory/rtti-usage-review.json) fetched all 251 C++/header paths
(`.cpp/.hpp/.cc/.cxx/.hxx/.h`) in `migration/inventory/changed-files.tsv` and
matched each returned Git blob to that inventory's `source_git_blob`. The 180
ordinary `.h` files add no RTTI operations. This is an inventory-scoped scan,
not a claim about all unchanged legacy repository files.

| Frozen source | dynamic_cast lines | typeid lines |
|---|---|---|
| app/base/glib-cxx-impl.hpp | none | 296 |
| app/base/glib-cxx-utils.hpp | none | 1126, 1141, 1152, 1169 |
| app/core/gimpclonelayer.cpp | 372, 656 | none |
| app/core/gimpfilterlayer.cpp | 987, 1024, 1032, 1162 | none |
| app/core/gimpperspectiveguide.cpp | 230 | none |
| app/display/gimpcanvasperspectiveguide.cpp | 285 | none |
| app/presets/gimpjsonresource.cpp | 206 | none |
| app/presets/preset-factory.cpp | none | 96, 169 |
| app/presets/preset-factory-gui.cpp | 80, 111, 133, 186, 235, 242, 249 | none |
| app/widgets/gimpcellrendererpopup.cpp | 146, 152, 334 | none |
| app/widgets/gimplayertileview.cpp | 1587 | none |
| app/widgets/gimptooltileview.cpp | 593 | none |
| app/widgets/popupper.cpp | 401, 411 | none |

There are 23 dynamic_cast expressions and seven typeid expressions. Nine casts
return an Interface from the concrete private Impl, whose declaration uses
public virtual inheritance. These are upcasts; the spelling `dynamic_cast`
alone does not establish a requirement for runtime RTTI. Seven reverse
Interface-to-Impl casts (Clone 372; Filter 987/1024/1032; CellRendererPopup
146/152; Popover 401) and seven preset factory/configuration downcasts depend on
runtime type information. Replacing a cast with reinterpret_cast would discard
the required pointer adjustment and is not a migration strategy.

The typeid uses have different roles: property-installation diagnostic naming;
four decorator-name lookups; a configuration-extension key from `typeid(*this)`;
and the preset registry key `typeid(config)`. The last expression describes the
static pointer type; it must not be documented as the pointed-to dynamic type.
These process-local identities are not serialized or public ABI identifiers.

The old Clone and Filter Interface::cast functions explicitly check is_instance
before private lookup. The other seven Interface::cast definitions do not.
Several reverse/preset cast callers immediately dereference the result. Those
observations describe the old code; they do not establish that its lifetime,
failure, or C-boundary behavior meets the new contract.

## Conditions for a transitional RTTI adapter

A temporary adapter, if one must be retained during a migration, may perform
RTTI operations only inside its private implementation boundary:

1. Its input must be a live GObject of the expected GType, held for the whole
   synchronous access, and associated with the correctly constructed, live C++
   implementation. A GType check alone cannot validate an arbitrary dangling
   address or prove that an Impl's constructor ran.
2. A downcast must start from the actual polymorphic C++ base subobject. The
   hierarchy must be complete at the cast and built/linked with compatible RTTI
   metadata, compiler ABI and inheritance declarations. Never cast a GObject
   address as if it were an Impl address.
3. Check pointer-cast failure before use; translate failure into the entry's
   declared error or safe return. Reference-cast/typeid failures, if used, must
   be contained by the normal C exception boundary. No exception or C++ layout
   assumption crosses a C entry, callback or GInterface/vfunc ABI.
4. Keep the adapter and its RTTI flags private to the affected implementation
   target. Do not publish a second Interface::cast API, type_info key, RTTI
   object address or C++ inheritance layout as the new handle/FFI contract.
5. Remove the temporary adapter when its callers and state ownership have
   migrated. These are requirements for an adapter introduced or retained now,
   not assertions that the frozen source already meets them.

The current migrated app does not compile an old RTTI adapter: its production
source/private-header scan finds no dynamic_cast, typeid, dynamic_pointer_cast,
type_info use, legacy Interface::cast, NewGClass or glib-cxx include. Several
current feature files share old path names, but contain new implementations;
path identity is not evidence that the old source remains compiled. Frozen
legacy capture helpers and source records under tools/migration remain evidence
or oracle infrastructure, not production app translation units.

## New handle and store identity

`ObjectRef<T>::check` in `app/painter/object-ref.hpp` validates against
`TypeTraits<T>::type()` with `G_TYPE_CHECK_INSTANCE_TYPE`. It uses the GObject
type system; C++ inheritance does not define the GObject instance layout.

`SlotSpec<Owner, Impl>` fixes the owner type and implementation together.
`BindingStore::emplace<Slot>` checks the owner GType, construction state and
duplicate slot identity. Each Slot specialization owns a function-local token;
the address of that token identifies the exact slot type. Even slots that use
the same Impl type have different identities. Private `entry(identity<Slot>())`
returns only the entry recorded under that identity, after which the internal
`static_cast<Entry<Slot> *>` is justified by the creation/lookup invariant.
Clients cannot provide an independent string/type key or substitute a raw Impl.

EntryBase remains polymorphic for close/destruction. Virtual dispatch and
virtual destruction are supported without C++ RTTI. The token is an internal
identity in the linked application; it is not a serialized value, plugin key,
or promised cross-DSO ABI. Existing owner leases, lifecycle checks and borrowed
result restrictions still apply independently of RTTI.

Current references: `app/painter/binding-store.hpp:14–21,37–64,121–151`,
`app/painter/binding-store.cpp:66–70`,
`app/painter/object-ref.hpp:11–13,45–51`,
`app/core/gimpclonelayer.cpp:46`,
`app/core/gimpfilterlayer.cpp:69`.
The same-Impl/different-slot foundation case is
`app/painter/tests/test-foundation.cpp:129–140`.

## Compiler proof and separate sanitizer policy

Normal Painter targets already receive `painter_cpp_args`, containing the
exception policy and supported no-RTTI switches. Core/display receive the
exception policy separately. Their default RTTI setting does not establish a
source-level API dependency. Original 04.011 can be closed with recorded
source/contract evidence plus isolated compilation with a trailing no-RTTI flag;
production compiler policy need not change. Preserve C compilation, upstream
target flags and the exception behavior accepted by original 04.010.

The [native verification](../tests/rtti-policy/README.md) inventories 73 default
and 78 HTTP production C++ commands. It recompiles every core/display C++
translation unit, twenty per configuration (sixteen Painter and four upstream),
into private objects with a final `-fno-rtti`; all forty compile successfully.
Original objects, production sources and compile metadata remain unchanged.
Other Painter production targets already select no-RTTI flags and are
inventoried separately, without a new full rebuild claim.

Both reconstructed foundation executables pass all 39 cases with no-RTTI
common C++ commands. These are fresh runs, not reused results from vanished
build products. Five focused control categories cover polymorphic downcast,
polymorphic typeid, ordinary virtual dispatch, virtual-base upcast, and the real
typed GType/store/C-close bridge. The first two are rejected with RTTI disabled
and run when explicitly enabled; virtual dispatch and the upcast work in both
modes. The bridge works with RTTI disabled.

The first typed-bridge fixture incorrectly treated GLib's GInitiallyUnowned
alias as a distinct C++ type. Initial FAIL reports remain archived. The corrected
fixture uses a distinct opaque test owner tag, rerunning only that fixture's
compile/link/runtime. Final reports record the correction, source hashes and
identity checks for retained object-compilation and foundation results. No
production defect or product-policy change is attributed to that fixture fix.

Focused UBSan vptr harnesses are a separate test policy. They deliberately
recompile instrumented C++ with RTTI and obtain the production no-RTTI
compatibility closure from the actual compile database using
`migration/tests/painter_sanitizer_scope.py`. Extra compatibility units receive
RTTI only and must be listed separately from sanitizer-instrumented units.
Shared BindingStore/Surface/shared-pointer COMDAT metadata must be consistent;
mixing their RTTI and no-RTTI definitions can produce a harness diagnostic.
Do not disable vptr checking or call compatibility-only units instrumented.
This does not introduce a dynamic_cast-based product API requirement.

The existing `04.011/sanitizer-rtti-scope` row and its three metadata tests cover
discovery of production sources, supported C++ suffixes, and compile database
command/arguments forms. They are neither new runtime acceptance nor proof of
all-feature sanitizer coverage. Source-frozen historic diagnostics/results
retain their original scope.

## Acceptance scope

Original 04.011 is accepted by the source-bound legacy conditions, current
1,907-path source/header scan, store identity invariant and native no-RTTI
evidence recorded here. No canonical source-duty rows belong to this original task.
The result does not prove another platform, a fresh full application relink,
new sanitizer runtime coverage, or all-feature legacy semantic equivalence.

