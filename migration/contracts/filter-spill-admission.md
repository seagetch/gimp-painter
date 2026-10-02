# Automatic FilterLayer temporary-space admission

This extension separates process memory, worker count, and temporary-file extent.
The native `GimpGeglConfig` owns one typed common `BindingStore` slot and one pool.
Its serialized `painter-filter-spill-size` property defaults to 8 GiB; zero disables
file-backed jobs, while small in-memory jobs and completed saved caches remain usable.
The existing resource preferences page exposes this limit. No native instance/class
layout changes and no second private implementation storage are introduced.

## Accounting and scheduling

The pool admits at most two workers, at most 1 GiB declared working memory, and
at most the configured sum of live declared spill bytes. Requests are FIFO across
both byte dimensions. Edge and identity requests reserve input plus result
(8 bytes/pixel); vertical Gaussian processing also reserves its transposed scratch
(12 bytes/pixel). All arithmetic is checked with 64-bit spill accounting, including
requests larger than 4 GiB. The file wrapper charges logical extents, not sparse
physical blocks. A scratch extent is released only after its file closes, allowing
sequential scratch reuse but rejecting a processor that exceeds its declaration.

The lease outlives all worker-only files, buffers, and callback storage, including
a cancelled job whose UI owner has gone away. Cancellation never joins the worker.
Lowering the limit does not pretend live reservations disappeared: no further work
is admitted until the live totals fit, and oversized queued requests fail explicitly.
Closing configuration prevents new admission; worker leases still finish cleanup.
Different native configurations have independent pools. This is a per-application
policy, not coordination among unrelated GIMP processes.

A budget change invalidates a pending/failed job so it can retry automatically;
it does not modify the generation or pixels of a clean/restored cache. Selecting
its native pool is likewise not a content edit. An initial unconditional pool-switch
invalidation violated saved generation restoration; the retained failed run and
regression make this correction explicit.

## Filesystem contract

The worker checks available filesystem space before creating its temporary rasters.
POSIX uses `statvfs` available blocks, with overflow-safe multiplication. The Windows
branch uses the quota-aware 64-bit `GetDiskFreeSpaceExW` API and UTF-8 conversion;
actual Windows execution remains an unpassed release gate. Capacity checks and
all file creation/I/O occur on the worker, not the main-context dispatcher.

This check is advisory, not an operating-system reservation: another process may
consume space after the query, and filesystem metadata/allocation-unit overhead is
not included in logical extent accounting. Real write, flush, read, and creation
failures still fail the job and preserve the last completed cache. Neither a saved
spill budget nor a successful capacity query proves whole-application memory/disk
bounds. GEGL caches, graph construction, callback costs, final unrefs and other
applications remain outside this pool. The existing measured responsiveness gates
are not relabeled as complete by this feature.

## Acceptance evidence

The checkpoint records native property persistence and pool isolation/closure,
zero-budget refusal without cache mutation, automatic retry after raising the
budget, continued small-image work, and clean-generation preservation. Independent
tests cover FIFO/limit changes, worker release, per-file quota overflow and scratch
reuse, missing-directory/advisory capacity failure, and greater-than-4-GiB offsets.
Normal and focused sanitizer results must be recorded separately; only reported
source lists are instrumented, and RTTI-only compatibility units are not counted as
sanitizer coverage. Cross-platform, physical disk exhaustion and arbitrary third-party
processor behavior are not certified by the Linux fixtures.

The independent sanitizer run passes13 admission,7 raster,11 spool and35 scheduler
cases. The actual native FilterLayer sanitizer run passes80 cases with19
instrumented sources and26 RTTI-only units. A narrow refresh rebuilt the corrected
configuration tests and the concurrently changed Tile RTTI unit, with predecessor
report and exact tested sources retained. The first property test incorrectly used
the non-serializable base configuration; its replacement initially omitted the
required Gimp owner. Both test-setup failures remain recorded, not hidden passes.

The first complete normal attempt reached58/80 before the preexisting60-second
suite timeout. Both new admission cases passed; real8193-square execution measured
16.99 seconds,151.8 ms maximum heartbeat and179.7 ms final close on that run.
The focused sanitizer completed all80, with14.07 seconds,56.5 ms maximum heartbeat
and215.5 ms close. The suite timeout is now180 seconds because it includes the
full large-image and old Gaussian matrix; per-job convergence deadlines and
responsiveness observations are unchanged. These measurements do not satisfy
an unconditional short-latency requirement.
