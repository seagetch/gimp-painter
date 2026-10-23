# Definition/cache generation contract (10.013)

This defines how saving during execution avoids confusing definitions with
caches. It refines [Filter lifecycle](filter-layer-port.md) and [v1 records](xcf-painter-v1.md).
Acceptance here is the specification and source correspondence, not a new native
running-save test or complete scheduling acceptance.

## Four distinct identities

- **Current generation**: Filter input/definition evaluation lineage.
- **Cache generation + complete flag**: lineage of the last committed pixel
  cache. Completeness alone does not imply freshness.
- **Definition revision**: session-local same-object equality token, changed by
  definition installation/edit/Undo/Redo, not by lower-layer/cache changes. It
  is not persisted and cannot authorize a reopened job.
- **Binding/job tokens**: runtime callback freshness capabilities. Persisted
  counters are never imported as these tokens.

The saved tuple is `version=1`, current/cache `uint64` generations,
`cache-complete` boolean, and diagnostic saved-state. Accept only
`0 <= cache <= current <= G_MAXINT64`; reject future-cache/overflow lineage
before mutating live state. Saved-state is validated but not literally resumed.

| Saved relationship / phase | Persisted cache and reopen expectation |
| --- | --- |
| Complete and cache=current | Committed pixels remain clean; no initial evaluation merely because of reload. |
| Complete but cache<current | Keep older committed pixels as stale; request fresh evaluation. |
| Incomplete, including equal counters | Never certify clean; request fresh evaluation. |
| Preparing/running/importing with a newer generation | Save the committed drawable cache plus the newer definition and stale lineage; never label worker/staged output completed. |
| Valid zero or maximum accepted counter | Relationship remains meaningful; normalize to a fresh runtime epoch, not reuse the absolute number. |
| Invalid tuple | Reject transactionally; do not partially install its generations. |

`restore_cache` cancels active work and advances the local generation, derives
freshness solely from equality/completeness, resets preparation and sets clean
or waiting/cancelling as appropriate. A callback from an old job cannot become
fresh by a persisted counter collision. Successful staged import publishes
cache metadata and atomically swaps the buffer; commit exceptions remove the
completeness claim. Worker buffers are distinct from the committed drawable.

## Save consistency boundary

Save preparation builds immutable semantic capsules and duplicates committed
GEGL drawable buffers. It retains Filter generation/state checkpoints, Clone
references and change observers. `xcf_painter_save_unchanged` rejects any
observed mutation or changed current/cache/completeness/state tuple. The native
save path checks that predicate before successful destination publication and
again before final close. A changed scene is refused, not silently assembled
from different moments. This contract does not promise arbitrary concurrent
mutation can be saved successfully, nor serialize unfinished worker output.
Definitions/typed arguments/raw original evidence are separate fields; saving a
new definition does not rewrite old provenance into purported new cache input.

## Verification scope

The source-consistency verifier checks snapshot guards, epoch normalization,
checkpoint comparisons, saved fields and native save checks. Existing native
test definitions `filter_running_save_close_reopen` and
`filter_importing_save_close_reopen` exercise real spill jobs; scheduler tests
include saved lineage and colliding old-job rejection. Their presence is checked
here, not reported as freshly executed. Full runtime evidence remains separate.
