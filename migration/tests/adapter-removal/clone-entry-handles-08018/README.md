# Clone-layer original operations 9–14: native named routes

Public base 8685d5b5a68aafd67aa49c120a02e3c8e445b73b; tree
a99d5d78f48f939f5935340ffceb642ad2924881. Legacy source pin:
afa43fae3e920210146abed514f136fd49f671b5.

## Explicit method migration

CloneLayerRef::set_source and set_source_name now each directly call their
original void C entry once. These named methods use the original C boundary's
contained failure policy. Callers needing the previous exception-reporting
behavior must use the new set_source_checked or set_source_name_checked methods;
those preserve the former _full/GError-to-Error bodies. The provider and all
production callers are unchanged. Existing named in-tree callers are tests;
ordinary editing continues to use explicit Undo-aware C operations.

Other original routes already existed: create calls the C factory and sinks its
floating GimpItem reference; source calls the borrowed C getter and immediately
retains its result; both shared TypeTraits call the original native GType
registration. No second provider/store/GType or serialization model was added.

## Source and persistence semantics

The source getter is a lazy, potentially mutating operation. It resolves the
first exact name in recursive pre-order, retaining identity through rename.
Setting a name preserves an existing source; missing text remains pending.
NULL clears a pending request, while empty text is a real pending name. Direct
source assignment keeps pending text, and its update may resolve that text.
The current generation-safe implementation rejects stale outer callbacks after
such resolution. Legacy on_source_update could overwrite the newly resolved
source with the outer stale callback's source. This safety correction is
preserved, so row 12 remains PARTIAL for literal historical semantics.

Weak source/image bindings, disposal/close handling, freeze balancing, cycle
protection, cached pixels on detach and native precision behavior are unchanged.
Owning a returned source reference is independent of clone/container ownership.

Native serialization still uses nonmutating dup_reference/restore_reference
snapshots, stable IDs, pending/live/expired metadata and guarded name lookup.
It never substitutes the lazy handle source getter. Missing/external IDs cannot
silently rebind by name. The original legacy-file import route remains separate.
No serializer, provider, original public C declaration or production caller was
changed by this batch.

## Compatibility limits

Row 11 remains PARTIAL: the modern GimpLayerMode factory parameter differs from
legacy GimpLayerModeEffects, and modern setters emit notifications/validate mode
where legacy assigned fields. Native factory geometry and ownership evidence do
not imply unchanged enum/source signature or construction signal order.

Rows 9/10/13 record bounded native operation routing. Row 14 records only native
get_type delegation: current CLONE_LAYER_SOURCE Undo differs from historical
GROUP_LAYER_CONVERT. Historical abstract Interface dispatch, instance/class
layout, parent types, subclass ABI and complete old Undo behavior are not
restored. Broader 08.018, the type/handle catalog, separate 31 hooks and 141/661
remain open/unchanged. Only rows 9–14 implementation evidence changes; other 70
entries, row 50's borrowed identity, ImageGenerator prerequisites and blocked
perspective are untouched.

## Baseline and validation scope

Supported GNU/Linux test-only linker wrappers count original setter entry calls
and forward to the real providers. Before the fix both named methods report
expected=1, actual=0 and abort at that assertion. Both isolated red controls have
process -6, Meson 1 and clean authenticated X server exit 0. Production code has
no added test instrumentation. Unsupported-toolchain skips are not accepted as
passing evidence for this environment.

An initial Meson preparation used a rejected nested ternary; it was corrected to
independent argument-list expressions before the successful baseline build. One
initial case-selection attempt used the old executable and registered zero
matching cases; that exit-0 attempt is retained but explicitly not acceptance.
Final verification requires exact source-derived TAP IDs and case counts.

The final clone suite preserves all 39 existing C cases plus focused original-C
and named-handle additions. Five predeclared existing XCF cases are separately
selected in each mode: clones_edit_save_reopen, missing_clone_id_and_pending_name,
external_references_remain_unresolved, malformed_clone_reference_stays_opaque,
and clone_duplicate_name_identity. Those are a focused XCF subset, not a full
XCF-suite pass. Unrelated multipart/disk-exhaustion tests are excluded.

This header-only routing change adds no UI and does not alter ordinary
application provider behavior. No artificial screenshot or application relink
is claimed; updated native and ASan test executables exercise the named route.

## Final acceptance

52 native and 52 ASan+UBSan full clone cases pass: 39 existing, 2 added actual-C11
and 11 C++14 cases. All 5 predeclared XCF selections pass in each mode with exact
single-case TAP IDs; all selected processes, Meson commands and authenticated
X servers exit 0. The malformed fixture's 90 exact retention diagnostics per mode
are source-derived and independently reviewed, not hidden or generalized.

375 C11/C++14 header probes and both header gates, 4 positive / 3 negative handle
compile controls, 48 policy tests, 90-contract/76-route checks, 12 standard plus 6
XCF-specific corrupted-report rejection controls pass. Final independent review
found no blocker. Original setter route counters change from 0 to exactly 1.

Reproduce the full clone suite with the established native/ASan builds:

    meson compile -C BUILD -j2 gimp-clone-layer painter-xcf-roundtrip
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-clone-layer

For each exact XCF ID listed above, run the existing painter-xcf-roundtrip test
with --test-args '-p EXACT_ID'. See report.json for exact commands, source,
binary/evidence hashes, expected diagnostics, failures and compatibility limits.
