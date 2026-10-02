# Real old Fill Brush paint, erase, selection and rate

This harness executes the unchanged pinned `GimpBucketFillBrush` vfunc and native
old paint-core interpolation/constant-mode transaction. Twelve cases combine
rate0/50/100, paint/erase, selection absent/present, a bitmap brush and an offset
layer. Each records the entire finish, Undo and Redo RGBA result (36 snapshots).
All four rate groups are byte-identical: the old rate property is stored and its
dynamics value computed, but it does not affect these actual strokes. Undo
restores every initial byte, and Redo exactly restores the finish snapshot.

The test is synthetic artwork, not an interactive tablet capture. The original
old defaults/bitmap and dynamics are described by `capture.c`; the exact pinned
source hash, archive identities, link command and captured binary hash are in
`runtime.json`. The default legacy test profile produced retained plug-in
registration/unwritable pluginrc diagnostics. Capture exit0; the missing profile
path was not written. No runtime source patch was made for this capture.

`pixels.tsv.gz` contains only BRUSH lines and the final completion marker.
`runtime-capture.log.gz` retains all actual old stdout, including noisy brush
registration/rendering diagnostics. The new port comparison is a separate gate;
these old fixtures alone do not establish new-tool parity or responsiveness.

Capture code and synthetic pixels are GPL-3.0-or-later. The old application uses
its upstream GPL licensing; there is no private artwork in this fixture.
