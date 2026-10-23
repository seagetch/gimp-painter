# Original WBS 07.008: emitter-first Connection destruction

The original acceptance is that a Connection destructor must not touch an
already finalized emitter. The common wrapper stores a GWeakRef-backed emitter
and a handler ID. Destruction calls close, detaches its own state, then locks the
weak reference before any native handler query or disconnect. A failed weak lock
skips both operations. This path has no subclass-specific branch; it applies to
GObject emitters, including native GTK widgets after actual finalization.

The previous ownership test finalized the emitter but explicitly closed the
handle before scope destruction. The new orders 4 and 5 in
`/painter/signal/owned-lifetime` close that evidence gap. Order 4 releases the
emitter, requires both its weak finalization counter and closure destruction
counter to be one, then leaves the expired handle untouched until its destructor.
Order 5 blocks twice before emitter finalization and unblocks twice afterward,
exercising the nonzero-depth weak-lock path before destructor cleanup. Both
require exactly one closure destruction after scope exit. The existing four
orders still cover explicit repeated close, wrapper-first destruction and
external native disconnect. The emitter refcount is one while connected, so
there is no hidden strong-reference cycle masking finalization.

The complete mixed C11/C++14 component passes 67 native, 67 ASan/UBSan and 67
native Meson cases. Current source hashes and real TAP results are recorded in
native.json, sanitizers.json and meson.json. System GLib is uninstrumented;
LSan, whole-process leakage and cross-platform acceptance are not claimed.
There is no production-code change or reproduced production defect in this task.

## Source and caller applicability

The prior routing assigned this exact Connection-destructor check to 94 hunks.
The bounded source review retains four: the delegators implementation
(01.002/000042), editor dropdown decoration (001604), popup-renderer click
(001544), and layer-tree popup-renderer creation (001651). The three caller paths
reach the old PopoverDecorator, which owns Delegators::Connection objects.
The other 90 hunks contain native GTK layout/model/declarations, inactive
branches, native C signal connections, or reference-owning Definer/Packer code
without creating, owning or destroying this Connection. The routing proof
removes only their 07.008 verification duty. Independent feature and integration
obligations remain. Routing itself adds zero DONE rows.

The modern ownership component is app/painter/connection.hpp. Its destructor and
weak-lock behavior are exercised directly with actual emitter finalization,
including the untouched expired handler ID. Current layer Tiles owns
popup_connections and clears them before releasing the popup; PainterLayerDialog
owns a connection vector and clears it on close. Both create the same common
Connection, rather than a separate popup-specific disconnect implementation.
The unchanged GTK ownership native report additionally covers popup repetition,
owner closure, retained/detached controls and callback reentry; its 14 source
seals still match. Those measured GTK results are reused, not freshly rerun.
The older GTK sanitizer report has filter-scheduler.hpp drift and is not claimed
as current sanitizer evidence.

The four retained duties are accepted for this shared destructor boundary.
This does not prove exact legacy dropdown, preset or cell-renderer UI equivalence.
In particular the exact old gimp_editor_add_dropdown UI route remains unresolved;
related modern popover/dialog code establishes ownership reuse only. Full
feature preservation remains in its existing feature/29.020 work items. The
original component condition is neither enlarged to require all future UI work
nor used to mark that work complete.

acceptance.json records the current reports, source mapping and state counts.
The routing reproducer checks pinned blobs and exact hunks, the 90 TODO-only
removals, four retained identities and unchanged unrelated work. Historical
acceptance digests are preserved; the whole old matrix is not described as
current PASS.
