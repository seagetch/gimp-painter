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

## Harness initialization correction

The initial harness (retained verbatim under `initial-harness/`) omitted setting
the paint core last coordinate after its first stamp. Its first interpolate
therefore also traversed from the core default origin, unlike the normal tool.
The corrected authoritative capture explicitly seeds that coordinate before
interpolation. No old renderer code changed; rate/Undo conclusions are checked
again on the corrected sequence. The initial recordings remain diagnostic
evidence, not discarded or passed off as ordinary pointer input.

The independent `dabs/` capture installs a transparent observer around the old
paint vfunc, forwards every call to the unchanged implementation and records
24 actual dab coordinates across the twelve cases. Its full pixel result is
byte-identical to the corrected authoritative capture. This also identifies the
old brush resource spacing of0.2, rather than the independent GIMP3 paint option
default of0.1, as the interpolation input required by this scene.
