# Common layer property native popups — 29.006/common-layer-native-popups

Status: **DONE for this bounded native-popup leaf; run18 accepted**. This is a new reconstruction on
`d4ce384f708090facbf6b08edc38b4ddcf84cff0`, not recovery of unpublished task307
source or reuse of its earlier test results.

## Bounded source contract

The reference is gimp-painter 2.8 commit
`afa43fae3e920210146abed514f136fd49f671b5`:

- `app/widgets/popupper.cpp:136–149,190–242,391–406`: runtime `GimpPopover2`,
  `GTK_WINDOW_POPUP`, GTK grab, Escape cancellation, consumed outside
  cancellation (`TRUE`), and no global Return/Space confirmation bindings.
- `app/widgets/popupper.cpp:270–377`: area-aware `show_over` uses
  `GTK_CORNER_BOTTOM_LEFT`. The missing brace at 311–323 makes its effective
  vertical preference **above first**, then below when above cannot fit.
- `app/widgets/gimplayertileview.cpp:734–736` and
  `app/widgets/gimpcellrendererpopup.cpp:161–165`: tile and tree routes use this
  host/decorator family, not an independent ordinary top-level dialog.
- `app/widgets/gimplayerpopup.cpp:829–854,881–891`: common opacity, alpha lock
  and mode controls write live layer values with Undo-enabled setters. Closing
  the outer popup is not an implicit rollback, save, or staged Apply.

The implementation converts only ordinary/Clone **property hosts** in tile and
tree entry paths to the existing sole native GimpPopover2 provider. It keeps the
current GTK3 controls and action routing, including Clone Source and Edit Layer
Attributes secondary dialogs. Existing mode policies are preserved: the tree uses GROUP context for groups;
the tile keeps its existing LAYER context even for a group target. This carrier
change does not introduce new tile pass-through semantics.
It does not claim identical old widget layout, all old controls, GTK2 ABI, or an
executed old2.8 visual comparison.

Filter property hosts retain their existing factory, Apply/draft controller,
event policy and placement helper. Tile creation retains GtkPopover. Tool
popups retain their separate helper and outside-event policy. Staged
assembly and request-gated control hooks are confined to common properties.
The original Filter/creation method bodies remain byte-identical to the base
after removing only the common-route dispatch; default hooks and Filter unmap
policy are preserved. Their inherited regressions remain required.

## Placement and visibility qualification

`gimp_popover_show_at` is separate from the tool helper. Its opening anchor is
an area in the target widget's coordinates: the selected tile preview row, or
the tree's full-row viewport rectangle after bin-to-widget conversion. It
retains the old effective edge-flip/above-first ordering, including its strict
below-fit comparison. A final monitor clamp is an explicit modernization for a
popup that fits in that monitor; it is not pixel-identical preservation of an
old off-screen result. An oversized popup cannot be guaranteed fully visible.

Placement is determined at opening, not continuously tracked against a moving
anchor. Only a later actual allocation increase may constrain the existing
window position to the opening monitor. The scale2 regression must cause real
mode-group/model growth and assert the whole resulting host remains visible;
no coordinate tolerance or assertion exemption is permitted.

## Ownership and reentry

Complete detached content, local control pointers and local Connection owners
exist before a host is created or published. Leases protect storage, while
immutable request/close epochs, image/context/target identity, attachment,
mapped-target and host-ancestry guards decide whether a request is still valid.
Staged handlers cannot edit a replacement popup's model. Same-request aborts
release the tree's retained target and image; they never clear a newer request.

Owner unmap, owner destruction, image changes, target removal, native host
destruction and explicit close retire the active request and disconnect its
controls. Tile close has a distinct serial so existing post-close action
continuations retain their request-generation semantics. No detached partial
host may remain mapped or retain a grab after an abort.

Rapid Escape/reopen and the queued GDK double/triple-press companion are an
explicit runtime gate. The common area helper records only an actual current
normal opening press on its anchor. The native handler consumes one matching
2/3-press companion only when time, button, GdkWindow, device and root coordinates
all match. CI attempt 8 observed opening companions without premature closing;
run18 passed the complete provenance/mismatch gate at both scales. The native host handler
has no blanket multi-press exemption or grace interval;
any fresh or mismatched outside press retains consumed cancellation. Tool and
Filter callers do not opt into this opening-event provenance.

Common tile and tree hosts stage an owned copy of a normal outside cancellation
from their **own bound receiver**: tile list/add, or the bound GtkTreeView, for
accepted buttons 1/3. Destruction confirms the still-current exact normal event
before that marker can be used. A receiver consumes one matching 2/3 companion
(time, button, source window, device and both root coordinates) before side
effects. For tiles this precedes row lookup; for trees an early `::event` hook
precedes generic native row activation. Unmatched classified inputs retain
their original policy, including ordinary tree double-click activation and
Filter/creation opening. Next button/key input, release, explicit close and
owner lifecycle clear the marker. A vetoed cancellation never arms it; later
unrelated destruction cannot confirm the staged copy.

Attempt 8 proved a consumed normal close followed by a same-owner tile
companion reopening the host. Attempt 10's supplementary tree probes proved
same-source double companions opening Edit Layer Attributes/Clone Source after
normal cancellation, even though the popup-cell path itself required a normal
press. These are GTK3 robustness corrections, not claims that old2.8 filtered
event types. The native outside TRUE result is unchanged. Cross-owner routing,
from tree to other tiles or arbitrary underlying widgets, is **not established**;
there is no global event filter or receiver registry. Attempt 10 exercised the
tile correction's exact/mismatch and rapid cases at scale2. Attempt12 passed
the complete 58-case common matrix at both scales, including tree closing
provenance, unchanged mismatch policies and the actual classified-click oracle.

A narrow additional Canvas lifetime guard is included: a retained layer editor
must still be parented before Canvas::show calls its show-name setter. Destroy
can release that editor's private children even while a strong root reference
keeps the editor object alive. This is a source-demonstrated invalid-lifetime
path exposed by destructive-owner tests, not a Canvas layout/model refactor.
A pinned historical staged-child-1 GDB run now demonstrates this exact setter
path during Fixture destruction, ending in a fatal GObject critical/SIGTRAP,
not the original unlocalized SIGSEGV. Identical signal reproduction remains
unproven. Attempt12 passed all tile/tree destructive cases at both scales.
Fixture destruction and all its assertions are retained.

## Accepted native evidence

[Run 40198528473](https://github.com/seagetch/gimp-painter/actions/runs/40198528473)
accepted source `d6d3126cc7ca5b7cb71ff48212cc6639f50bc145`: 58 common cases
at each portrait scale, six separate offscreen gate cases at each scale,
thirteen adjacent registrations/360 cases, genuine header checks (453 probes),
361 catalogs and unchanged-source verification. All sixteen raw 1080×1920
upper/bottom captures were individually inspected with default oncanvas enabled.
Physical lower-control reachability is tested, not inferred from widget existence.
Native014 supplies the final coherent compilation receipt. All seventeen compiler
inputs match the exact tested source manifest `04b3053c24509ad643b6782197bd80abcf5a74582c3e474500a01323e2c87e9f`.
The four pinned306 carrier RED controls and immutable historical controlled-order
RED remain separate evidence, not positive candidate cases.

The detailed attempt history and accepted evidence are in
`../tests/common-layer-native-popups/README.md`. Earlier failures, incomplete
passes and source/test corrections are preserved there; successful reruns are
not retroactive passes for failed revisions. The exact tested source-manifest
file is retained as a pre-acceptance snapshot; its historical DOING field does
not override this scoped final acceptance.

## Owned offscreen paint ordering qualification

A narrowly scoped gate in `gimpoverlaychild.c/.h` owns one native freeze token
and a logical GTK layout/after-paint generation witness, with explicit
unmap/remap and teardown cleanup. It adds no forced allocation, update
processing, warning suppression or global interception to production.
The focused tests prove actual new-page insertion and observed current-label
allocation before exact native child-window drawing, eventual paint, supported
pending teardown, one-shot late invalidation, redraw-only liveness with an
independent freeze token, and a fresh remap witness. Each case also reaches the
real common popup by physical input.

This gate grants a layout opportunity, **not** a universal descendant-allocation
guarantee. Empty/clipped or absent invalidations and mutation inside a running
draw remain qualified. GTK's own exhausted repeated-layout warning remains
fatal and unmodified. The separately flagged ACTIVE existing-update diagnostic
reproduced the historical native tab warning inside its controlled call; this
proves controlled-order susceptibility, not identical natural-event causality
for attempt14. Attempt17's failed wait omitted component counters, so its exact
missing component remains unproven. The corrected focused oracle tracks actual
native page insertion and current-label replacement; it never substitutes
positive dimensions for observed allocation.

## Remaining obligations outside this leaf

Only `29.006/common-layer-native-popups` is closed. This bounded leaf consumes
the existing native provider, default-oncanvas session and Filter route work;
it does not close parent29.005/29.006, other popup families, full-port/aggregate,
cross-platform/sanitizer or old-runtime visual equivalence obligations.
Historical GtkPopover ownership wording in `popover-lifecycle.md` remains
historical evidence, superseded only for these accepted common property hosts.
The broader `08.019/popover-placement-and-decoration` and all-parent UI work
remain separate. The fixture keeper tab, minimal test resources, portrait
status clipping and expanded-scrollbar overlap of some right-edge value pixels
are disclosed in the test README; they are not full installed-startup or
all-UI visual acceptance. No unrelated task is marked complete.
