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
close own/store state before parent dispose; free final C resources then parent
finalize; pass unknown property IDs to the actual defining parent handler. Paint,
projection, save and interface order must follow the per-slot audit, not a global
"always call parent first" rule.

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
