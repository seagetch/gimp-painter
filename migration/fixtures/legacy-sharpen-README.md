# Genuine pinned legacy Sharpen observations

These two sealed corpora were produced by GIMP Painter commit
`afa43fae3e920210146abed514f136fd49f671b5`, using its unchanged `sharpen`
plug-in, shadow merge implementation, and XCF writer. They are input/output
observations, not pixels calculated by the new Sharpen implementation.

- `legacy-sharpen-context.tar.xz`: 186 cases, including 114 synchronous PDB
  calls across Y/YA/RGB/RGBA and 72 real FilterLayer cases. Twenty FilterLayer
  scenes were saved by the original XCF writer at percent 10, 0, -100, 99, and
  INT32_MAX. Native no-alpha lower inputs become YA/RGBA in the original
  FilterLayer; the manifest records both source and observed output layouts.
- `legacy-sharpen-large.tar.xz`: two 1031×1021 FilterLayer scenes, YA and RGBA,
  beginning with a hard selection and changing it at the original plug-in's
  final progress call to empty or soft selection. Each then changes the real
  lower input and executes again. This is the genuine large-input evidence for
  modern spill comparisons; capture alone does not prove modern parity.

The base capture includes ordinary and noncanonical defined percentages,
width-two and border cases, positive/negative offsets, hard/soft/outside
selection, component gating, own alpha lock, hidden color with alpha zero, late
selection/component/lock changes, and changed-lower-input reruns. Width one,
height below three, and percentages below `100 - INT32_MAX` were deliberately
not executed because the pinned implementation has undefined behavior there.

## Provenance and diagnostics

Every archive includes the exact capture script, harness, generated case
header, observation helper and hook patch, original source/binary hashes,
build commands, full compressed build/runtime logs, native buffers, and a
capture report. Source and binary hashes were checked again after execution.
The original implementation sources were not modified by the capture. Hooks
copy buffers and apply specified owner-context mutations before the actual
legacy shadow merge.

The asynchronous FilterLayer path does not expose a PDB return value to this
harness. Its TSV status is therefore `-1` (unavailable), with observed
start/end/merge counts recorded separately. Synchronous PDB status is the
actual return. The base corpus's 2,126 raw artifacts and all 20 XCF files were
also byte-identical to an earlier capture; its reporting flaw (an initialized,
unobserved live status) is explicitly documented in `repeat-receipt.json`.

All captures emit two startup `g_value_set_object` GObject criticals followed
by the missing fresh-profile tag-cache message, before case execution. The
same text and order occur in the previously sealed context, expansion, and
convolution corpora. The full diagnostics remain in the reports and logs.
This is inherited runtime diagnostic evidence, not a claim of warning-free
startup or a reason to hide diagnostics. No additional case-time warning,
critical, crash, or timeout was observed.

Actual writer records for all twenty files have the exact literal procedure
name including its NUL and a 68-byte payload: INT32 run mode (tag 2, size 4),
omitted unsupported image (tag 1, size 0), omitted drawable (tag 10, size 0),
INT32 percent (tag 2, size 4), and END (tag 0, size 0). Original signed percent
bits and the complete capsule are retained. Manufactured malformed variants
exist only in native tests and are not labeled old-writer evidence.

## Verify and use

Run `python3 -B tools/check_sharpen_evidence.py --extract EMPTY_DIRECTORY`.
The verifier pins archive, manifest, and report hashes, validates every regular
file and bounded extent, checks complete case counts and source consistency,
and creates a consumer-only combined `captures.tsv` with 188 rows. Original
archive receipts remain unchanged. The native test launcher supplies this
directory through `GIMP_PAINTER_SHARPEN_FIXTURES`; the registered FilterLayer
and XCF Open tests do not silently skip missing evidence.

The 24 TSV fields, in order, are: id, live, source_channels, output_channels,
width, height, image_width, image_height, offset_x, offset_y, percent,
start_selection, final_selection, start_components, final_components,
start_lock, final_lock, rerun, writer, starts, ends, merges, status, mutated.
Negative final-context fields mean unchanged. Selection kinds are 0 (none),
1 (center hard), 2 (soft), 3 (wholly outside), and 4 (expanded hard).

Raw buffers are exact interleaved native bytes. `source` is the lower layer;
`input`/`start1` are the actual first input drawable. `start-mask` and
`final-mask` are whole-image selection bytes (empty means no selection), while
`mergeN-mask` is drawable-local effective coverage. `mergeN-shadow` is the
observed raw shadow before merge. `source2`, `start2`, and `output2` describe
changed-lower-input reruns.

## Reconstruct

Restore the pinned build with `tools/build_legacy_reference.sh` and the locked
legacy dependency manifest. Source its generated `env.sh`, then run
`tools/capture_legacy_sharpen.py --capture --legacy-source SOURCE
--legacy-prefix PREFIX --work NEW_WORK --output NEW_OUTPUT`. Add `--large`
for the separately captured large cases. The script acquires the shared build
lock, uses a fresh isolated profile containing only the original Sharpen
plug-in, and refuses to overwrite a sealed capture report. Build/runtime
provenance is specific to each capture; a fresh rebuild can legitimately
produce different executable hashes. New observations must be reviewed and
sealed separately, never substituted silently for these fixtures.

Archives have deterministic member names, mode, owner fields, and timestamps.
The base uses an 8 MiB LZMA2 dictionary; the large corpus uses 32 MiB. The large
archive is 477,872 bytes versus 13,429,721 bytes for its retained gzip copy,
with identical 94,981,221 decompressed payload bytes.
