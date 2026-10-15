# Opt-in multi-window toolbar host

This bounded child of `29.010` and `30.005` restores a usable dedicated toolbar
host, not merely a hidden notebook. The runtime gate is exactly
`painter-canvas-ui && !single-window-mode`. Ordinary GIMP profiles default to
false; proven Painter migration enables the existing preference. No new saved
preference, inferred tool/provenance gate, or process-global host singleton is
introduced.

## Legacy correspondence and qualification

At `afa43fae3e920210146abed514f136fd49f671b5`, `app/gui/gui.c:595–606`
marks the startup empty display; `gui-vtable.c:297–319` excludes the toolbar
host from reuse; `gimpuiconfigurer.c:569–610` creates/reuses it on multi-window
conversion; `gimpimagewindow.c:330–334,583–597` provides its role and hides the
owned notebook. The same image-window file at 439–442 comments out the toolbar
constructor while still packing its field, and uses a flip-button without a
construction site. Those incomplete paths establish intent and lifecycle
correspondence, not proof of a functioning standalone legacy presentation.
They must not be copied as defects.

## Required coherent behavior

- Exactly one host per Gimp instance owns one imageless shell. Its notebook is
  hidden; a separate parent-owned presenter supplies the actual independent
  horizontal tool factory inside a horizontal-only scroller. Overflowing controls
  must be reachable at portrait width without widening beyond the monitor.
  Native Tool Options Dockable widgets stay owned by
  their existing editor. Unsupported factories clear stale controls.
- Startup, ordinary New/Open, existing-image mode conversion, Painter
  enable/disable, and zero-image cases reconcile to the correct topology
  without losing images, selection, docks or temporary shell references.
- The owned active shell remains owned by the host. A separate non-owning
  action target follows the selected live image. Host menu/key actions use that
  image, never the placeholder. Focus remains local: a foreign canvas cannot be
  assigned as the host's primary focus widget in modern GTK/GimpWindow.
- Host WM Close invokes ordinary Quit with dirty-image confirmation and
  cancellation; File Close/Ctrl+W acts on the routed image. Closing the final
  image leaves the host usable. Destruction disconnects pending work and does
  not retain or destroy a foreign image shell.
- Existing `gimp-empty-image-window` session registration remains tied to the
  owned placeholder, not the routed image. Real writer/fresh-process restart
  evidence is required before claiming session restoration.
- Existing per-canvas bottom controls are preserved in this bounded unit.
  Removing or relocating them, move/reparent parity, all legacy UI semantics,
  other operating systems and sanitizer acceptance remain separate gates.

## Implementation boundaries

The host keeps its original imageless shell as its owned active shell. Action
lookup uses a separate live display selected through the user context; it does
not install a foreign canvas as the host focus widget. A hidden host placeholder
is excluded from image first-draw readiness checks so it cannot block real
image keyboard input. Session lookup explicitly finds the owned host, while
ordinary empty-display reuse excludes it. An image losing its final reference
cannot claim the host's singleton session slot while it is being deleted.

Clean single-window Painter-only changes reconcile presentation without
consolidating unrelated external docks. Actual mode changes and interrupted
conversion replay still perform topology conversion. Dock-side classification
uses the selected real image's pre-conversion center, independently of the host
window chosen as the merge destination. Host display/image actions reject a
context shell while it is detached or absent from its live owner's membership.

Topology conversion retains temporary object references and checks live
membership after callbacks. Nested changes supersede current intent and replay;
unchanged action-state notifications are idempotent, including while native
session docks await their restore idle. When Painter is disabled, an interrupted
host's demoted empty window is retired alongside real image windows, but a lone
ordinary empty window is preserved.

A destination dock window receives geometry and is mapped before transferred
docks are inserted. This preserves GTK 3 overlay-scrollbar preferences and
allows the moved subtree to realize before viewport allocation emits adjustment
changes. Mapping only after insertion reproduced a GTK 3.24.49 NULL overlay
indicator callback following unrealize/reparent. Source-close, nested conversion
and repeated mode changes exercise the corrected ordering.

Portrait reachability uses two independent horizontal scrollers: one for the
independent tool controls and one around the existing native menu. The menu
wrapper is created only when a window first becomes a host and remains
parent-owned for that window's lifetime. Demotion changes its horizontal policy
to NEVER, restoring the native menu-driven minimum and full row allocation.
Windows never promoted keep their original native hierarchy. The menu and its
model retain identity; fullscreen/custom-titlebar transitions move the complete
row, and menu visibility applies to that row. Leases and early wrapper publication
protect synchronous first-attachment callbacks. If native fullscreen temporarily
holds a parentless menu, the existing weak reconciliation idle retries after the
native parent is restored.

Temporary diagnostic logging and backtraces were removed before the acceptance
snapshot. Passing child-process stderr remains visible to the strict output
classifier. A native Move-options fixture sets its type before taking the
descendant snapshot; redundant later type notifications legitimately rebuild
labels, so retaining the old snapshot there would test a stale fixture rather
than ownership. Exact descendant membership and parent assertions remain.

## Acceptance status

The bounded host unit passes on the source hashes recorded in
`migration/tests/multiwindow-toolbar-host.json`:

- Full default native build, including strict headers.
- Nine affected registrations: 231 TAP cases, zero skips, crashes or unexpected
  diagnostics. The compact-options suite retains its four expected negative
  diagnostics, classified without a baseline exemption.
- Entire dedicated host suite at scale 2: 22 cases. Both focused portrait/menu
  cases also pass. The portrait bound is 540 logical pixels at this scale.
- Independent read-only ownership/reentrancy review and unchanged source seals.
- Installed application at authenticated TCP Xvfb 1080x1920: a real focused-host
  shortcut changes Move to Paintbrush, actual horizontal wheel events expose
  the rightmost Expand Layers controls, and normal GUI Quit returns zero.

Coverage includes all four startup gate combinations; New/Open and two-image
routing; active/inactive/last-image close and Quit cancellation; repeated mode
and Painter toggles with zero/one/two images; controls and native-dock ownership;
true finalization; real session writer/restart; existing compact, canvas and
dock regressions; menu/model identity, first-wrap fullscreen reentry and native
row restoration on demotion. Prior failed attempts and fixture corrections are
retained in the receipt rather than counted as acceptance. This is targeted
acceptance; the earlier exact b6f0 full aggregate remains a separate baseline.

## Pending session-restoration interruption

The startup combinations and post-startup conversions do not establish safety
for a genuinely changed mode/Painter request while native session dock-fill
idles are pending. Same-value action feedback must be idempotent, but a changed
request can still encounter a session-created empty dock whose children have
not yet been restored. `29.010/startup-session-mode-barrier` tracks an explicit
restore-pending barrier and replay of the latest requested configuration, with
saved dock/control preservation. Skipping the native restore callback or its
docks is not an acceptable repair. This follow-up stays open.

## Transient action-dispatch boundary

A live real display can be temporarily ownerless during remove/add callbacks.
Direct host display/image lookup rejects that state. The bounded event repair
defers manager state synchronization and consumes host key/button events
while that attachment gap is present, avoiding toggle synchronization against a
still-selected detached image. This is distinct from the normal no-image host.

It does not establish transactional safety for a menu already open before the
gap, or direct global GAction activation inside a reentrant transfer callback.
`29.010/toolbar-transfer-menu-transaction` remains open for that broader action
framework obligation. Programmatic activation deliberately bypasses sensitivity
in the existing action implementation and is not equivalent to GTK key/menu
dispatch. No general callback-suppression mechanism is introduced here.
