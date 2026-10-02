# Transparent old Smudge probe

The wrapper in `capture.c` forwards the unchanged pinned old paint vfunc and
records case 12 after each of four dabs: coordinates, accumulator bytes, and
subsampled mask bytes. All 193 standard records still match the independent
48-scene authoritative capture exactly (2,952,800 normalized bytes).

This diagnosed a real old/new rounding difference: old subsampling uses +127
while consuming input rows and +128 for the final two flushed rows. Modern
BrushCore uses +128 throughout. The compatibility renderer retains the old
mask arithmetic; direct native BrushCore interpolation is shared, with the old
drawable-local coordinate arithmetic retained around each continuation.

Reproduce with `migration/tests/capture_smudge_probe.py`, the prepared legacy
environment, and the shared build lock. This is a diagnostic native runtime
probe, not interactive GUI or instrumented-old-runtime evidence.
