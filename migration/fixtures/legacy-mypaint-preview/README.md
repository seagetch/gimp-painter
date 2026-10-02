# Actual pinned editor preview oracle

`capture-preview.cpp` links the already built, unchanged old application
archives and calls `GimpMypaintBrushPrivate::get_new_preview` directly. It does
not contain a port renderer or a reconstruction of the old preview loop.

Sixteen explicitly synthetic combinations toggle nonincremental, asymmetric
bitmap shape, paper and smudge. Each uses the old 256×256 output with its
257-sample spiral, diagonal red/white initial pixels and original mapping/cache
startup order. Every scenario was called twice independently and was identical.
`preview-values.tsv.gz` holds all sixteen complete RGBA buffers, sixteen repeat
results and one completion record (33 records, 8,389,189 bytes). Compact hashes
are derived from those buffers and consumed by the real GTK editor test.

`capture.json` records pinned feature-source hashes checked against the old Git
commit, all linked application archive hashes, exact compile/link plans and the
executable hash. Full old stdout/stderr/build/link logs are retained compressed.
`capture_mypaint_previews.py` uses the sealed link plan to reproduce the capture
without editing or rebuilding any old production source/archive. The original
full-session oracle was neither modified nor recaptured.

This does not establish all 177 presets, arbitrary ICC or precision modes,
platforms, real tablets, or legacy brush-pipe preview state bugs. Modern pipe
preview isolation is independently tested against controlled native selection.
The old renderer's retained negative-substep warnings remain visible in raw
stdout; they were not suppressed to obtain passing records.
