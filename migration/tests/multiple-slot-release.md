# Original WBS 07.004: each implementation is released once

The current common tests already observe separate close and destruction counts
for each registered implementation. Their 65-case native, ASan/UBSan and Meson
[reports](construction-failure/README.md) have unchanged source seals and passing
lines for the relevant tests. This acceptance reuses those measurements.

`implementation-ownership` has independent counters for two slots and checks
teardown during construction, active-state fallback and repeated explicit close.
Releasing a non-final native reference destroys neither Impl. The last owner
reference closes and destroys each exactly once. Thus a total destruction count
of two cannot conceal one missed slot and one repeated destruction.

`shutdown-paths` uses actual C base/child GTypes, with one or two slots and with
or without explicit close before repeated native dispose. Per-slot tag counters
remain one at finalization. `parent-dispose-reentry` adds nested parent disposal
and another explicit disposal, checking both per-slot destruction counters and
native finalize phases. Repeated parent chaining does not imply repeated Impl
destruction. `multiple-slots` additionally checks independent stored values.

Duty `legacy-1917976c127e8b287262` identifies old glib-cxx-impl.hpp hunk
`01.002/000045`, exact blob `a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`.
The old native-private placement construction and explicit instance-finalize
destructor map to uniquely owned modern Entry/Impl objects and one native qdata
store teardown. The exact source is already preserved in the inherited-property
evidence. This acceptance does not require a new order between Impl destructors;
logical close order and native parent chaining remain the documented contracts.

`multiple-slot-release.json` records the exact duty and reused report seals.
There is no new production change or redundant runtime execution. Explicit
destructor counters and ASan/UBSan do not constitute LSan or cross-platform
results, and feature-specific owner adapters retain their separate obligations.
