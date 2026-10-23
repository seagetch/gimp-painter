# Painter extension record definition (10.012)

This is the original **record specification** criterion: represent layer type,
references, procedure, arguments and uninterpreted information. It consolidates
existing [v1](xcf-painter-v1.md), [reference persistence](filter-parameter-persistence.md)
and [transport](xcf-multipart-transport.md) contracts; it is not full save/runtime
acceptance. Names/versions are defined in [the registry](xcf-extension-identifiers.md).

All semantic records are canonical little-endian GVariant dictionaries `a{sv}`;
keys are unique. A missing optional key is not automatically equivalent to a
present empty value. The following logical fields are independent of inline or
multipart transport. Type notation is GVariant notation (`u` uint32, `t` uint64,
`b` boolean, `s` UTF-8 string, `ay` byte array).

| Record / fields | Type and meaning |
| --- | --- |
| All `version` | `u`, exactly 1 |
| Image `dialect` | `s`, `gimp-painter-standard-v1` |
| Item `id` | `u`, transaction-assigned nonzero unique file-local identity |
| Item `kind` | `s`: `ordinary`, `clone`, or `filter`; current model type, not historical class |
| Item optional `legacy-mode` | `u`, explicitly mapped legacy mode, never blindly cast to standard enum |
| Clone `source-id`, `source-state` | `u`: file-local source identity (0 means no active file target), core reference state |
| Clone `source-expired`, `allow-name-lookup` | `b`: reference state and explicitly retained lazy-name policy |
| Clone `pending-name`, `source-name`, `original-source-name` | Optional nullable bytestrings: present `ay` includes trailing NUL; absent means NULL, present one-byte NUL means empty |
| Clone optional `unresolved-source-id` | `u`, preserved diagnostic missing identity; not an active lookup candidate |
| Filter `procedure` | Optional nullable bytestring as above; exact current procedure name |
| Filter `has-definition`, `definition` | `b` plus `ay`: distinguishes absent definition from present zero-length raw bytes |
| Filter `has-arguments`, `arguments` | `b` plus converted `a(sbv)` model, or current uninterpreted field retained as its original variant; absent field remains distinguishable |
| Filter `generation`, `cache-generation` | `t`, validated saved lineage, not a runtime job token |
| Filter `cache-complete`, `saved-state` | `b`, `u`: completeness and diagnostic execution state; committed pixel cache stays in normal XCF pixel storage |
| Original `original-owner` | `s`, imported owner type provenance |
| Original `original-header`, `original-properties`, `original-extension` | `ay`, immutable original byte evidence, not regenerated from edited semantics |
| Original `opaque-record-sequences` | `aay`, exact ordered unknown record sequences; existing original archive is not recursively replaced with itself |
| Original `original-argument-model`, `original-argument-procedure` | Original variants retained when replacing imported converted/opaque state; distinct from current model |
| `external-reference-origins` | `aa{sv}`, bounded diagnostic reference provenance, never lookup authority; privacy restrictions in v1 contract |

## Typed arguments and references

Each `a(sbv)` element contains declared GType name (`s`), explicit null flag
(`b`), and type-specific payload wrapped in `v`. Nested `GimpValueArray` payloads
are another `a(sbv)`. Object and core-object-array payloads are `a(uuxsbb)`, each
reference storing, in order:

1. kind `u`: 0 unset, 1 the saved image itself, 2 same-image item, 3 expired/unavailable;
2. file-local item ID `u` (zero when not a bound same-image item);
3. original recorded runtime ID `x` (signed64 provenance, **never** a process-table lookup key);
4. declared object GType name `s`;
5. `was_set` boolean;
6. `expired` boolean.

Only an acquired weak-target lease may establish live file identity. Missing or
external identities stay unresolved; no same-name, path, network or runtime-ID
substitution. Actual type validation and same-image checks apply on restoration.
Scalar payload encodings are the exact codec in `painter-xcf-arguments.cpp`:
float/double use raw uint32/uint64 bits, integer widths are validated, strings and
arrays distinguish NULL from empty, and unsupported object internals are never
fabricated. Depth32 and65536 aggregate slot/reference bounds apply to typed
models. Unknown current models are preserved opaque rather than interpreted as
empty; incompatible edited semantics refuse preflight instead of destroying them.

## Evidence boundary

The source verifier checks field constructors, nullable string representation,
argument/reference tuple layouts and unknown-record archive keys against the
implementation. It records source hashes. This is definition consistency, not
native serialization/reload or every scalar codec's runtime validation. Generation,
unknown retention, size limits and format ambiguity have their own following tasks.
