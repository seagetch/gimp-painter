# Original WBS 07.015: typed hierarchy lookup

The original condition is retrieval of base, derived and multiple slots as the
correct types. The implementation and measured tests already satisfy it. This
acceptance closes exactly the three assigned verification duties after checking
the original sources and current test seals; it adds no new feature or adapter.

The real C fixture registers Base and Child GTypes, with Readable implemented by
Child. Native instance initialization installs BaseSlot and ChildSlot into one
BindingStore. The two slots deliberately use the same C++ Impl type: selection
must follow the declared Slot identity, even when owner and payload types alone
cannot distinguish entries. Typed ObjectRef access checks the native GType before
acquiring ownership; scoped store access selects the registered Impl without
performing an unchecked most-derived cast or creating missing state.

The current cases prove:

- Child is accepted through child, base, GObject and implemented-interface views;
  base-only owners reject child and interface views without changing ownership
- All views find the same native owner's store; BaseSlot and ChildSlot have
  distinct addresses, tags and values, with independent mutation
- A true slot alias resolves correctly; an otherwise identical newly declared
  slot is absent. Missing-derived read/write on a base-only owner rejects before
  the callback and creates no state
- Multiple slots, duplicate/incompatible-owner registration and missing lookup
  have explicit positive and negative assertions. Const reads after close retain
  the separate values and both implementations are eventually destroyed once

`source-mapping.json` verifies the three existing source copies against their
assigned Git blob IDs. glib-cxx-bridge.hpp declares native class/interface types;
glib-cxx-types.hpp binds Instance/Class/GType and ancestry. Their replacement is
TypeTraits, checked ObjectRef and SlotSpec. glib-cxx-impl.hpp supplies native parent
registration and get_private/Binder lookup; its class-specific private storage is
replaced by exact typed slots in the single BindingStore. This task does not
require recreating the obsolete GTK2 type catalog or accepting every feature
adapter. No source duty is moved or removed.

The unchanged boundary-conversion native, ASan/UBSan and Meson reports each contain
69 passing cases and all 25 current source hashes. Eight directly relevant cases
are listed and checked per report. Their actual execution is also corroborated
by the four native OS/architecture reports accepted for 07.014. Those prior test
results are reused only because their bytes still match current source; no new
execution is claimed here and historical reports are not edited. LSan remains
disabled in the referenced sanitizer run. The foundation acceptance checker
validates this row against the current native/sanitizer/Meson reports; unrelated
historical row mismatches remain separately recorded.
