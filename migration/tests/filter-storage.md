# Independent filter spill storage and transport

These components are ready for scheduler integration, not evidence that the live
FilterLayer large-image gate is complete. Their tests do not run a GEGL graph.

`FilterRaster` is worker-owned independent byte storage. `TemporaryFilterRaster`
uses exclusive mode-0600 files, anonymous after opening on POSIX and marked
for deletion on close in its Windows branch. Access offsets are checked in
64 bits and chunks in `size_t`. Wrong-thread accesses and invalid extents fail.
Reads diagnose unexpected EOF, and explicit flush reports buffered-write errors
before a completed result can be accepted. All file creation/access/close is
worker-side; it is never an owner-thread mmap or file write. The Linux path is
executed here; the Windows branch still needs native platform validation.

The transpose uses at most 4,198,400 heap bytes, rearranging exact RGBA bytes
without conversion. It supports odd/asymmetric tile boundaries and propagates
cancellation or I/O exceptions, leaving only a private partial output.

`FilterSpool` gives each direction two fixed queue slots with exclusive byte
chunk ownership, at most 32,768 RGBA pixels per chunk. Producer/consumer transfer
uses atomic pointers, never an owner-thread file operation or mutex wait. Each
side may additionally hold its current bounded chunk. Only the worker waits on
its condition variable. The owner is notified of phases through atomics and
polls through its paced scheduler; there are no GObject or UI callbacks.

The worker constructs its input/result files, drains a fully sealed input,
executes the independent kernel, then transports completed result bytes. A true
kernel return alone is insufficient: any later result-read failure prevents a
successful transport completion even if earlier chunks were staged. All files
are destroyed before completion is published. Close requests cancellation and
returns without joining; the detached independent state retains its admission
lease until every owned queue/storage/closure is destroyed. A processor that
ignores cancellation is not killed in-process and continues holding that lease.

## Evidence and limits

`run_filter_storage_tests.py` reproduces six storage/transpose cases and nine
transport cases in normal or ASan/UBSan modes. Reports include exact unit/header
hashes, commands, full output and a check for source changes during compilation.
GLib/system libraries are uninstrumented; LeakSanitizer is disabled. Tests cover
native bytes, cleanup, sparse offsets beyond 4 GiB, bounds/EOF, affinity, odd
transposes, input and full-output cancellation, nonwaiting close/lease retention,
worker exceptions, invalid input, and failure after an already queued result.

These tests use `g_dir_make_tmp()`. On this executor `/tmp` is tmpfs. Bounded
process heap/RSS does not mean bounded total physical memory if raster files are
placed on a RAM-backed filesystem. The live adapter must use an appropriate
configured swap directory, and report storage failures without publishing a
partial generation. OS cache/backing, GEGL-managed cache/staging, filesystem
capacity and pathological I/O latency are separate resource limits; these
component results do not certify application RSS or responsiveness.
