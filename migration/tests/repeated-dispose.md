# Original WBS 07.006: repeated native dispose preserves state

The current 66-case [common reports](construction-property/README.md) already
exercise repeated real `g_object_run_dispose`, fixed native parent chaining,
and reentry. Every recorded source seal still matches. This acceptance reuses
those measurements; it does not rerun unchanged tests or introduce a production
change.

- `property-dispose-finalize` sets value 25, disposes twice, then reads the same
  value through the native property getter. Close occurs once, destruction is
  deferred to final unref, and the exact lifecycle trace rejects extra phases.
- `parent-dispose-reentry` enters native dispose recursively from the parent and
  disposes again. The generation advances once, each slot closes once, and all
  parent calls see a closed store with readable state. Final unref still chains
  native dispose and destroys each Impl once in the measured finalize phases.
- `shutdown-paths` covers base and derived owners, both with and without a prior
  explicit close. Repeated native dispose preserves generation, per-slot close
  counts, readable parent state and deferred destruction.
- `close-transitions` checks repeated and nested close during construction and
  active lifetime. Generation remains stable after the first transition, new
  callback admission stays closed, saved value 41 remains readable, the native
  reference count is restored, and each slot survives until final release.
- `construction-property-callbacks` additionally confirms repeated native
  dispose leaves the owned signal disconnected: a later emission cannot enter
  the observer, and final unref destroys once.

Source duty `legacy-0910998e1e1b6516b051` refers to glib-cxx-impl.hpp hunk
`01.002/000045`, exact blob `a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`,
already preserved in the inherited-property evidence. Its lines 413–416 destroy
Impl at native finalization and chain the saved parent; line 443 installs that
finalize callback. This old wrapper does not install a dispose callback. The
migration preserves the distinction between repeatable logical close and final
Impl destruction through the common store and native C vfuncs; it does not claim
that the old wrapper already provided the modern closed-state contract.

`repeated-dispose.json` seals this source mapping and the reused reports. The
original common-boundary condition is met. Feature-specific disposal adapters,
whole-application and platform acceptance remain their own tasks. ASan/UBSan
and explicit ownership counters do not constitute LSan or system-GLib coverage.
