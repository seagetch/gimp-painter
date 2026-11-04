# C42: bounded GtkImage identity observation

Local diagnostic candidate on published C41 afa3ed81bef58241e65afbf16ab5844a3aa60f12,
tree4983054ddd202bd50f44bf6ea07f9281a3911a6a. No publication or native acceptance.
Task308 remains DOING. The original3s requirement and leak guards are unchanged.

## Evidence and source decision

Run58 completed with5 successful jobs and3 failures. Normal Brush21/21 both scales
and the fixed107 mask suite pass; the old mask control fails as required. Sanitizer
case14 still fails its first wait at8,623,012us, stable0, longest dispatch8,619,363us.
Other20 leaf assertions pass but their processes report198696–201336 bytes leaked.
The support-pin repair is verified; readiness and leak acceptance remain unresolved.

The separate C41 diagnostic captured complete declared leaf coverage and successful
restoration. Its GimpImageWindow-host group contains89 GtkImage width calls totaling
5,499,992us; text width714calls/407,286us and pixbuf get_size596calls/169,633us are
smaller. Other owner buckets are zero. These debugger wall durations are not primary
performance timings. Role3 groups all image-window instances, including later ones.
89 calls do not establish89 distinct images, repeated decoding or a cold cache.

Source review found existing same-size guards in gimpeditor.c:1034–1049,
gimpitemtreeview.c:752–804 and gimpdrawabletreeview.c:298–311. The display-shell pixel
size setter also has GTK's equality guard. A generic redundant-size fix is therefore
not justified. gimpdockbook.c:282–291 recreates tab labels on style update, reaching
new GtkImages through gimpdockable.c. That is a concrete path to investigate, not a
proven cause: skipping tab refresh could break theme or geometry behavior. Forcing
pixel dimensions can change lookup/geometry and defer work to drawing. Neither
change is made on this evidence.

## Added observation

Before the unchanged first-wait timer, the existing separate diagnostic snapshots
GtkImages beneath existing GimpImageWindow hosts. It traverses internal children,
including notebook tab labels, through gtk_container_forall. Fixed limits cover
16hosts,128 scanned toplevels,4096 queued nodes,256 image identities,64 tree levels,
and128 ancestor steps. Weak queue slots prevent stale sibling dereferences across
virtual traversal; finalization/reparenting during setup makes the snapshot
inconclusive. Snapshot and host identities are weak, never strong retained owners.

The existing exact-forwarding GtkImage width thunk gains numeric per-identity entry
and exit observation. A token contains no object pointer; no widget is dereferenced
after the original callback. The old leaf source is recovered byte-for-byte by
removing the two profiling lines, preserving all C41 class-slot restoration logic.
Image and leaf entry/exit counts must agree before the collector accepts capture.

Metadata contains storage type, logical/pixel size, visible/mapped/child-visible/
realized state, state flags, scale and fixed ancestor/host codes. No icon names,
widget names, paths or addresses are exported. Snapshot, first-entry and last-entry
metadata distinguish changes. Public getters do not load an icon surface; scale is
read only after bounded ancestor traversal succeeds. Metadata is read before
forwarding, without size requests, icon lookup or callback allocation/printing.
Destroy notification and weak lifetime are separate facts. Weak-cleared slots are
never reused, so a replacement at the same address cannot inherit an old identity.

Known identities have first-observed and repeated-call inclusive durations. First
observed does not mean first-ever or cold; repeated requests do not prove repeated
SVG decoding. Nested inclusive spans may overlap and must not be added as disjoint
work. Untracked calls have a separate aggregate, not a unique/new-image count.
Mapped or visible does not establish painting, visibility through ancestors or
occlusion. Fixed ancestor categories direct later source inspection without names.

The strict IMAGE_DIAG parser validates bounded order/counts, signed-size sentinels,
metadata ranges, origin/summary/metric consistency, canonical missing metadata and
same-wrapper leaf counts. Missing semantic ancestor types alone permit explicit
partial capture. Other faults are inconclusive. Raw debugger text remains excluded.
The existing1MiB report,2MiB wire,96-frame and owned-child lifecycle limits stay fixed.

## Limits and validation scope

The snapshot covers existing objects, not all later/replaced images. Observed
untracked calls make identity coverage partial. gtk_container_forall cannot stop
its own sibling enumeration early; queued work/storage are bounded, not arbitrary
GTK callback count. Toplevel-list allocation occurs before timing and is not claimed
allocation-bounded. Snapshot traversal invokes existing virtual enumeration methods.
No whole-GUI or whole-layout coverage, cold-cache classification, causal attribution,
production optimization or latency acceptance is claimed.

Synthetic verification uses real GObject weak lifetimes and signals with explicitly
synthetic GTK methods, not a native GTK/GIMP run. It covers internal children,
duplicate enumeration, destruction/reparenting during traversal and forwarding,
retained destroyed objects, weak-cleared reused addresses, late/other-host unknown
calls, metadata changes, nested durations, bounds and invalid states. The existing
leaf/owner/controller tests and source/identity closure are retained. The final
aggregate logs and independent review accompany the frozen candidate.
