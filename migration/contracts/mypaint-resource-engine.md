# Extended painter MyPaint resource and evaluator

Pinned source: seagetch/gimp-painter `afa43fae3e920210146abed514f136fd49f671b5`.
This increment is a resource/model and evaluator port. It does **not** establish
GeglBuffer Surface pixel parity, installed factory/tool/editor integration,
normal-brush paper texture, independent Smudge, or tablet/GUI equivalence.

## Resource contract

`app/paint/painter-mypaint/Resource` owns a deep-copyable JSON tree. It reads the
177 exact original assets restored under `data/painter-mypaint-brushes` (including
previews and surviving asset license/readme files). `assets.json` records hashes.
It is not a wrapper around standard GIMP 3 MyPaint or libmypaint.

- The 176 JSON-v3 and one line-v2 files load with all original numeric settings,
  curves, metadata and unknown values retained. Semantic accessors use original
  float defaults and conversion, without clamping original values in storage.
- Missing switches remain false, matching the old private constructor, including
  `use_gimp_brushmark`, despite its metadata default being true. Explicit false,
  missing fields, null texts and empty texts remain distinct.
- `opaque_multiply` retains the default pressure curve when that input is absent;
  an explicitly empty pressure curve removes it. This is old reader behavior.
- In v3, an explicitly empty parent name uses the filename stem when a source
  name is supplied. Missing parent-name fields and v2 absence stay empty. This
  distinction was found and fixed by independent actual-old-parser comparison.
- Legacy aliases and their original float transforms are supported. v1 parsing
  fixes the documented semicolon defect, validates point pairs and nondecreasing
  x coordinates, and applies the old y transform. Synthetic v1 cases are tests,
  not historical application fixtures.
- JSON subtrees unknown to this implementation survive saving and edits. Legacy
  line source and raw fields survive in `painter_legacy`. `colorize` (177 assets),
  `snap_to_pixel` (24), and `pressure_gain_log` (20) are explicitly diagnosed as
  ignored by the pinned engine. They are preserved, not silently activated or
  deleted. Unknown line-v1/v2 inputs are mirrored into the canonical input tree as well
  as raw source, so they too are diagnosed and refused rather than ignored.
  New unknown engine fields prevent engine configuration; they can
  still be loaded, inspected and saved. Unsupported curve counts/order are
  retained, diagnosed and refused by the evaluator instead of truncating them.
- `Mapping` value-owns its curves, supports independent copy/move/self-assignment,
  and preserves old interpolation/extrapolation order and duplicate-x behavior.
- Stream save writes to the caller's `GOutputStream`, leaves stream ownership to
  the caller and returns write errors. It never derives another JSON filename.

`GimpPainterMybrush` is a distinct `GimpData` resource with one typed BindingStore
slot and `PainterMybrushRef` owning handle. It supports loader/stream writer,
copying, dirty notification, safe same-icon assignment, explicit closure and
native pixbuf/temp-buffer previews. Icon pixels are owned by GdkPixbuf including
real stride; preview conversion returns a caller-owned GimpTempBuf. Loaded old
PNG sidecars are retained in a namespaced `painter_preview_png` JSON field so
JSON and icon survive a single stream save and a move without an inconsistent
second-file transaction. Old readers ignore this additional metadata. Missing
icons use the normal GIMP icon fallback, not a fake brush stroke preview.

Factory registration, context selection and editor actions remain subsequent
integration; constructing the resource alone does not make it selectable in UI.

## Evaluator contract

The isolated `Legacy::Brush` preserves the pinned evaluator's arithmetic and
state transitions. Mechanical changes are namespace/include isolation, Mapping
value ownership, RAII random-generator ownership, safe initialized color helper
structs, and explicit C++ standard-library `isfinite`. The old GIMP double HSV/HSL
math is isolated with its LGPL notice; standard modern color conversions are not
silently substituted. There are no shared libmypaint symbols.

`Engine` accepts the lossless resource, rejects unsupported fields, owns the old
52-slot evaluator (45 active numeric settings plus old inactive slots), and
exposes all 30 replay states. It forwards stationary pressure/time samples rather
than dropping equal positions. Its Surface interface carries old hardness,
aspect, angle, lock-alpha, texture grain and contrast parameters. Smudge's
previous-valid-sample retention on alpha-zero, premultiplied state updates and
sample/dab ordering are unchanged. Stroke opacity and non-incremental mode remain
separate surface/session properties; this does not yet implement their pixels.

The old core copied mappings without recalculating speed caches and then set
foreground color through `set_base_value` at stroke start (which recalculated
caches). The current pure evaluator comparison exercises the mapping-copy path;
actual tool/session integration must reproduce the full foreground/start order.

## Runtime evidence and exact scope

1. `resource-capture.json` and `resource-values.tsv.gz`: separate harness links
   the real rebuilt old application archives and invokes unchanged
   `gimp_mypaint_brush_load` and private model methods on all 177 assets. All
   81,420 setting/curve/switch/text/name/group records match the port exactly.
   No port decoder participates in the oracle. Loader/model source hashes,
   archive/binary fingerprints, link plan and raw capture are retained.
2. `mypaint-engine-comparison.json` and `engine.trace.gz`: compiled unchanged old
   Brush header + installed old GIMP color library versus the port. All 177 real
   brush configurations plus one explicitly synthetic extended-smudge/texture/
   stationary-pressure case produce identical 5,637,911-byte traces, 27,392 dabs,
   3,172 color samples and 30 states after every input event. Both engines use the
   new decoder to supply settings; parser equivalence is established separately
   by (1). The Surface's sampled colors are deterministic synthetic responses,
   **not** pixels captured from the old drawable renderer.
3. Meson resource tests cover round trips, missing/explicit defaults, unknown
   data, v1 fix, ownership, malformed data and output-stream errors. Registered
   golden tests run without an old checkout. Re-capture scripts require the
   exact documented old build and verify unchanged behavior source first.

4. `gimp-painter-mybrush`: four actual GIMP application-core tests pass. They
   load/copy every bundled resource, use a real initialized GimpContext for the
   preview API, verify rowstride/pixel ownership, rename/save/load JSON and icon
   through memory streams, check malformed-edit atomicity and write errors, dispose repeatedly, and safely complete a copy when old-icon finalization
   releases the caller’s last destination reference. Full `app/gimp-3.0` links after the Meson ordering move.
5. AddressSanitizer/UndefinedBehaviorSanitizer pass the eight resource/model
   cases, including all corpus loads. LeakSanitizer cannot complete here because
   the sandbox uses ptrace; no leak-check success is claimed. This sanitizer run
   covers the standalone resource/evaluator module, not the full GIMP process.

6. `mypaint-native-sanitizers.json`: all four native GIMP cases also pass under
   ASan/UBSan with eleven explicitly instrumented sources: native adapter,
   Resource/Engine, GimpData/Viewable/Object/Resource lifecycle, native test and
   shared BindingStore/boundary helpers. Private thin archives replace all aliases
   of these members, including Meson-flattened `link_whole` copies, without
   replacing production objects. Instrumented C++ consistently enables RTTI for
   UBSan. Other GIMP/dependency code is uninstrumented and LSan is disabled.

No GUI, real pen/tablet, Windows or macOS run is claimed by these tests. Python-
launched golden comparisons are registered only for native builds; normal Meson
resource/metadata executable tests retain Meson's configured cross-runner path.
