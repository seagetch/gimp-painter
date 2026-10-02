# Real old ordinary paper

`capture.c` links the rebuilt pinned old application archives and calls the
unchanged native BrushCore/Paintbrush code. `pixels.tsv.gz` has216 paired raw/
textured masks plus24 complete stroke/Undo/Redo scenes (529records). Pattern
channel layouts1–4, asymmetric/even/large byte brushes, soft/hard/pressure,
fractional and negative-x positions are included. The negative-y invalid-read
path is never used.

The same harness has two explicit extra modes. `PAINTER_PAPER_MODES_FIXTURE=1`
captures384 defined mode/selection scenes to `../legacy-paper-modes`;
`PAINTER_PAPER_TRANSFORM_FIXTURE=1` captures1,296 transformed/bitmap/generated
mask pairs to `../legacy-paper-transform`. The native port trace adapts image
versus drawable coordinates and explicit native brush spacing, not oracle
pixels. See `../../contracts/ordinary-paper.md` for supported/undefined scope.

Recapture with `migration/tests/capture_paper.py` after sourcing the prepared
legacy build environment, holding `/workspace/shared/gimp-painter-build.lock`.
The report pins behavior sources, archives, harness and executable hashes.
The captured old startup diagnostics are retained; they are not paper failures.
