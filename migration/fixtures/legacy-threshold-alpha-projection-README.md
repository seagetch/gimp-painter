# Genuine Threshold Alpha projection-history supplement

The separate 16-scene capture completed with exit 0 and 48 observed
copy/start/merge/completion runs against pinned original GIMP Painter commit
`afa43fae3e920210146abed514f136fd49f671b5`. Native-port acceptance is a separate
gate. The original base/large archives and their 174-scene/256-output gate are
unchanged.

## Scope and measured behavior

The matrix is YA/RGBA × top-level/ordinary group × initial positive/negative
overflow × fresh zero/synthetic seeded cache. Every image is 13×11; the lower
source and filter are 17×13, initially at either (3,2) or (-4,-3). Three runs
use thresholds -1, 127, 127. Before run 3 only the filter moves to the opposite
offset; the lower source remains stationary and byte-identical throughout.

Eight seeded controls use the actual `gimp_filter_layer_mark_as_loaded()` API.
Their initial settling produces zero jobs and retains every seed byte. This
tests the loaded-cache initialization path, not an actual XCF file reopen.

One additional observation-only hook records existing drawable pixels and the
actual immediate-parent projection immediately before the original unchanged
`copy_region_nocow()` call. Existing start and merge hooks record its result.
All 48 inputs equal the previous cache with exactly the observed projection
intersection overwritten. All 10,608 output pixels match the signed threshold
operation, including preserved color channels. In top-level run 2, 888 pixels
outside the projection retain alpha 255 from run 1. After movement, run 3
retains 380 alpha-zero and 508 alpha-255 pixels outside its new intersection.

Ordinary group bounds are observed automatic child-union bounds, not forced
clipping. They include the filter and stationary source, expanding after
movement; 2,664 group-run pixels lie outside the image canvas while remaining
inside the immediate parent projection. Image-only clipping would be wrong.

## Evidence and diagnostics

`legacy-threshold-alpha-projection.tar.xz` contains 572 regular files with
693,847 payload bytes, compressed to 44,948 bytes using an 8 MiB LZMA2
dictionary. Its adjacent `.tar.manifest.json` seals every member. The checker
pins the archive, manifest and successful report, verifies original source
and binary pre/post seals, and repeats all input-copy/output proofs.

The archive retains the exact compiled harness, capture script, capture-time
validator, generated header, observation helper and full hook diff, commands,
compressed build/runtime logs, tables and actual buffers. Original source,
operation and copy call are unchanged. Runtime diagnostics are the same two
startup `g_value_set_object` criticals and missing fresh-profile tag cache
message as the earlier captures. Legacy compiler warnings remain in the build
log. No warning-free legacy runtime is claimed.

The first attempt's original report and files are preserved separately. Its
harness completed all 48 runs, but validation incorrectly required alpha 255
outside the projection in both runs 2 and 3. Run 3 can legitimately expose
previously thresholded alpha-zero pixels. The corrected validator limits that
specific alpha-255 check to run 2 while retaining exact previous-cache and
copy-intersection checks for every run. A fresh capture produced this archive;
the failed report was not rewritten. Its report SHA256 is
`8d2bed0fe91e799e3ece8cb5196b865715c86ae96a3892231c6321157a46a0d8`.

## Synthetic-data provenance audit

All images are created fresh by the inspected harness. There is no input
artwork-loading path and no XCF file in this supplement. Independent replay
matches all 64 source buffers (42,432 bytes), 24 seed/loaded-cache buffers
(15,912 bytes), 64 empty image masks (9,152 bytes), and 48 full local masks
(10,608 bytes). The remaining 352 native buffers (232,824 bytes) are actual
intermediate/cache/projection/shadow/output observations, checked against the
recorded copy and threshold operations.

The 20 text members, including decompressed build and runtime logs, were
inspected and scanned for credential patterns. They contain synthetic case
metadata, harness code, generated workspace paths, hashes, compiler commands
and legacy diagnostics. No credentials, private user content or artwork were
found.

## Consumer schema and reproduction

Run `python3 -B tools/check_threshold_projection_evidence.py --extract
EMPTY_DIR`. The native launcher supplies that directory through
`GIMP_PAINTER_THRESHOLD_PROJECTION_FIXTURES`; its separate consumer requires
exactly 16 scenes and 48 outputs.

`captures.tsv` has 14 fields: id, source_channels, grouped, seeded, initial_x,
initial_y, final_x, final_y, starts, ends, merges, copies,
initial_loaded_starts, initial_loaded_ends. Fresh controls use -1 for the last
two fields; seeded controls use zero. Every final execution count is three.

`copies.tsv` has 20 fields: id, run, channels, width, height, filter_x,
filter_y, parent_x, parent_y, parent_width, parent_height, source_x, source_y,
dest_x, dest_y, copy_width, copy_height, starts, ends, merges. Counts at this
pre-copy observation equal run minus one.

Raw labels include `seed`, `source`, `input`, `start-mask`, optional
`loaded-cache`, and for each N=1,2,3: `beforeN`, `projectionN`, `startN`,
`startN-mask`, `mergeN-input`, `mergeN-shadow`, `mergeN-mask`, `mergeN-output`,
`outputN`, `sourceN`. Native buffers use interleaved YA/RGBA bytes;
`projectionN` uses its recorded parent dimensions. Image masks are 13×11;
local masks and owner buffers are 17×13. Other execution-table fields follow
the base Threshold README.

To author new observations, source the original replay's `build/env.sh` and
run `tools/capture_legacy_threshold_projection.py --capture --legacy-source
SOURCE --legacy-prefix PREFIX --work NEW_WORK --output NEW_OUTPUT`. The script
requires fresh destinations and acquires the shared build lock. New captures
need their own validation and seal; they must not replace this evidence.
