# GIMP Painter 3.0 migration workspace

`design.md` is the migration contract. `../tasks.md` is the authoritative WBS;
every finished task has a matching record in `progress.tsv`. The initially
targeted upstream series is GIMP 3.0, on branch `gimp-3-0-port`. The separate
`gimp-3-2-port` branch is only a future candidate and has not been validated.

| Path | Contents |
| --- | --- |
| `baseline/` | source and destination revisions, versions, build conditions |
| `inventory/` | file and hunk assignments, saved-field inventories |
| `fixtures/` | distributable legacy documents, brushes, input events and expectations |
| `tests/` | runnable compatibility checks and reports |
| `measurements/` | time, memory, UI occupancy and image differences |
| `packages/` | build and distribution manifests; binary outputs stay out of git |
| `progress.tsv` | task ID, state, owner, artifact, tests, outcome and remaining limits |

Check the WBS after each update with `python3 tools/check_tasks.py`. A new
finding is recorded as a child `parent-ID/target-key` in `tasks.md`, then
assigned a dependency, an implementation action, and an independent check.
The parent remains open until all its children are complete. For an external
block, set only the affected task to BLOCKED in `progress.tsv`, describe the
constraint, and continue tasks with satisfied dependencies.

Completed means the row's stated exit criteria have evidence, not merely
that code was written. A change in baseline requires an explicit revision of
the metadata and affected task dependencies. Preserve the old implementation
as the behavioral reference, including cases where the modern GIMP API uses
the same feature name with different semantics.
