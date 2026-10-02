# Remaining XCF integration gates

Audited 2026-10-02 after the reader, native v1 writer, opaque-definition,
external-reference, capsule-schema and registered recovery-save checkpoints.
This is an implementation/test handoff, not completion of tasks 10–12.

## Native envelope validation now covered

The earlier Filter definition-before-cache-validation defect is fixed. Filter
cache fields, counter relationships, definition flags/bytes and name encodings
are validated before custom construction; Clone fields/state and optional names
are likewise checked before construction. Common mode and original-name fields
are checked before replacement. Invalid active envelopes remain ordinary opaque
proxies and cannot be rewritten as edited custom semantics. Native Filter
freshness is applied only from a validated snapshot, with no catch-and-mark-loaded
fallback. The current application tests cover 42 invalid envelope scenes and
seven valid boundary/name scenes, including exact duplicate/edit/resave retention.

Remaining adversarial review should cover incompatible native owner contexts
(for example custom-layer metadata placed on group/channel/path records),
post-validation allocation/adapter failures and property-order/context conflicts.
The envelope checkpoint does not claim a blanket audit of every construction
failure or every namespace/owner combination. Preserve active semantics rather
than merely archiving the original bytes when adding those recovery paths.

## Saved-field checkpoint

The field/owner/default ledger is now `../inventory/xcf-field-acceptance.tsv`,
with exact assertion/fixture and remaining-scope columns. See
[xcf-field-acceptance.md](xcf-field-acceptance.md). This adds genuine old text,
unit/palette/path fixtures, editable text/config checks, locks/attributes across
native owners, profiles/metadata, item sets, modern effects, wrong-owner capsules,
explicit large-retained-record Save refusal, and actual active Filter Save cases.
The ledger distinguishes coverage from the remaining adversarial/platform gates.

## Historical saved-field inventory and ordinary semantics

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
