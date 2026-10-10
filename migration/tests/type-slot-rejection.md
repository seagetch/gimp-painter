# Original WBS 07.003: wrong types and missing slots

Current tests satisfy the original explicit-error and invalid-implementation
access condition. Their results in the unchanged 65-case native, ASan/UBSan and
Meson [common suite](construction-failure/README.md) are reused after checking
all recorded source hashes and the specific PASS lines. No new execution or
production change is claimed.

- `type-rejection` rejects incompatible live GObject and GInitiallyUnowned
  instances in retain/adopt/sink before changing ownership. It checks WRONG_TYPE,
  native GError conversion, unchanged counts and floating state, and finalization
  only when the caller releases its reference
- `type-ancestry` accepts exact, base, implemented-interface and GObject views,
  while rejecting a base instance requested as a child or unimplemented interface
- `slot-registration` rejects an incompatible native owner before constructing an
  Impl. The rejected candidate's counters stay zero and the owner's reference
  and store generation remain unchanged. Subsequent duplicate/state rejections
  preserve the explicitly installed slot's value
- `slot-lookup` rejects absent identities in construction, active and closed-state
  read paths without invoking their callbacks or creating a default Impl. The
  one explicitly registered slot stays unchanged and is destroyed once
- `base-missing-derived` repeatedly rejects a child slot on a base-only owner;
  it neither falls back to the base slot nor allocates a child implementation

The native pointer precondition is a live GObject of some type, or NULL where
the API allows it. The type predicate necessarily reads valid GType metadata.
The condition concerns rejecting an incompatible implementation and absent
storage before access; it does not promise to validate arbitrary corrupt or
already-freed addresses. ASan/UBSan and callback/construction counters provide
complementary evidence within that precondition.

`type-slot-rejection.json` maps the three exact old bridge, traits and utility
blobs. The old catalog selects native GTypes; TraitsBase supplies an instance
predicate and checked casts; IObject conversions use those casts. The current
factories reject before ownership and BindingStore rejects before resolving an
absent Impl. This does not reinstate the old macro bridge or certify every
legacy catalog entry. Other feature callers, wrapper kinds and platforms retain
their separate duties. LSan and instrumentation of system GLib are not claimed.
