# Native DnD / reentrant container notification contract

Bounded follow-up **08.017/dnd-container-notification-contract**. Production DnD and native container code are unchanged. This repairs test assumptions invalidated by the independently validated `08.018/container-reentrant-membership` lifetime and notification contract. It does not close the broader migration or claim sanitizer coverage.

## Cause and stronger replacement checks

- During `items-changed` after removal, model membership is gone but `gimp_container_remove()` still holds its temporary call reference. The DnD source snapshot adds the other reference. Exactly **two**, not one, must be present. Initial/final single-owner assertions and weak finalization assertions remain unchanged. Removing DnD snapshot retention must still fail.
- `gimp_container_insert()` may reorder the inserted object. The coherent outer GListModel notification can replace the whole model, so `added` counts replacement rows, not native admissions. Existing anchor rows are included. Native `add`/`remove` observers now measure actual admissions/removals; all original exact expected operation counts are retained.
- Adoption fixtures additionally replay every GListModel splice into a borrowed identity vector before reentrant adoption callbacks, checking bounds, current length, and every object identity. No persistent reference is added by this probe. Existing native tree-model identity/order, selected-item topology, newer-owner precedence, rollback, weak-admission behavior, and finalization checks remain.

## Validation

Initial and restored green runs each pass **107/107**, exit 0, without skips. Both compiled production mutations fail at the intended new assertions: removing the snapshot ref yields 1 instead of 2; under-reporting replacement rows yields replay length 1 instead of model length 2. Production sources are byte-identical after restoration. See compact JSON results and source seal. The original full canvas group runs without skips/exclusions on authenticated loopback-only **1080×1920×24 Xvfb**, including its enabled-oncanvas tests. DnD fixtures themselves invoke native class vfuncs and model callbacks; this is not a synthetic mouse-gesture or screenshot claim. Visual dock results remain independently recorded under `../dock-empty-populated-portrait/`.

The original seven baseline failures are retained. An intermediate probe correction is recorded: once the lifetime assertion passed, the same obsolete replacement-count assumption surfaced at the single-transfer destination observer; that observer now counts native admissions too.

Raw build/Meson logs are private because they can include inherited environment. Only allowlisted case names, statuses, assertion messages, durations, and source hashes are published.
