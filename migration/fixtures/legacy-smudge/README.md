# Actual legacy independent Smudge stroke oracle

The pinned application `afa43fae3e920210146abed514f136fd49f671b5`
runs its unchanged native `GimpSmudge`, BrushCore interpolation and PaintCore
transaction. The48 synthetic scenes cross Gray/Gray-alpha/RGB/RGBA, rate0/50/100,
color-blending off/on and fixed/dynamic brush size. Pressure drives the separate
blending output, and size-enabled cases vary pressure across input events.
The layer is offset, and strokes traverse both color and transparency boundaries.

All192 full native-byte snapshots (initial, finish, Undo, Redo per scene), plus a
completion record, are stored in `pixels.tsv.gz`. Undo equals the entire initial
buffer and Redo equals the entire final buffer in every scene. An independent
second capture reproduced all2,952,800 normalized bytes exactly. Four no-alpha,
zero-rate/nonblending cases make no pixel change, as expected from this trace;
we do not require a visually changed result to manufacture a passing fixture.

The original implementation first updates its accumulation buffer through
`blend_region()`, rounding effective rate to an8-bit value. With alpha, that
operation weights the old accumulator with `blend+1` and the sampled pixel with
`255-blend`; its signed integer division and alpha right shift matter. Optional
`blending-output` shades a separate output buffer toward foreground using
`shade_region()`. It does not replace the accumulation state with that shaded
color. The accumulation rectangle starts from the ceiling of the transformed
brush diagonal and is retained for the stroke; per-dab size dynamics crop the
paint area within it. These contracts must be compared explicitly with the
modern floating-point Flow implementation, not treated as an alias.

`capture.c`, exact source/archive/binary hashes and link commands provide
provenance. No old renderer source was changed. Legacy-build compatibility
changes are described in the existing baseline preparation evidence. The old
test startup emitted its known plug-in registration and missing default
pluginrc-path warnings, retained in the raw log/stderr. Both captures exited0.
There is no GUI, tablet, new-port, high-precision or sanitizer claim here.
Capture source and synthetic artwork are GPL-3.0-or-later.
