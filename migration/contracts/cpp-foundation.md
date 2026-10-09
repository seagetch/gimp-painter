# Painter C/C++ foundation contract

This contract implements the module boundary in migration/design.md §8.4.1.
It is not evidence that the legacy feature adapters or GIMP runtime work.

## Placement and dependency direction (04.001)

`app/painter` contains the shared C++14 ownership, binding and callback machinery.
It depends only on GLib/GObject; it does not depend on GTK, GIMP app types, GEGL,
JSON, the brush engine, PDB, or any feature registration object. The existing C
sources remain C. Core, paint, display, tools, widgets, presets and PDB adapters
may use this library; it must never include a feature header or invoke a feature
initializer. Type-specific C headers stay with their owning app module. Private
C++ implementation headers are not installed or included by C translation units.

The sole C++ object API is a named typed handle built on `ObjectRef<T>`. Handle
methods call the existing C operation; C operations and exact-signature vfunc
adapters resolve the same typed slot. Neither a second `Interface::cast` API nor
`ref(object)[function]` is introduced. Registration uses normal C-compatible
GObject instance/class structures. GType identifiers are acquired lazily, after
GIMP initialization, never by C++ global constructors.

## References and borrowing (05.002–05.004)

- `retain`: acquire one reference to a valid, borrowed object; no floating-ref
  conversion. Null yields an empty handle.
- `adopt`: consume one already-owned reference, without increment or sinking.
  Null yields an empty handle. Failed type validation consumes nothing.
- `sink`: for floating objects, consume the floating reference; otherwise acquire
  one additional reference. The caller must choose this factory intentionally.
- Copy adds one reference. Move transfers without increment and clears the source.
  Empty, self-copy and self-move are valid. Destruction drops exactly one reference.
- `get()` and values supplied by a binding lease are synchronous borrowed values.
  They must not outlive the handle/lease or be captured into deferred callbacks.
  BindingStore additionally rejects returned references and direct Impl pointers
  at compile time. Capturing a borrow indirectly still violates this contract
  and must be reviewed; a compiler check is not a proof against arbitrary captures.
  An asynchronous completion uses a weak owner and a generation token; the worker
  owns only independent input, output and cancellation state.
- Type validation is against `TypeTraits<T>::type()`, using GType ancestry rather
  than C++ RTTI or object-layout assumptions. Only valid GObject pointers may be
  supplied: arbitrary invalid addresses cannot be validated by GLib.
- GIMP/GTK handles are created, copied, locked and released on their owning UI
  thread. A generic GObject reference does not imply that the object is thread-safe.

## One object, one store (05.006–05.010)

A private, process-wide quark owns a single BindingStore through qdata. `ensure`
may create the store only in a type's construction adapter or an explicit behavior
installation path. Lookup never constructs. The store owns each implementation
with a unique_ptr in a compile-time-declared slot. A slot declares its object type
and implementation type together, so a free-form string cannot select a type.
Different base/derived/behavior slots may coexist on one object. They do not alias
one another even when their implementation types are equal.

State transitions: constructing -> active -> closing -> closed -> destruction.
Construction failure may go directly constructing -> closing -> closed. Slot
registration is permitted only while constructing, rejects duplicate identities
and incorrect owner types, and retains no partially constructed slot on failure.
Active stores cannot replace or erase an implementation. Type adapters activate
after inherited and own construction properties are ready. Construction-property
handling uses explicit adapter state and must not invoke active-only callbacks.

`close()` is idempotent and non-waiting. Before invoking close hooks it marks the
store closing and increments its generation, so reentrant callbacks cannot start
new mutations. It calls slots in reverse registration order (derived before base
when their instance initialization follows GObject order), then marks closed.
Each close hook must be noexcept, detach signals/sources, request cancellation and
release references; it must never join a worker. Store destruction invokes close
if needed, then destroys implementations once. dispose/destroy/controller-close
must explicitly invoke close; qdata destruction alone is only the final fallback.

A call lease retains the owner throughout a synchronous operation. Reentrant
close cannot destroy an implementation while the lease is on the stack. A closed
store allows only explicit const reads required by serialization, cleanup or parent
callbacks; mutation attempts fail. The owner thread is checked for store access.
A live strong handle is not permission to mutate a closed object.

## Boundaries, callbacks and errors (05.011–05.013)

GObject instance/class layouts remain C structures. Each vfunc adapter has the
exact current slot signature and normal assignment; incompatible function-pointer
casts are prohibited. Parent vfunc invocation is an explicit type-specific choice:
close own/store state before the documented parent dispose/destroy phase; free
final C resources then parent finalize. Property handlers distinguish their own
properties, inherited owner-class dispatch, explicit parent fallback and invalid
IDs; they do not blindly forward every unknown numeric ID. Paint,
projection, save and interface order must follow the per-slot audit, not a global
"always call parent first" rule.

[The original 05.011 type-specific contract](cpp-vfunc-order.md) and its
machine-readable inventory record every current application C++ GType and native
compatibility counterpart, including deliberate instance-init activation,
post-construction installation, inherited-only handlers and conditional types.
They classify all legacy handle entries without claiming the remaining feature
implementation and exception/lifecycle gates are complete.

All C entries and callbacks catch C++ exceptions, map them to a declared error or
safe return and leave output arguments initialized. They never use exit, throw a
pointer or let exceptions unwind through C. A type's class initialization must be
fallible by explicit adapter state: prevalidate declarations before registration,
record a terminal initialization error if setup fails, and do not retry an already
registered incomplete type. The common boundary does not claim to recover from
process-aborting GLib allocation failures.

Expected errors distinguish wrong type, missing slot, duplicate registration,
invalid lifecycle state, closed owner, wrong thread and unexpected exceptions.
GError** follows GLib's standard precondition: NULL or pointing to NULL. A missing
GError** is allowed and never turns failure into success. No callback may hold a
mutex while issuing GObject notifications or invoking another callback.

Signals retain handler identity and a weak emitter; disconnect/block/unblock check
that the emitter and handler still exist. Sources are owned by their GSource
pointer, not only a recyclable integer ID, and are destroyed on close. Destroying
a source during its callback must defer its user-data destruction until dispatch
unwinds. Disconnection is paired with weak-owner/generation validation wherever a
callback might already have been queued.

## Migration end condition (05.014)

The source scanner must reject legacy `Interface::cast`, `ref(...)[...]`,
NewGClass private placement, free-form painter qdata ownership, and obsolete
TileManager/PixelRegion use in migrated implementation paths. Existing unrelated
upstream qdata is not blanket-prohibited. All entries in cpp-types.tsv, generated
GType inventories, vfunc inventories and call-boundary inventories must have a new
adapter and runtime evidence, or a specifically proven non-feature exclusion.
Passing independent foundation tests does not close that all-feature requirement.

## Type and slot inventory

`cpp-handle-registry.tsv` assigns one named handle contract to all 90 distinct
legacy declared/runtime types found in pinned C++ traits, cpp-types.tsv and the
macro-generated GType inventory and explicit C-entry owners. Repeated declarations and underscored C struct
aliases are consolidated. The table's implementation column deliberately stays
TODO until that feature adapter is compiled and tested. Removed GdkDrawable has
an explicit GTK3 window/Cairo route; ImageGenerator is retained in the ledger
until its exclusion prerequisites are proven. This registry is not an assertion
that all the handles already exist.

The ten owning behavior attachments in `cpp-owning-data-review.tsv` move to
separate declared slots on their existing owners (widget decorator, preset
configuration, numerical editor, curve editor, brush editor, options editor,
MyPaint tool core, popup view creator, binding action, editor button actions).
Each slot's declaration binds the exact owner GType and implementation; generic
helper templates instantiate distinct tag types per role. No behavior creates a
second owner store, steals another slot, or treats a plain C++ model as a GObject.
Signal closure ownership is separate from implementation ownership.

The registration implementation defensively holds the owner during construction,
reserves the pending slot identity, rejects nested registration/activation and
rechecks lifecycle before publication. Rejected completed constructions close
and destroy their state exactly once. This defensive check does not authorize
production constructors to fire callbacks. During final destruction, every
implementation access is rejected and reentrant close is a no-op.

## Minimal mixed-language round trip (original 04.006)

The C-compiled `tests/test-c-api.c` now registers three named `/painter/interop`
cases. It calls the C++ implementation in `tests/test-foundation.cpp`, which
calls the real C callback and uses the production `boundary.hpp` conversion.
The test checks the callback input/count and returned values, the exception's
GError domain/code/message, zeroed failure output, caller-owned error cleanup,
and omission of the optional error output. No production API is changed.

The [native result](../tests/cpp-roundtrip-native.json) records all 37 foundation
cases and the three-case selection passing, the actual C/C++ compiler commands,
plain C symbol references in both objects, and the C++ final link to the real
bridge archive. There are no canonical source-duty rows assigned to 04.006.
This closes its original minimum criterion; GObject lifecycle (04.007), full
application linking (04.008), platform and feature acceptance remain separate.
Earlier SHA-frozen reports describe their original snapshots and are preserved.

## Minimal GObject callback order (original 04.007)

The existing C `PainterFixture` installs ordinary GObject instance, constructed,
property and dispose callbacks. Those C functions call the C++ typed-slot
adapter, using the production BindingStore and its normal destruction path.
`test-gobject.cpp` records a bounded, allocation-free event trace and checks
order at construction, after property callbacks, after repeated dispose, and
after releasing the last owner reference.

The [native result](../tests/cpp-callback-order-native.json) observes:
`construct → construct-property → activate → get → set → get → close → get → destroy`.
Two explicit dispose calls close once; a closed-state read precedes destruction.
The selected lifecycle case and all 37 native foundation cases pass. Actual C
and C++ objects retain the four unmangled adapter references/definitions. This
is the original minimum test, not completion of every migrated type or platform.
The unrelated legacy configure hunks retain their own corrected source duties.

The [configure source review](../inventory/configure-hunk-routing-review.json)
proves that the thirteen previously assigned `configure.ac` hunks contain build
and resource configuration, not GObject callbacks. Their ninety-one blanket
source duties are replaced by forty-nine exact TODO duties; all 22,899 unrelated
rows and 204 completed source records are preserved. The three routing consumers
now agree on each hunk (and the file-level union). Run
`python3 -B tools/check_configure_hunk_routing.py --check` to reproduce this
bounded checkpoint from the two byte-verified archived blobs. Full historical
generator execution was blocked by unavailable pinned source/base trees and is
not claimed. Completed baseline WBS criteria are unchanged; the new source-level
comparison duties do not reopen those original criteria or imply completion.

## Final C++ runtime link (original 04.008)

Both GUI and console targets explicitly use `link_language: 'cpp'` while
`main.c` remains C. The old empty `dummy.cpp` linker-selection trick is replaced
by this target property. The common bridge and current feature archives join
their owning final targets; HTTP core/GUI archives are conditional.

The [fresh native result](../tests/final-cpp-link-native.json) builds actual GUI
and console executables with HTTP disabled and enabled. All four use a C main
object and C++ final driver, retain `libstdc++.so.6`, the C bridge and migrated
C++ feature symbols, and exit successfully on isolated `--version`. C was not
compiled as C++; no explicit C standard flag is claimed from the DWARF C11 tag.
Replacing the console driver with C and removing the explicit C++ runtime link
fails in both configurations with the expected GLIBCXX unresolved dependency;
the production executable hashes remain unchanged.

The [25 source-duty mappings](../inventory/final-cpp-link-scope.json) cover all
40 counterpart paths through actual compiler, object, archive/direct-input and
generator ownership evidence. Only these 25 execution records close; all source
identities and other rows are preserved. This is final-link/runtime acceptance,
not proof of unused registration-member retention (04.009), complete feature
behavior, other platforms, or installed operation. No listener or GUI workflow
was started. The evidence archive retains exact reproduction scripts and logs.

Seventeen enum/PDB recipes per configuration ran against isolated source copies
to protect tracked generated C sources. Their outputs matched, all compile/link
recipes stayed unchanged, and 9,915 tracked source hashes matched before/after.
The immutable earlier source-ledger correction reports remain historical
checkpoints; current mutable work status is validated by the granularity tool.

## Static archive order and retained roots (original 04.009)

The [native archive check](../tests/archive-order/native.json) replays all four
actual default/HTTP GUI/console links into separate outputs and obtains
byte-identical executables. GNU rescan groups resolve their cyclic static
archive dependencies. Seventy applicable explicit-root records retain 122
required symbol observations, with compiled consumer references and actual
archive-member extraction reasons. Lazy GTypes are not required to register
at startup. The maintained roots cover current implemented paths; future
feature registrations must extend their own root contracts.

Twelve controlled comparisons expose both failure modes: removing grouping
breaks the two real console links on app-provided symbols, and restoring only
grouping resolves them. An unrooted real Clone type member drops even in a
group and returns with an explicit root. Separate small fixtures prove
constructor-only registration dropout and A/B cycle ordering. Five checker
self-tests verify failure detection. No GIMP/GUI/server/profile is launched.

The [27 source-duty mappings](../inventory/archive-order-scope.json) bind the
42 current counterpart paths to native extraction or configuration evidence.
Preset application has only GUI consumers, so its member is absent from both
console maps and present in both GUI maps; this reviewed omission is recorded.
HTTP sources are excluded when disabled. The two JSON configure hunks also
have [default](../tests/json-link/default.json) and
[HTTP](../tests/json-link/http.json) direct-dependency evidence: eight positive
links, eight failures without the JSON library, two failures without its
headers, and four real Resource reader/writer smoke runs. Isolated Resource
links omit GEGL/Soup/GTK; complete app links retain required GEGL APIs and still
fail if only the direct JSON library is removed. Full old-file parity remains
with the feature tests. The existing JSON child is not checked by this update.

No production link defect required a code change. Only original 04.009 and its
27 source execution records close. The default checker validates the frozen
historical checkpoint and recorded translation-unit/configuration identities;
it does not bind all headers or discover future source-list additions. Such
changes require fresh builds and link evidence. The executed checker is
preserved separately from a documented wording-only clarification. Platform,
feature-runtime and earlier historical-source-drift gates remain separate.


### Direct JSON dependency acceptance (existing 04.009/json-dependency)

The default/HTTP direct-link reports published with 04.009 satisfy this existing
child's dependency criterion. All ten recorded source identities still match.
`json_glib` is required and appears directly in the core archive, MyPaint
Resource archive and its exported internal dependency. Native preset objects
have real JSON references: omitting JSON from strict final links fails even
with GEGL/Soup available. Real Resource links and reader/writer smoke succeed
with only JSON-GLib/GIO/GLib and fail when JSON is removed. This establishes
independence from accidental transitive JSON linkage; removing the full app's
semantic GEGL APIs is not asserted. No new native run is attributed to this
acceptance update. Complete persistence fixtures and platform gates remain open.

## Enforced compiler exception policy (original 04.010)

Painter C++ code may throw internally; its C entries use the common result/void
boundaries to return declared errors or safe fallback values. The build now
requires a supported exception-enabling switch and a successful exception-macro
and `try`/`catch` compile probe. GNU-style compilers select `-fexceptions`;
MSVC-style argument syntax selects `/EHsc`. An unavailable switch or a compiler
that accepts it without exception support stops configuration. The Meson APIs
used here are within the declared 0.61 minimum: see the
[compiler API reference](https://mesonbuild.com/Reference-manual_returned_compiler.html).
Only native GCC was executed in this acceptance.

`painter_exception_args` is separate from RTTI flags and is applied to core and
display in addition to the existing Painter targets. Before this change, sixteen
Painter translation units per configuration relied on GCC defaults and failed
when an earlier global flag disabled exceptions. Current default/HTTP probes
cover 73/78 production C++ commands, including 64/69 Painter units; all Painter
units explicitly enable exceptions and override that earlier disable. A final
disable still fails. The five configure controls include the old silent-filter
behavior, rejected switch and accepted-but-disabled compiler cases.

The [native results](../tests/exception-policy/README.md) rebuild both affected
archives and the common test in both configurations. Each foundation run passes
39 cases. Two C-compiled cases make sixteen calls through the real result/void
boundaries: typed Error, standard exception, injected `std::bad_alloc` and a
non-standard exception, each with and without GError output. They verify safe
FALSE returns, error identity, one RAII cleanup and C caller continuation. Actual
C references and C++ definitions have the same unmangled entry symbol.

Every C compile command remains unchanged (1,865 default / 1,867 HTTP). The only
changed C++ arguments are the exception switch on twenty core/display commands
per configuration, including four existing upstream `.cc` sources. RTTI flags
are unchanged. The guarded generators preserve tracked C outputs and all 9,938
tracked source hashes during the bounded builds.

This completes the compiler/common-boundary criterion, with no canonical
source-duty rows assigned to 04.010. It does not complete every feature's error
or class-initialization migration, recover process-aborting allocation failure,
or prove another platform, sanitizer, or full application relink. Earlier
source-frozen reports remain evidence for their recorded snapshots.


## RTTI independence (original 04.011)

The [RTTI contract](rtti-separation.md) distinguishes the frozen legacy downcasts
and runtime keys from GType checks and exact Slot identities. Native default/HTTP
proof compiles all forty core/display C++ objects with RTTI disabled, runs the
common 39-case suite per configuration and tests the real typed bridge. Existing
production flags are unchanged. Exception typeinfo and focused sanitizer vptr
metadata are explicitly separate from the new API's RTTI independence.

## Private C++ visibility (original 04.012)

The [visibility contract](symbol-visibility.md) keeps private declarations and
template derivatives out of the application's dynamic ABI, while explicit C
entry attributes preserve functions involving hidden record types. Native
before/after ELF checks cover all six default/HTTP application executables,
SDK/module controls, C compile vectors and C/C++ record layouts. The C++ inline
visibility switch has separately recorded effects on generic/upstream inline
symbols; no C export, SDK or module entry is removed.

## Explicit initialization phase (original 04.018)

[Current native object evidence](../tests/static-initialization/README.md) covers
all 73/78 application and Filter worker production C++ inputs in the default/HTTP
builds. No initialization root is present before main; lazy function statics and
standard GObject type getters remain explicit-use paths. The maintained ELF check
includes startup sections, arbitrarily named constructor roots, TLS initializers
and IFUNC resolvers, with real GType/GTK positive controls and metadata omission
controls.

Here “after GIMP initialization” means within controlled application startup or
later, never a namespace/global constructor. It is not a prohibition until the
function gimp_initialize returns: gimp_new invokes gimp_constructed and the
ordinary paint registration phase earlier. C resource/CRT/dependency constructors
are outside the C++ object invariant. The separate feature-entry-point lifecycle
obligation is not closed by this foundation check.
