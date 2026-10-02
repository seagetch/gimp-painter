# Remaining XCF integration gates

Audited 2026-10-02 after the reader, native v1 writer, opaque-definition,
external-reference, capsule-schema and registered recovery-save checkpoints.
This is an implementation/test handoff, not completion of tasks 10–12.

## First: known-v1 malformed semantic fields

`app/xcf/painter-xcf-preserve.cpp`, `xcf_painter_restore_layer()` and
`xcf_painter_restore_bindings()` require a transactional malformed-v1 audit.
The source currently converts the ordinary proxy before validating every
semantic field. A Filter definition is installed before cache fields are
validated; the `std::exception` recovery branch then calls
`gimp_filter_layer_mark_as_loaded()`. Thus the control flow can certify an
unvalidated cache and a later save can rewrite the imported fields from a
partially restored model. This control flow is source-confirmed; a dedicated
corrupted-cache runtime fixture is still required.

Next acceptance checks:

- Missing/wrong-typed procedure, presence flags, definition bytes, generation,
  cache completeness and state, plus invalid counter relationships
- Missing/wrong-typed Clone ID/state/name-policy fields and conflicting kinds
- Unknown legacy mode on ordinary and custom layers
- No partial executable definition or falsely fresh cache after a failure
- Retain the exact active capsule and committed pixels, either as an inert
  opaque proxy or another explicitly preserved unsupported state
- Duplicate, edit, Undo, save and reopen without silently turning malformed
  active semantics into default/empty valid semantics

Duplicate dictionary keys and unknown/wrong-typed item kinds already have real
Open/Save coverage. That does not validate every field in a known v1 kind.

## Saved-field inventory and ordinary semantics

`migration/inventory/xcf-wire-records.tsv` is the source-grounded wire inventory,
not a completed field-by-field application roundtrip matrix. Existing tests
cover representative hierarchy, masks, channels, selection, paths/references,
resolution, modes, opacity, offsets, raw provenance and typed values. Complete
explicit checks are still needed for text editability, profiles/palettes/units,
all locks and attributes, metadata/parasites/item sets, and modern effect-specific
records. Tie each supported field/default/owner to a fixture and comparison.
Do not infer a loss merely from missing test coverage.

## Corruption, limits and resource behavior

- Expand native known-property, hierarchy, tile and recursive-graph malformed
  cases; the source snapshot/bounded probe does not replace upstream audits
- Exercise resource-limit recovery and both genuinely valid dialects beyond
  the current dual-valid and short-declared-extension fixtures
- Test platform-specific temporary-file unlink/close/failure behavior; Linux
  large-input and cancellation coverage is not a Windows guarantee
- The whole source has no arbitrary size cap, but each serialized metadata
  parasite is limited below 256 MiB. Decide a chunked representation if a valid
  retained definition/origin exceeds that limit; do not silently discard it
- Arbitrary live object internals outside supported image/item identities still
  require opaque state or preflight refusal, rather than fabricated objects

## Save while work is active and application lifecycle

Current tests cover pending/fresh/stale caches, exact saved pixels, reentrant
edits, cancellation checkpoints and five injected write failures. They do not
complete real RUNNING/IMPORTING Filter save/reload/re-evaluation, queued main-loop
completion, simultaneous image close, or sustained GUI lifecycle stress. Repeat
these against the latest integrated Filter spill pipeline and common source
snapshot; persistence counters describe freshness, never reusable runtime tokens.

The recovery helper exercises the real registered procedure and reopens backups.
A fatal process, backup discovery/prompt, rename failure and actual crash-handler
lifecycle remain distinct gates. Existing recovery is best effort and is not
async-signal-safe. No new autosave schedule was implemented.

## Final integration and platform gates

Run the complete current shared build and applicable suites after concurrent
feature changes. Focused sanitizers instrument a declared subset of GIMP, not
all dependencies; leak detection is disabled. Big-endian encoding logic exists
but has not been validated on a real big-endian application. Full native GUI
Open → edit → Save → reopen behavior and cross-platform packaging remain open.
Compositor pixel coverage is owned by the mode workstream; the XCF route does
not itself prove every historical blend mode or precision combination.
