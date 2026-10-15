# Original anime/watercolor composite re-edit records

WBS 02.007: 「複合レイヤー作品を fixture 化する / アニメ塗り・水彩の再編集操作記録を保存」.
These are deliberately synthetic 96×80 RGB fixture scenes, not user artwork or
representative hand-painted art. A paper rectangle and an opaque 48×40 paint
rectangle at (18,16) are created through the original application core.
The strengthened watercolor seed also receives a contrasting 22×18 inset at
image coordinates (30,26), using original rectangle selection and edit-fill. The
unchanged pinned `new-anime-layer.json` and `watercolor.json` are loaded by the
original JsonResource implementation and applied by the original
`ILayerPresetApplier`, not reimplemented or substituted.

## Captured sequence and results

Both capture processes exit 0. Full model/argument traces, full projection RGBA
bytes, every layer's pixel bytes, original XCF files, and independent reader logs
are retained. `observations.json` extracts records from the actual logs.

### Anime

- The actual preset creates `New group`, a `mask` CloneLayer in dst-in mode,
  `shadow` in Multiply mode, and the original `paint source`.
- The harness fills the shadow with synthetic RGBA (51,76,153,178), saves the
  initial scene, and loads it through the original core reader.
- It edits that reopened image: fills the source with (51,153,204,255), sets
  shadow opacity to 0.6 with Undo enabled, calls ordinary Undo and Redo, saves,
  and reopens again.
- Initial and reopened projection bytes and all five layer buffers are exactly
  equal. Edited/Redo and final reopened bytes likewise agree. Undo restores the
  recolored projection; Redo restores the changed-opacity projection.
- The actual custom CloneLayer type, reference name, hierarchy, geometry, mode
  and opacity are logged before and after persistence.
- Separately, the installed uninstrumented original app reads both initial and
  edited XCFs, checks root count/group name, prints the pass marker and exits 0.
  This independent check is intentionally narrower than the core pixel/model
  comparison and does not claim independent per-layer type introspection.

### Watercolor

- The actual preset creates `New group`, a `mask` FilterLayer using the original
  `plug-in-edge` procedure, Difference mode and 0.2 opacity, plus paint source.
- Initial and edited files are saved by the real writer. Source recolor, effect
  opacity 0.6, Undo and Redo are recorded in the live original-core image.
- The final recorded initial state has pending=0 and one observed completion;
  after source recolor, pending=0 and two completions. Later opacity/Undo/Redo
  states remain pending=0 with two completions. Observer logging was enabled.
- The source edit recolors only that selected inset, preserving the nonuniform
  boundary. Changing effect opacity 0.2→0.6 changes 480 projection bytes
  (160 pixels). Undo restores the complete recolored projection exactly and
  Redo restores the complete changed-opacity projection exactly.
- The prior flat-seed package is retained losslessly in `historical-flat-seed.tar.gz`, with all member sizes
  and SHA-256s in `historical-flat-seed-members.json`.
  Its opacity edit changed model state but left projection bytes unchanged;
  those limited observations are historical, not this strengthened result.
- Both independent installed-original reopens exit 139, with no pass marker.
  No successful watercolor persistence or post-reopen editing is claimed.
  This is the same unsupported image-argument reader family already preserved
  by the negative edge fixture, now in an actual preset-created group.

This package supplies both requested composite re-edit *records*, including
negative persistence evidence. Whether the bounded collection task is marked
complete must not be mistaken for old-reader compatibility success. Watercolor
successful reopen/post-reopen editing remains unestablished and is not silently
replaced by the positive brightness/contrast fixture.

## What the capture does and does not establish

The application executes ordinary editing, preset, compositor, asynchronous
filter, Undo/Redo and XCF APIs. It is a headless original-core test harness,
not a GUI interaction recording. There are no synthetic GUI screenshots,
hand-encoded XCFs, user artwork, physical tablet events, MyPaint stroke tests,
performance or cross-platform claims. Brush/input fixtures belong to 02.010.

Each snapshot services the main context, flushes and reads the real projection
for two seconds before observing. It records the actual final state, not a
scheduler timeout success assertion. Runtime callbacks are logged by the
inherited capture-only observer, whose patch is included. No scheduling,
CloneLayer, FilterLayer decisions, XCF or preset implementation was repaired.
The preset resource/applier are retained for the short capture-process lifetime;
this is not resource-destruction or leak testing. Startup GLib, Json, pagecurl,
thumbnail-directory, and process-exit Script-Fu warnings remain in the logs.
Thumbnail directory creation was not worked around.

## Provenance and reproduction

Source: `afa43fae3e920210146abed514f136fd49f671b5`, with the existing documented
legacy build adaptations and read-only filter observer. The original installed
binary's SHA-256 stays
`85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1`.
The reference reconstruction is recorded in
`../../baseline/legacy/replay-20261015/report.json`.

This capture used a separate copied configured source/build tree and isolated
profiles/output. Earlier fixture files, original build tree, installed app,
current GIMP3 source/build/prefix and task ledger were not modified.
`source-overlay.patch` records the complete tracked delta on the pinned legacy
source, including inherited build adaptations/observer. Apply it once on the
pin, and add `composite-preset.cpp` as `app/tests/composite-preset.cpp` (a new file,
provided separately). `capture-composite.c` is the final test-xcf.c source.
Compile the C++ bridge using the legacy include/configuration environment and
link `composite-preset.o` in addition to the previously required test link
additions. Exact compiler/link commands are retained in the build logs/scripts.
Rebuild the test after bridge changes; the original Makefile does not list the
extra bridge object as a dependency. Do not reinstall this observer build over
the original reference application.

The record scripts retain actual local paths, not portable turnkey paths.
Adapt only environment/input/output paths when reproducing and record the new
binary/log hashes. `run-record.sh` enables the inherited observer; the separate
independent script uses the preserved installed original binary. The copied
preset bytes are checked against the pin in `preset-provenance.json`.

`manifest.json` seals the package. The eight evidence tests check hashes,
completed capture markers, pixel equalities, live trace stages and negative
versus positive independent exits; they do not rerun GIMP or prove the current
port passes. Licensing follows the repository and retained upstream notices.
