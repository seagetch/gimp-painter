# Defined old paper mode/selection scenes

The shared `../legacy-paper/capture.c` harness runs384 native old scenes with
Normal, Multiply, Erase, Replace, Anti-Erase, SRC-IN, DST-IN, SRC-OUT and DST-OUT;
Y/YA/RGB/RGBA; soft/hard/pressure; constant/incremental; absent/soft selection.
48 opaque DST-IN/OUT combinations are excluded because reversed sources make
the old replacement loop write the wrong destination channel count. No unsafe
capture output is used as a supported oracle. There are1,537 records including
complete initial/finish/Undo/Redo pixels and the completion marker.

`migration/tests/capture_paper_modes.py` records the unchanged old runtime.
See `../../contracts/ordinary-paper.md` for exact scope and safe extensions.
