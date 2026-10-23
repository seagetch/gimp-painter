# Original WBS 07.013: shared bridge ASan/UBSan acceptance

The original section 07 scope is independent bridge verification. The existing
[module contract](../../contracts/module-layout.md) confines the common bridge
to GLib/GObject ownership, BindingStore, exception boundaries, Connection and
Source. This acceptance does not encompass GIMP/GEGL/PDB/HTTP feature algorithms.

The current boundary-conversion native, ASan/UBSan and Meson reports each contain
69 passing cases. All 25 source/runner seals match this checkout. The sanitizer
report records address and undefined instrumentation on eleven C/C++ translation
units, with no unresolved diagnostic in the passing run. Existing exact-current
measurements are reused; no duplicate suite execution or production change is
needed. The six deliberately rejected compile probes are expected type-safety
checks, not runtime sanitizer failures. LSan is disabled; system libraries are
not instrumented, and no cross-platform or all-feature pass is claimed.

## Four retained source duties

- delegators.hpp: closure/connection and qdata owners map to Connection, explicit
  C callbacks and BindingStore. Signal ownership, block/after order, reentrant
  disconnect/close and store lifetime cases exercise the migrated common path.
- glib-cxx-utils.hpp: owning Object/IObject, GValue owner/borrow/copy, CString,
  GArray, synchronized and EventSource map to ObjectRef/WeakRef, Value/ValueView,
  String, ArrayRef, MutexGuard and Source. Resource, reference, source and
  reentrant assignment cases exercise those replacements. Old BoundMethod is
  raw, not weak; this does not accept all decorators, list/hash/regex callers.
- scopeguard.hpp: matching native deleters and CXXPointer map to typed RAII and
  unique_ptr-owned Entry/Impl. Implementation ownership and acquired-construction
  failures verify release and unwind.
- selectcase-utils.hpp: duplicated matcher strings and referenced pattern arrays
  map to String/ArrayRef and construction unwind, including acquired string and
  two-element pattern array. Cleanup is accepted; matching semantics are not.

Independent caller migration, 07.008 child requirements, 07.016 and 08.019 remain
open where already open. Full fixed helper bytes are reused from the committed
GTK caller census. Exact hunk identities and bounded test mappings are recorded
in routing-cases.json; validation.json pins the unchanged measured reports.

## Fifteen corrected assignments

The original generic core-hooks profile incorrectly assigned common bridge
sanitizer acceptance to fifteen unrelated hunks: curve property range, whitespace,
TileManager null check, GEGL multiline metadata, GimpArray length API, image
invalidation origin, dependency minima, process termination backtrace and native
tool input fields/dispatch. Each fixed source/base blob and zero-context payload
is verified by reproduce-routing.py. Their source-specific reasons remain in
routing-cases.json. The GimpArray length getter is used for HTTP/PDB result
serialization; it is not ArrayRef's GArray. Its modern typed getter/element-count
replacement does not receive an equivalence pass from common bridge tests.

Only these fifteen TODO 07.013 duties are removed. Their forty-five existing
08.018/core-extension-hooks, 36.011 and 36.012 duties remain byte-for-byte
unchanged. All other duties and prior DONE evidence are preserved. Routing adds
zero DONE entries; the four retained helper duties separately gain measured
acceptance. No source hunk, feature obligation or original WBS item is deleted.

Run `python3 -B migration/tests/bridge-sanitizer/reproduce-routing.py --check`
and `python3 -B tools/audit_legacy_granularity.py --check` for the bounded routing
proof. Historical full-tree generators are not reported as passing: only the
nineteen reviewed hunks from fifteen paths were reconstructed. Historical report
hashes stay fixed; unrelated stale acceptance rows remain identified separately.
