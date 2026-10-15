# 07.008/untracked-signals: bounded FilterLayer correction

This is partial acceptance, not completion of the whole task. The original WBS
checkbox and all unrelated task statuses are unchanged. The source map accounts
for every one of the 60 assigned inventory rows. One registration is inside
literal `#if 0`, leaving 59 reachable candidates; several popover registrations
were assigned to members on preceding lines rather than discarded. The map
records those corrections without rewriting the historical inventory.

## Reproduced defect and correction

The replacement for the four old FilterLayer discarded dependency connections
already stores Connections in `FilterImpl::dependencies`. That alone was not
safe: `refresh_connections()` calls `invalidate()`, whose synchronous
`filter-state-changed` signal can close the owner or reparent it. The interrupted
refresh then appended new handlers after teardown, or appended registrations
for the old container after a nested refresh had selected the new one.

Two actual GIMP regressions reproduce these failures. Both baseline failures
observe one native root-container reorder handler where the pre-filter baseline
is zero. The corrected implementation retains the old container until return,
checks terminal scheduler state, and rejects superseded refresh revisions and
changed stack identity before installing handlers. No alternative owner store,
callback wrapper, legacy UI replacement or signal-name change is introduced.

The tests retain native emitters across controller close, query actual handler
registrations with balanced block/unblock operations, issue later source updates
and reorders, and verify that nested reparenting leaves only the newest stack
registered. Full-suite results also cover existing retained-filter/image/source
finalization and callback-close tests. The full native suite passed 149/149 in 269.34 seconds; the ASan/UBSan
suite passed 149/149 in 319.55 seconds, with leak detection disabled. Before
the successful sanitizer run, two setup attempts lacked the worker and then
the blinds plug-in; those failures are preserved separately. See `report.json`
for commands, source hashes and limits. An initial partial-path `-p` selection matched
zero cases; it is explicitly excluded from acceptance.

## Remaining acceptance

`source-map.tsv` separates actual regression testing from static ownership
mapping. Other modern routes still need their final-source runtime evidence
reconciled or rerun. Missing old UI routes are not silently counted as tested:
for example, PageRemind preserves notebook selection across newly created old
popups, while the modern persistent selector only preserves hide/reopen and
new/rebuilt editor notebooks do not implement that general behavior. Its UI
work remains separate; this patch does not implement unrelated UI to obtain a
signal-ownership checkbox. Historical generic Popover/Feature adapters are
retired; mapped caller/native-lifecycle replacements are not generic parity.

This patch does not alter 02.006 legacy-reader evidence, unfinished UI/session
acceptance, MyPaintUndo work outside this repository, connection-name acceptance,
full application/dependency leak checks or platform acceptance. Separate work
on those tasks must use its own evidence and task-ledger entries.
