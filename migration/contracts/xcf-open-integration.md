# Legacy XCF application Open integration

Status: implementation/verification in progress, 2026-10-02. This is a reader
checkpoint, not completion of migration sections 10–12 or edit/save/reopen.
The source-level wire contract is [xcf-compatibility.md](xcf-compatibility.md).

## Application path and dialect decisions

`file_open_image()` invokes the registered `gimp-xcf-load` procedure and
`xcf_load_stream()`. `painter-xcf-load.cpp` creates an immutable reusable source,
runs the pure candidate probe, and supplies an explicit legacy/standard context
to the existing pixel, image metadata, channel, mask and hierarchy loader.
Legacy v4 does not consume a precision word. Only legacy **layer** tags32/33
construct independent FilterLayer/CloneLayer instances. Standard32/33 keep their
position-lock/float-opacity meaning. Raw legacy mode values use the explicit
`gimp_painter_layer_mode_from_legacy()` API; they are never cast to the colliding
modern values. Exact mode composition is verified separately.

Both typed and ordinary properties are processed in encounter order, including
historical layer replacement semantics. Custom replacement transfers name,
size, offsets, visibility, opacity, mode and pixel format. Other records remain
available in the immutable original even where the historical replacement did
not retain them as live values. Ordinary attributes occurring later are applied
to the new object. All image layers/hierarchy/masks are built before legacy
source-name resolution. Source binding occurs with Undo frozen. Unresolved names
remain pending and their original names remain independently retained.

The metadata probe is not a complete old parser. In particular, current GIMP
writes standard tags32/33 even in v000/v003. When both metadata candidates pass,
the application additionally validates actual legacy nested-extension consumption
and the subsequent layer property sequence and hierarchy/mask offsets. It does
not infer invalidity from the outer declared size: the historical reader ignores
that size for nested records. Bounded decoding may read through a short declared
payload to the actual END. A remaining limit is inconclusive, never negative
evidence. If both interpretations remain possible, normal Open reports the
choice instead of silently selecting either.

Two recovery procedures are registered in the actual Open file-type list:

- `gimp-xcf-load-standard`: GIMP XCF image (standard recovery)
- `gimp-xcf-load-painter`: GIMP Painter XCF image (legacy recovery)

They have no suffix, MIME or magic auto-match, so they cannot hijack detection.
Each explicit choice starts an independent construction attempt using unchanged
bytes. They preserve access to standard salvage and malformed old recovery even
when the structural probe cannot finish. No input file is overwritten. A fully
validated source-header version above4 excludes the unaudited old dialect and
continues to the existing standard recovery route.

## Immutable source, ownership and bounds

Input is copied in 64KiB chunks to a private temporary file and mapped read-only.
There is no arbitrary whole-file size rejection and no proportional heap copy.
The original stream is closed on success, cancellation and failure. The temporary
name is unlinked immediately where supported; otherwise it is deleted after
unmapping and closing. A shared GBytes owns the backing until the last image or
extension reference is released. Nonseekable input streams also work because
construction operates on the snapshot.

`xcf_painter_ref_original(image)` returns the exact complete original byte stream.
`xcf_painter_original_offset(object, ...)` identifies original layer, channel or
mask records. This preserves image-level and object-level unknown records,
ordering, duplicates, raw names and opaque payloads. Custom objects additionally
retain their exact consumed extension payload, and Clone retains the normalized
original lookup name independently of the subsequently bound source. Filter's
own definition API stores the raw payload. These are in-memory source/definition
ownership guarantees; a persistent writer and duplication of opaque provenance
are separate unfinished gates.

Seeking validates the target against the source size. Unknown property skipping
checks bounds and seeks rather than allocating the declared size. Short reads
zero the unread portion so parser scalars cannot contain uninitialized bytes.
Cancellation propagates through reads and seeks, and prevents returning a partial
image as success. Temporary selection/link/item-set transaction lists are cleaned
up. This does not replace all upstream tile/recursive graph corruption auditing.

## Filter recovery

The genuine `filter-edge.xcf` was saved by the old runtime after completion, but
the old reader crashes. The new reader creates a real independent FilterLayer,
restores the exact89-byte definition and saved pixels, and marks the cache loaded
after final topology restoration. It does not execute the old invalid GValue
pointer-copy path. Supported edge/Gaussian scalar arguments are converted into
separate execution values; omitted image/drawable context slots are safe context
placeholders for the independent executor. No saved image/drawable ID or omitted
array is invented. Other definitions remain raw and can retain their cache even
without a runnable argument conversion.

## Verification and still-open gates

`app/tests/test-painter-xcf-open.c` uses actual `file_open_image()` for genuine
ordinary/Clone/Filter/modern fixtures and registered recovery procedures. It also
covers synthetic dual-valid interpretations, unresolved names, outer-length
recovery, input closure, cancellation, transaction cleanup and a256MiB+1 source.
The existing upstream `app/tests/test-xcf.c` suite must continue to pass through
automatic Open, including modern32/33 written into older-version headers.

Current confirmed ordinary metadata includes hierarchy, offset, dimensions,
stored191/255 opacity, Multiply legacy mode, layer mask, channel, selection and
resolution. Clone fixtures have correct live source identity and exact saved
RGBA191,32,64,255 pixels, and imported-source edits update the clone. Filter cache
pixel12,12 is RGBA0,0,0,255 and opening does not start an initial rerun.

Exact ordinary appearance remains an explicit open gate: the current GIMP3
projection differs from the old exported merged PNG in910/7680 pixels, maximum
channel difference1, with alpha identical. This is not accepted as exact parity;
old direct-projection evidence and stage-specific rounding analysis are pending.
No full writer, ordinary/custom edit-save-reopen, broad malformed tile corpus,
GUI interaction timing or all saved metadata audit is claimed by this checkpoint.

Reader checkpoint verification:

- Normal Meson: upstream XCF4/4 cases pass; application Open11/12 cases pass,
  with the separate exact ordinary projection test explicitly TODO/incomplete
- Targeted ASan+UBSan: same Open cases exit0 with no sanitizer findings, while
  the exact projection gate remains TODO. Instrumented scope is XCF load/probe/
  source/read/seek, Clone/Filter adapters, test harness and BindingStore; remaining
  upstream GIMP/dependencies are uninstrumented. Leak detection is disabled
- New saved custom layers/modes are temporarily refused **before** opening a
  replacement destination. An actual file_save sentinel test verifies unchanged
  preexisting content. Direct stream calls cancel replacement close on refusal
- This temporary guard will be replaced by the approved preservation writer;
  it is protection during the reader checkpoint, not completion of the request
- The additional old direct-projection capture is byte-identical to the sealed
  ordinary PNG, confirming the910-pixel rounding difference is genuine
- Reproduction: build app/tests/painter-xcf-open and app/tests/xcf, then run
  `meson test -C build-debian13 painter-xcf-open xcf --no-rebuild --logbase painter-xcf-open-integration`;
  sanitizer runner is `migration/tests/run_painter_xcf_open_sanitizers.py`
- Evidence: `migration/tests/painter-xcf-open-{meson,testlog}.txt` and
  `migration/tests/painter-xcf-open-sanitizers.json`
