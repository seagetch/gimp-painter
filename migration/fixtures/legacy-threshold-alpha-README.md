# Genuine pinned legacy Threshold Alpha observations

Both genuine captures completed with exit 0 and no evidence-validation errors.
The 256-case base and 2-case large corpora are sealed. Current-port native/XCF
acceptance is a separate gate; capture success alone does not certify it.

## Bounded original-runtime evidence

The harness invokes the unchanged original `threshold-alpha` executable from
GIMP Painter commit `afa43fae3e920210146abed514f136fd49f671b5`. Its input images
are created from deterministic arithmetic patterns. It does not open user
artwork. Observation-only hooks record actual FilterLayer starts/completions,
PDB progress requests, shadow merge boundaries and buffers; the original
operation, merge and writer are not replaced.

The genuine small capture contains 256 cases: 84 synchronous PDB invocations,
172 genuine live FilterLayer scenes, and 28 original-writer saves. It covers
INT32_MIN, -1, 0, 1, 127, 254, 255, 256 and INT32_MAX; YA/RGBA direct inputs;
Y/RGB no-alpha rejection; promoted Y/YA/RGB/RGBA live lower inputs; valid tiny
geometry; negative/asymmetric offsets; empty, hard, soft, outside and partial
selection; all GrayA/RGBA component masks; and target own alpha lock.

Forty-eight live scenes mutate context at actual progress requests. Twenty-four
are one-tile pre-merge observations at progress 1.0. Twenty-four are genuinely
mid-work observations at progress 0.25, including local `[16,112)²` and tiny
`[63,65)²` tile-crossing rectangles. These labels are different deliberately:
the old iterator emits progress only for portions 0, 16, 32, … and has no
unconditional final 1.0 update.

Twenty-eight changed-lower-source reruns accompany writer cases. Fifty-two
separate unchanged-byte lower notifications exercise current-context reruns,
including twelve idle context-only edits with no unsolicited job/cache change.
The separately captured large matrix has two 1031×1021 live lower inputs, Y
and RGB, with late selection changes and unchanged-byte notification reruns.
Recorded start/end/merge counts prove these executions. The small adapter gate
compared 334 invocations with genuine bytes before large capture: 328 raw
shadows/final merges and 6 outside no-merge cases, through both vector and
raster adapters; the two observed no-alpha PDB rejections were kept separate.

## Sealing and diagnostics

Capture outputs include the archived harness actually compiled, capture
script, validator, generated case header, observation helper/patch, commands,
compressed full build/runtime logs, source and binary pre/post hashes, actual
buffers, execution tables, and the original-writer files/capsules. The script
fails if any capture source changes during execution and refuses nonempty
output/work destinations. Existing legacy startup diagnostics remain in the
report; no warning-free runtime is claimed.

All 28 actual original-writer files independently confirm 76-byte framing: the exact NUL-terminated
`plug-in-threshold-alpha` name, then tags/sizes `(2,4),(1,0),(10,0),(2,4),(0,0)`.
Strict native decoding was enabled only after this measured shape was
confirmed. Signed threshold bits are retained. Manufactured malformed native
test variants are not genuine old-writer evidence.

Sealed archives and adjacent `.tar.manifest.json` receipts:

- `legacy-threshold-alpha-context.tar.xz`: 303,256 bytes, 3,356 regular files,
  21,909,230 uncompressed payload bytes, 8 MiB LZMA2 dictionary.
- `legacy-threshold-alpha-large.tar.xz`: 311,072 bytes, 58 regular files,
  90,803,851 uncompressed payload bytes, 32 MiB LZMA2 dictionary.

The verifier pins archive/manifest/report hashes, checks bounded regular files
and every observation, and creates a consumer-only combined 258-row capture
table. It verifies 174 live scenes, 256 live outputs, and all 28 actual writers.
Run `python3 -B tools/check_threshold_alpha_evidence.py --extract EMPTY_DIR`.
The registered native launcher requires this evidence and does not silently
skip a missing corpus. Original corpus receipts remain unchanged.

Both captures retain the same two startup `g_value_set_object` GObject
criticals and missing fresh-profile tag-cache diagnostic as earlier sealed
legacy captures. No additional case-time warning/critical/crash/timeout was
observed. This is explicit inherited-runtime evidence, not a warning-free claim.

## Synthetic-data provenance audit

All images and all 28 XCF documents were created fresh by the inspected
harness. There is no input artwork-loading path. Independent deterministic
replay matched every generated source and image mask byte-for-byte:

- Base: 336 source buffers (2,206,638 bytes) and 776 image masks
  (2,235,700 bytes); 328 local masks (871,500 bytes) matched their image masks
  and layer offsets. The remaining 1,840 native buffers (14,473,024 bytes) are
  recorded intermediate inputs/shadows/outputs. The 28 76-byte raw capsules
  are exactly the bytes extracted from the corresponding original-writer XCFs.
- Large: 4 source buffers (8,421,208 bytes), 8 image masks (8,553,048 bytes),
  4 local masks (4,210,604 bytes), and 22 recorded native buffers
  (69,474,966 bytes). No XCF documents are present in this corpus.

All 40 text members across both corpora, including decompressed build/runtime
logs, were inspected. They contain capture code, synthetic case metadata,
hashes, generated workspace paths, compiler commands and legacy diagnostics.
No credentials, private user content, or user artwork were found. The recorded
native outputs are attributed to these synthetic inputs; this privacy audit
does not claim independent regeneration of every old algorithm output.

## Consumer schema

The materialized fixture root is supplied as
`GIMP_PAINTER_THRESHOLD_ALPHA_FIXTURES`. Existing Sharpen fixtures remain
separately available. The 30 `captures.tsv` fields are:

id, live, source_channels, output_channels, width, height, image_width,
image_height, offset_x, offset_y, threshold, start_selection, final_selection,
start_components, final_components, start_lock, final_lock, rerun, writer,
starts, ends, merges, status, mutated, phase, idle_selection, idle_components,
idle_lock, idle_starts, idle_ends.

- `phase`: 0 none; 1 actual progress=1 pre-merge; 2 actual 0<progress<1 mid-work.
- `rerun`: 0 none; 1 changed lower bytes; 2 unchanged-byte lower notification.
- Negative final/idle fields mean unchanged.
- Selection kinds: 0 empty/no selection; 1 centered hard; 2 soft; 3 outside;
  4 expanded hard; 5 local `[16,112)²`; 6 local `[63,65)²`; 7 local 1×1 at (1,1).
- PDB status is the observed return. Async live status -1 means unavailable;
  actual start/end/merge evidence establishes its execution separately.
- `rois.tsv`: id, run, selected, x1, y1, x2, y2. PDB uses run 1.
- `events.tsv`: id, event, starts, ends, progress, merge_count.
- `merges.tsv`: merge id, width, height, channels, effective active bits,
  selection-empty, observed own lock. Effective bits include own-lock alpha
  suppression.

Raw buffers are interleaved native bytes. `source` is the lower layer, while
`input`/`start1` are its actual alpha-bearing owned input. `start-mask`,
`final-mask`, `idle-mask` and `startN-mask` use image coordinates;
`mergeN-mask` is local effective coverage. Each `mergeN-shadow` is captured
before the actual old merge, including outside-start-ROI zero bytes. `source2`,
`start2` and `output2` record the distinct rerun. `idle-output` demonstrates
that a context-only edit did not change the completed cache.

## Reproduce

Source the restored pinned runtime's generated `build/env.sh`, then invoke
`python3 -B tools/capture_legacy_threshold_alpha.py --capture --legacy-source
SOURCE --legacy-prefix PREFIX --work NEW_WORK --output NEW_OUTPUT`. The script
acquires `/workspace/shared/gimp-painter-build.lock`. Run the small matrix first;
only after it agrees with the adapter run the separate `--large` matrix. Fresh
observations require their own review and seal, never substitution into an
existing corpus. Without `--capture`, only source checks and case/header
preparation occur; neither the old runtime nor the port is executed.
