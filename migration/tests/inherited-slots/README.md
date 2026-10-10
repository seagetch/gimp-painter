# Original WBS 06.025: inherited slot resolution

The original condition is that multiple hierarchy implementations on one native
object cannot be confused. The existing common store already keys an entry by
its declared Slot type, not the payload Impl type, owner GType, most-derived
runtime type or a string. This task adds the missing negative and independent
mutation assertions and closes the two source duties against fresh evidence.
No production slot-selection defect or behavioral change is claimed.

## Native hierarchy and negative cases

The C fixture defines real Base/Child GTypes and a Readable interface. Their C++
adapter registers BaseSlot and ChildSlot in one store during native instance
initialization. Both slots deliberately contain the same Impl C++ type.

- Existing properties/interface and type-ancestry cases prove native inheritance,
  interface access, construction properties and the two independently owned Impls
- New `slot-identity` compares the store through child, base and interface views;
  nested scoped borrows prove distinct Impl addresses and tags. Changing base
  alone preserves child, and changing child alone preserves base. A true C++
  alias selects the same slot; a newly declared lookalike with identical Owner
  and Impl types is missing. Closing preserves distinct const values and releases
  both implementations once
- New `base-missing-derived` requests ChildSlot read and write three times on an
  actual base-only object. Each request returns MISSING_SLOT before its callback,
  with no new Impl, unchanged BaseSlot value, active state and generation
- Existing multiple-slots and slot-registration/lookup cases separately verify
  same Owner+Impl with different Slot identities, duplicate and incompatible-owner
  registration, and missing lookup without lazy construction

`native.json` and `sanitizers.json` each record all 61 cases passing, exact source
hashes, C11/C++14 build/link commands, and six compile-time borrow escape rejections.
ASan and UBSan cover the common component and actual GObject fixture. LSan is off;
this is not a whole-GIMP sanitizer, platform or every-feature lifetime result.
The additional Meson build/run is recorded in `meson.json`.

## Exact source duties

`source-mapping.json` identifies the original hunk keys and unchanged ledger
hashes. The archived files under `legacy-source/` independently reproduce the
following Git blob IDs:

1. `app/base/glib-cxx-bridge.hpp`, `01.002/000043`, blob
   `5a56ab19e30499a86fa93d81419f907812652c4d`: declarations describe native classes
   and interfaces. Their role in this task maps to TypeTraits and the typed slot's
   expected native owner, exercised through base/child/interface views
2. `app/base/glib-cxx-types.hpp`, `01.002/000046`, blob
   `19a9d47773fa06a1c654283a6d2e033a67b2428a`: TraitsBase and declaration macros bind
   native Instance/Class/GType and ancestry predicates. The new TypeTraits,
   ObjectRef native type check and SlotSpec retain that type relationship while
   exact Slot identity selects the implementation

These files do not define old private-storage lookup; that is in separately
assigned glib-cxx-impl.hpp. Recreating every old GTK catalog entry is not the
06.025 condition. Property-ID delegation and hierarchy teardown remain the next
original tasks, 06.026 and 06.027. Only the two exact 06.025 ledger rows change;
their immutable contracts and all other rows are preserved.

## Reproduction and evidence limits

```sh
python3 tools/test_painter_foundation.py --build-dir /tmp/painter-hierarchy --report /tmp/painter-hierarchy.json
python3 tools/test_painter_foundation.py --sanitize --build-dir /tmp/painter-hierarchy-asan --report /tmp/painter-hierarchy-asan.json
```

The captured Linux environment is GCC 14.2.0 and GLib/GObject 2.84.4. The Meson
target is `app/painter/painter-foundation`. Existing historical reports retain
their original seals; their later source mismatches are not rewritten into new
PASS evidence. `validation.json` distinguishes the current 06.025 acceptance from
those unrelated historical snapshots.
