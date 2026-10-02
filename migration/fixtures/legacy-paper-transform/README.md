# Old transformed mask inputs for paper

The shared `../legacy-paper/capture.c` harness captures1,296 actual old raw and
textured masks (2,593records):4 pattern layouts,3 bitmap plus3 generated brush
shapes,3 scale/angle/aspect configurations,3 hardness modes and6 fractional/
negative-x positions. Native BrushCore supplies both masks; the port's paper
kernel consumes the raw mask and must match the old textured mask byte-for-byte.

This is exact texture arithmetic/phase evidence, not complete GIMP3 native
transformed/generated geometry equivalence. That follow-on uses explicit
legacy-origin options and shared proven transform helpers.
