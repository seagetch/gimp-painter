# Layer Presets physical-click correction (08.018)

Public base: dfbc5d5a79e63ad3f34a4b87df488e5e2526d7b1.
Narrow scope: app/widgets/gimplayerpresetview.c and app/tests/test-layer-presets-ui.c.

One ordinary primary click applies once and creates one Undo. Clicking an already
selected preset again reapplies once. An icon-cell physical double-click is two
physical operations, with no third operation from the activation/release callback
combination. Writable name-cell double-click retains native rename behavior; it
is a different editing gesture, not a two-application requirement.

Explicit Apply and independent API/keyboard selection retain their meaning.
An API selection or Apply between a pending press and release retires that
pointer gesture. Programmatic selection emitted synchronously inside another
handler for the same current GTK pointer event is attributed to that event;
explicit Apply is the separate direct-operation entry point.

The per-view token uses weak preset/image/source identities and event device,
window and wrap-safe timestamp matching. Every matching release retires the
token, including blank-space releases that emit no selection callback. Borrowed
release event identity is used only for equality during dispatch, never
dereferenced; a newer reentrant gesture clears it. Context image-changed clears
pending state immediately, including A -> B -> A before release. Context changes
reconnect the observer, and dispose disconnects/releases it. Escape/focus-out,
unmap, drag begin/end, broken grabs, factory freeze and dispose also invalidate.

Final acceptance requires all 16 UI + 11 backend + 19 editor cases in native and
ASan+UBSan, exact source registration/TAP identity, normal process exits, unchanged
90/120-second registration limits and no timeout multipliers. LSan is disabled.
No full GIMP-suite pass, broader adapter closure or perspective guard approval is
claimed. 141/661 and 08.018 remain open; 76 exact C entries and 31 separate hooks
are not reclassified by this fix.

Regression evidence preserves the unchanged-production failure at first release:
three image layers instead of two, after exactly one native press/release.
Intermediate fixture failures are retained, not relabeled passes: Escape was
observed at the wrong widget (GimpWindow consumes it and changes focus); reopened
mapped trees needed their native cell allocation to settle before hit testing.
Both fixtures now observe actual dispatch/layout and retain cancellation/hit-test
assertions without forced widget geometry, event-signal shortcuts or timeouts.

Not runtime-covered: writable-name rename gesture, a second physical device,
reentrant close during pointer-triggered apply, nested synthetic same-event API
selection, forced backend-error retry, drag/factory-freeze interruption. These
are separate from the 46 registered case results and must not be implied green.

## Verified result

46 native and 46 ASan+UBSan cases passed, all three processes exited 0 in each
mode. Raw integrated before/after screenshots show two clones for one baseline
click versus one candidate clone, with one candidate Undo restoring the source.
Candidate normal application quit exited 0. See report.json for hashes, unchanged
time limits, verifier controls, retained failed stages and precise exclusions.
