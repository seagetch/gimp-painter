# Native layer presets, version 1

This is the WBS28 implementation/acceptance contract. The eight bundled assets
are byte-for-byte copies from `afa43fae3e920210146abed514f136fd49f671b5`.
`layer-presets-v1.schema.json` enumerates all fields used by those assets plus
all additional fields the pinned applier interprets. Unknown fields at every
level are retained in the resource JSON and in filter definition bytes.

## Source semantics

The old `app/presets/layer-preset.cpp` is the static source. Independent runtime
construction is recorded in `fixtures/legacy-layer-presets/construction.tsv`
and the accompanying original stdout/stderr/provenance files. Do not call a
static schema audit runtime evidence.

- `version`: integer 1. `name` is the resource display name. `desc` and `#` are
  retained annotations, never executed
- `source-layer`: object with optional type (`any`, `normal`, `group`, `clone`,
  `filter`) and optional Boolean `alpha` source-matching requirements
- `replacement-layer`: ordered array. Top-level insertion starts at the source's
  position in its current parent. Group children start at position zero
- Each node's `target` is `new` or `source`. `source` moves/edits the exact existing
  object. It does not duplicate it. Its name/type/filter/children fields do not
  replace its definition. New type defaults to `normal`
- New `normal` with `source` duplicates pixels as an ordinary layer and retains
  the duplicate's native name. Without content it is transparent, with alpha
- New group children retain array order. Groups inherit source mode/opacity when
  the corresponding field says `source`
- Content `source` and the old unfinished `#anything` reference both mean the
  original source object. No name-based arbitrary alternative is invented
- `boundary`: absent means leave source geometry alone/use full image dimensions
  for new raster layers; `source` copies its rectangle; `full` uses the image;
  `selection` uses channel bounds; four integers give x1,y1,x2,y2 in image space
- `mode`: exact old enum nick (`normal-mode`, `dst-in-mode`, etc.), `source`, or
  an explicit legacy numeric value. Numeric values go through the existing
  Painter conversion, never the GIMP3 enum's coincidental integer. Unknown string
  nicks retain the old fallback (source unchanged; new normal/group/clone normal,
  new filter Replace)
- `opacity`: number in [0,1] or `source`. Absent leaves source unchanged; new
  layers default to 1. The legacy `alpha` construction field is retained but did
  not change alpha creation and is not given newly invented meaning
- New filters retain procedure name plus full original filter JSON and typed
  positional arguments. Bundled FLOAT values are cast through float then stored
  as double, as the old PDB runner did. INT32 becomes native signed int; INT8
  unsigned int; STRING string. Unknown/unsupported RGB/INT8ARRAY fields remain
  in raw JSON and uninitialized typed slots. Edge/Gauss first three placeholders
  normalize to zero run/image/drawable IDs, matching old runner defaults
- The executor remains the existing exact, allowlisted native Edge/Gauss runner.
  Presets cannot invoke arbitrary saved PDB names or scripts

Measured bundled quirks are intentionally preserved: `test.json` changes its
source filter's mode to difference but does not install its JSON filter on that
source. `test2.json`'s `Multiply` is not an old enum nick: its new Gaussian filter
uses Replace, and its source keeps its previous mode. No asset is rewritten.

## Ownership and transactions

The resource is a normal C `GimpData` subclass with an owned JsonNode copy,
GInputStream loader and GOutputStream saver. Search/writable paths are native
GimpCoreConfig properties, with a dedicated extension-manager path property,
GimpDataLoaderFactory, startup/refresh/tag cache/save/teardown hooks and installed
assets. There is no NewGClass, private C++ placement, dynamic singleton registry,
or second GObject ownership bridge.

The short-lived C++ planner uses the common ObjectRef and exception boundary.
The applier is a stack object; no UI path g_object_unref()s a C++ instance.
Preflight validates dimensions, types, hierarchy bounds and probes construction
before modifying the image. The Undo description is an owned string snapshot,
so application callbacks may rename the resource without invalidating it. The
original parent is retained across probe construction and cleanup; source
attachment, image, parent and sibling index are rechecked afterward before Undo
starts. A changed target is rejected without undoing the observer's independent
edit. Actual evaluation then follows the old depth-first
order, so repeated source edits and later source-valued fields/duplicates observe
the earlier changes. Allocations during actual evaluation remain inside rollback. During application, native undo records
are captured into private transaction stacks. A cancellation/failure rolls back
only those records and restores pre-existing Undo, Redo, dirty counters and
selection. On success the actual group is published as one native Undo step.
It must not be nested inside an extra group: native Redo reverses only the
published group's child list. Unrelated history is never cleared on failure.

The native GTK3 dock uses the standard data-factory view, with source selection,
activation, Apply and Refresh actions. It suppresses initial/refresh selection
side effects, supports one selected source at a time and reports incompatibility
without modifying the image. Preference entries are static C data: the old
returned GArray leak and null-array indexing do not exist. Action callbacks are
static functions registered by the standard action factory, so no destroyed
C++ registry owner is retained in the factory cache.

## Evidence boundaries

Acceptance tests exercise all eight old construction traces, one Undo and Redo,
resource unknown-key roundtrip/independent duplication, malformed input, inert
unknown filter procedures, ordered repeated source edits, malformed definitions without prior
history loss, cancellation after real insertion, insertion-callback resource
rename, preflight construction/cleanup callbacks which reparent, reorder or
detach the source, native configured factory
search/refresh, and editable anime/watercolor XCF save/reopen plus source edits.
Native UI and sanitizer results are separate artifacts; no compile-only result
is described as UI or runtime parity. The old capture retains JsonResource
objects until process exit to avoid its observed broken JsonNode-as-GObject
finalizer. Its construction/applier code is unchanged; retained old warnings
are included verbatim rather than filtered away.
