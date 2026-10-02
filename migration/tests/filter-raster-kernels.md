# Exact spill-backed Edge/Gaussian kernels

The independent worker can run legacy Edge and Gaussian filters through
`FilterRaster`, without a whole-drawable RAM snapshot. The existing vector
entry points and their strong publication guarantee remain available.

## Arithmetic and ordering

- Edge and Gaussian arithmetic lives in shared private headers, extracted from
  the already oracle-tested vector implementations. Neither entry point links
  the vector API to spill storage, so its standalone build commands still work.
- Edge preserves the column-major 3x3 kernel order, all six detectors, all three
  wrap modes, original alpha, and original hidden RGB at fully transparent
  destinations. Hidden neighboring RGB still contributes to edge detection.
- Gaussian preserves the legacy vertical-before-horizontal order, IIR/RLE
  method fallback, fractional radii, premultiply/separate byte rounding after
  each pass, RLE encoded/nonencoded endpoint asymmetry, and final original-RGB
  shadow merge wherever output alpha is zero.
- Gaussian transposes exact RGBA bytes in 1024x1024 tiles, processes the now
  contiguous vertical lines, and transposes back. A horizontal pass follows.
  There are no per-pixel column file reads or color conversions.
- Dimension/offset arithmetic is checked before I/O. Per-line Gaussian
  arithmetic retains defined legacy signed-32-bit limits. Undefined old
  arithmetic is rejected instead of reproduced.

## Ownership, failure, and memory

Input/output and factory-created scratch rasters must have independent backing
storage. Direct object aliases, null scratch, and incorrectly sized scratch are
rejected. The worker and its scratch factory contain no GObject/GEGL callbacks.

Unlike the vector API, a raster output is private workspace: cancellation or
an exception may leave partial output. Only a true return after final flush
permits the adapter to consider publication, and its generation/lifetime checks
must still succeed. I/O/factory exceptions propagate. Scratch is destroyed on
success, cancellation, and failure. Input is never changed.

Explicit algorithm buffer bounds, excluding small objects, allocator/stdio
bookkeeping, and operating-system file cache:

- Edge: 16,408 stack bytes for three 1026-pixel halo strips and one 1024-pixel
  result strip. No dimension-sized or area-sized Edge allocation
- Gaussian transpose: 4,198,400 heap bytes (1024x1024 RGBA tile plus one column)
- IIR pass: 72 times line length bytes (two RGBA byte lines and two four-channel
  double recurrence arrays)
- RLE pass: 16 times line length plus 28 times curve length plus 8 heap bytes
- Final shadow merge: 131,072 heap bytes

These phases do not overlap; Gaussian peak algorithm heap is their maximum.
There is one full-sized scratch disk raster if the vertical axis is enabled,
and none for horizontal-only Gaussian. The input and private result are separate
full-sized rasters supplied by the caller. Edge needs no scratch raster. The
host admission layer must reserve line state plus transport/bookkeeping, and
must handle temporary-storage exhaustion as a failed unpublished generation.

## Tests and evidence

`test-filter-raster-kernels.cpp` compares every captured byte from the genuine
old executable against both vector and real temporary-file execution:

- 76 Edge RGB/RGBA cases
- 104 Gaussian RGB/RGBA cases
- 77 native Gray/Gray-alpha cases, using channel replication without conversion

It additionally covers 126 generated Edge halo/border cases, 86 generated
Gaussian fractional/one-axis/transpose-boundary cases, deterministic cancellation
and exception injection at every raster/factory call boundary (234 cases),
invalid dimensions/options/storage, alias rejection, and synthetic sparse
addresses beyond 4 GiB. Width/height one and boundaries immediately below, at,
and above the 1024-pixel tile side are included.

The 4099x1281 bounded-workspace case uses only chunked file I/O, with no raster
vector. Each raster is 21,003,276 bytes. It verifies one Gaussian scratch raster,
correct output, cleanup, Edge I/O bounded to 4104 bytes per call, and strip-scale
rather than pixel-scale I/O counts. The normal evidence report records an
isolated process peak RSS when the platform supports `wait4`; RSS includes
libraries, stack, allocator and stdio in addition to algorithm buffers.

`run_filter_raster_kernel_tests.py` builds and runs the new test plus all three
original standalone vector programs. `filter-raster-kernels.json` and
`filter-raster-kernel-sanitizers.json` record commands, source/corpus hashes,
outputs, and bounds. The sanitizer run covers AddressSanitizer,
UndefinedBehaviorSanitizer, and float-cast-overflow. LeakSanitizer cannot operate
under this executor's ptrace sandbox; that limit is recorded rather than
reported as a passing leak check. System libraries are not instrumented.

Reproduce after sourcing `tools/linux-debian13-env.sh` with the configured
dependency directory, and serialize with the shared lock:

```sh
flock /tmp/gimp-painter-build.lock python3 migration/tests/run_filter_raster_kernel_tests.py /tmp/gimp-raster-kernel-evidence --report migration/tests/filter-raster-kernels.json
flock /tmp/gimp-painter-build.lock python3 migration/tests/run_filter_raster_kernel_tests.py /tmp/gimp-raster-kernel-evidence --report migration/tests/filter-raster-kernel-sanitizers.json --sanitize
python3 migration/tests/sanitize_filter_evidence.py migration/tests/filter-raster-kernels.json migration/tests/filter-raster-kernel-sanitizers.json
```

This evidence is limited to independent arithmetic/storage workers. Native
FilterLayer transport, admission, generation publication, lifecycle, Undo, and
UI latency require their separate adapter/scheduler integration gates.
