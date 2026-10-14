# FilterLayer query/lifecycle routes, original rows 21–25

Public base fdd2e15f0fb74ce8c6af32de960cfae8410e674e; tree
13dbf918381926555e4271174938a08f094694c7. Legacy source:
afa43fae3e920210146abed514f136fd49f671b5.

## Query ownership and caller responsibilities

The restored original const gchar *get_procedure entry borrows the existing
native saved-name storage. It is valid until definition replacement or owner
destruction, including existing read-only metadata access after explicit close.
The named handle immediately copies it into owned String. Existing modern
procedure()/dup_procedure APIs remain unchanged. This native saved exact name,
including unsupported definitions, is not the old resolved PDB original_name.
No shadow storage or procedure resolver was added.

The restored GArray *get_procedure_args entry materializes through the existing
immutable, reference-aware native argument model, then initializes and copies
GValues into an owned GArray with cleanup. Callers unref the array; the named
handle adopts it using existing ArrayRef. Absent arguments return NULL, while
an explicitly empty native argument array yields an owned empty GArray. Native
materialization failure returns NULL without a partial result. Temporary native
arrays and initialized output values are RAII-cleaned.

IMPORTANT: Independent GArray/GValue storage is not universal object ownership.
Scalar GObject values retain the same object identity. Boxed values preserve
their own native copy contract. In particular GIMP_TYPE_CORE_OBJECT_ARRAY copies
only its pointer container, not element references. Callers must keep those
elements alive separately and must not dereference them after owner loss. There
is no hidden element-ref redesign or GObject cloning. Expired scalar references
materialize as NULL; an expired object-array member rejects the entire existing
native materialization. The saved weak-reference metadata itself is unchanged.

Legacy ProcedureRunner copied its GValueArray first, then FilterLayer transferred
those GValues into a GArray but leaked the intermediate container. This was not
direct aliasing of runner-owned payloads. The new explicit copy-and-cleanup route
preserves independent value ownership without the leak, but does not recreate
runner-populated defaults, context-ID substitutions or conversion rules.

## Loaded-cache and factory routes

The named mark_as_loaded method calls the existing original C entry once. Native
behavior advances generation, cancels/discards stale work, certifies current
cache pixels, clears dirty/error and releases preparation. Legacy only set a
flag consumed on first projection. The existing scheduler implementation and
loader order (definitions/pixels/topology, then mark loaded) remain unchanged.

Existing create sinks the original C factory's floating GimpItem before image
insertion. Shared TypeTraits uses the original sole native GType registration.
Native GimpLayerMode differs from historical GimpLayerModeEffects; factory
fallback/validation/notifications do not prove old construction equivalence.

## Scope and preservation

Rows 21/22/24/25 remain PARTIAL for these precise native-model differences.
Row 23 records callable native registration only. No historical abstract
Interface, parent/class layout, subclass ABI, enum/source equivalence, arbitrary
PDB dispatch or old async-runner equivalence is claimed.

Row 26 set_procedure stays untouched: a GArray-to-set_definition alias would
change lookup, unknown-name behavior, registered defaults, transformations,
context arguments and failure/notification semantics. Modern definition/edit/
Undo APIs and production callers remain unchanged.

Native and legacy serializers continue using their existing raw/opaque bytes,
immutable snapshots, reference metadata and cache state. They do not use the
new materialized getters. The scheduler, immutable argument model, serializers,
loader ordering and existing regression sources are hash-protected. Only rows
21–25 implementation evidence changes; other 71 entries, separate 31 hooks,
type/handle catalog, broader 08.018/counts and excluded work are preserved.

## Acceptance boundaries

The new focused suite compiles actual C11 and C++14 against real image/filter
objects. Native and ASan/UBSan runs cover original C and named routes, ownership,
metadata invariants, native materialization failures, invalid/closed/thread
boundaries and real loaded-cache cancellation/rerun behavior.

Six predeclared existing filter selections run in each mode:
saved_definition_is_separate; loaded_cache; object_arguments_do_not_cycle;
object_array_arguments_do_not_dangle; retired_definition_payload_reentry;
native_precision_loaded_cache_and_rerun. Two existing XCF selections per mode:
filter_cache_and_arguments and opaque_argument_shapes. This is explicit selected
regression coverage, not a full filter/XCF/GIMP suite pass. Unrelated giant spill
or disk-exhaustion scenarios are not required for the query/handle change.

No new ordinary UI or artificial screenshot. Provider additions are queries;
existing scheduler/dispatcher behavior must not expand to satisfy a test.

Failure-coverage qualification: expired object-array inputs exercise rejection
and cleanup inside the existing materializer before an output array is returned.
The new output-copy loop's partial cleanup is structurally reviewed RAII; no
runtime allocator-global fault or forced mid-loop boxed-copy failure is claimed.
GLib's fatal out-of-memory policy is not converted into a recoverable API.

The selected opaque_argument_shapes XCF fixture intentionally emits exactly two
"Invalid arguments" retention warnings and two "Unsupported or malformed saved
Filter argument; original definition is retained" warnings, on stderr only.
Its two malformed shapes are each reloaded twice; the test verifies exact opaque
byte preservation, zero execution and refused-save sentinel preservation.
Strict evidence verification permits only those complete messages/counts in
that exact selected case, rejecting extras, alterations, other cases or stdout.

## Final acceptance

17 focused native and 17 ASan+UBSan cases pass: nine actual-C11 and eight C++14.
All six predeclared filter and two XCF selections pass in each mode. Every
selected process, Meson command and authenticated X server exits 0. The opaque
fixture's four expected stderr retention diagnostics per mode are independently
source-derived and precisely checked, not hidden or generalized.

375 public-header probes and both header gates, four positive/three negative
handle compile controls, 48 policy tests, 90-contract/76-route checks, 12 standard
plus seven selected-XCF evidence rejection controls pass. Final independent
review found no blocker. Native application was incrementally relinked; all five
relevant original C symbols are present once in it and in both focused binaries.

Reproduce the focused suite using the established native and ASan builds:

    meson compile -C BUILD -j2 filter-entry-contracts
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs filter-entry-contracts

Run each exact selected existing regression using --test-args '-p EXACT_ID'.
See report.json for commands, source/binary/evidence hashes, expected diagnostics
and explicit partial compatibility/ownership limits.
