# Independently closable legacy work (01.017)

The **2,489 hunk assignments and 402 asset assignments** are expanded into
**2,891 source checks** in `granularity-review.tsv`. Multi-action assignments,
including whole-file additions, no longer rely on one completion bit:
`legacy-port-work-items.tsv` has **16,153 implementation obligations and 6,881
verification obligations**, each with exactly one concrete WBS action and its
acceptance condition. All **23,034 execution rows are TODO** at creation.

These are source-to-action cross-references, not 23,034 new features or an
estimate of separate code changes. One implementation or test artifact may
cover many related source rows, but each applicable row must explicitly cite
that evidence. An inventory DONE row never completes a port or behavioral
comparison automatically.

Every work item has a stable ID derived from source kind/ID, phase and task;
the pinned blob/range/diff hash (or asset identity hash); source scope; action;
acceptance condition; execution state; owner; artifact; test and result fields.
A group with three implementation actions and two independent tests has five
separately closable rows. There are no section-only IDs, empty task lists or
comma-separated multi-action work rows.

## Additional splits found during the granularity pass

Mixed files were examined beyond their filename-level role:

- `app/tools/gimp-tools.c` contains custom MyPaint/bucket/ruler registration,
  placeholder registration, compact-GUI ownership and saved tool grouping.
  Those hunks now point separately to their concrete feature actions
- `app/core/gimp.c` contains brush loader/factory restore/save, generic preset
  factory lookup/lifetime and tool-item lists. Those responsibilities are
  separately routed; shared core type declarations are registration contracts
- Display events contain ruler lazy snapping, ordered motion compression,
  overlay focus/hiding, rotation, radial drag scaling and transformed panning
- `23.014/event-compression-order` now retains the old event ownership and
  reinjection sequence. Matching motions require the same widget, window,
  state and device; a following nonmatching event is returned to the caller
  for later dispatch rather than silently discarded
- `25.009/drag-zoom-scroll` retains Ctrl radial drag scaling and transformed
  pan coordinates. The old scaling factor is distance plus the recorded start
  offset, divided by `ZOOM_UNIT_DISTANCE=300`, with pointer/cursor teardown
- `25.008/mirrored-arrow-input` independently preserves Left/Right key
  remapping in mirrored display state
- Tool-item drag/drop and selection data are assigned to the grouping model;
  paint-mode menu additions are assigned to the compositing enum/UI contract

The declared source range remains the original hunk. A whole-file addition may
support many actions; no synthetic smaller range is fabricated to claim a
function-level proof. Its independently stated behavior actions and tests are
all exposed in the work ledger, and future implementation can refine source
locations while preserving original provenance.

## Gates and reproducibility

```
python3 tools/assign_legacy_hunks.py --check
python3 tools/audit_legacy_candidates.py --check
python3 tools/audit_auxiliary_scripts.py --check --syntax
python3 tools/audit_standard_paint.py --check
python3 migration/tests/test_bucket_selection_contract.py
python3 tools/audit_legacy_granularity.py --check
python3 migration/tests/test_legacy_assignment_gates.py
python3 tools/check_tasks.py
```

`check_tasks.py` now rejects checked 01.013–01.017 rows when the corresponding
source set, assignment IDs, concrete WBS targets, identity, audit state or
expanded action rows are missing, duplicated or inconsistent. It does not
substitute an audit for runtime equivalence.

The seven negative tests cover missing/duplicate work, section-level or
multi-action substitution, source/range/hash/acceptance drift, DONE without
evidence and release acceptance of unfinished rows. The release check
`python3 tools/audit_legacy_granularity.py --require-closed` intentionally fails
now. The same closure requirement is applied before 38.016 or 38.017 can be
checked, after their legitimately later dependencies can be completed.

Regeneration preserves execution status/evidence for unchanged contracts and
refuses to discard active/completed evidence when source/action contracts
change. If a WBS action or acceptance text is intentionally refined, regenerate
and review the resulting delta; do not erase an already verified item.
