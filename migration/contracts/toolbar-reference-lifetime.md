# Canvas toolbar reference lifetime

This is the bounded child `29.010/toolbar-ref-balance-canvas-lifetime`.
The broader move/reparent and separate toolbar-window obligation remains open.
Legacy `afa43fae3e920210146abed514f136fd49f671b5`,
`app/widgets/gimptooloptionstoolbar.c:328–335`, retained an extra reference on
hide. The current Canvas owns toolbar widgets through RAII, hides panels
without adding a persistent reference, replaces the old independent GUI on tool
change, and releases toolbar/panel ownership on close. The regression verifies
that behavior rather than reintroducing the removed legacy ownership code.

`test-painter-toolbar-lifetime.inc` observes the toolbar created by the real
Canvas without retaining it. It pairs a weak pointer with a private qdata
finalizer witness: destruction/disposal alone is insufficient proof that no
strong reference remains. A retained-then-destroyed actual-toolbar control
must remain unfinalized until the explicit last owner releases it.

The main case exercises eight toolbars across 64 hide/show cycles, duplicate
visibility setters, visible and hidden tool switches, visible and hidden shell
close, and reopening. It also observes finalization of the toolbar container.
An anchor display keeps the same real Tool Options Dockable alive; snapshots
verify cached native widget identities, parentage and orientation throughout.
The fixture preserves an existing startup empty display, returns its image
slot to empty, restores its Canvas visibility, and restores multi-window mode
before retiring a newly created anchor so pre-existing docks can separate.
Final checks require the original display count and existing Dockable identity.

Independent review found and corrected the startup-display cleanup issue before
integration. The source/API review is complete. Native focused/full/HiDPI and
sanitizer execution remain pending; this draft claims no runtime acceptance.
This unit does not supply the missing separate toolbar window, foreign-active
shell routing, toolbar-window session geometry or complete legacy UI parity.

## Native validation

The [receipt](../tests/toolbar-reference-lifetime.json) records both focused
cases passing in 3.59 seconds, all 62 compact-options cases passing in
107.73 seconds, and both focused cases at GDK_SCALE=2 passing in 3.42 seconds.
The strict aggregate diagnostic classifier finds no skips or unexpected
diagnostics; existing intentional negative-case warnings remain visible.
No production code changed. Sanitizer/other-platform checks, toolbar moving
and dedicated-window ownership remain outside this acceptance.
