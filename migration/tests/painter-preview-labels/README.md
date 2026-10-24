# 30.017/brush-preview-generator — original ImageMagick pipeline

`tools/label_painter_brush_previews.py [directory]` retains the original immediate visible `*.png` selection, first `_prev.png` substitution, underscore-to-hyphen captions, fixed font, South placement, caption size/background, composite, both borders, resize128x128, unsharp, flatten and alpha-removal order. It overwrites the same selected basenames. No GIMP batch/runtime or held native preview hook is used. Non-preview PNG basenames are also labeled, as in the original.

Safe deliberate improvements: filenames are separate argv values (spaces/Unicode/leading dash work); no-match is a no-op; non-regular/symlink targets reject before writes; per-file temporary output preserves the original on converter failure or empty successful output, preserves permission bits, and is removed on failure. Multiple-file conversion remains per-file, not an all-files transaction. Do not run on immutable bundled assets without an explicit asset-lock update: use a working copy.

**Seven groups PASS**, with exact source-before/after hashes. The full locked corpus is copied into temporary directories: all178 PNG outputs are independently compared as decoded RGBA bytes to the unchanged pinned old shell script on this same ImageMagick installation; all are128x128. All363 manifest paths remain and all185 non-preview asset bytes stay identical. Separate literal-pipeline tests cover spaced Unicode and leading-dash names. Original bundled assets were not modified: the independent sorted-manifest generator confirms363 exact bytes afterward. No old GIMP application was executed.

ImageMagick7.1.1-43 Q16 warns that `fixed` is unavailable and falls back in BOTH old and new runs. The warning is retained; this proves same-install pipeline equality, **not historical ImageMagick/font parity or availability of the original fixed-font raster**. No replacement font is silently imposed by the new utility. `old-run.log` additionally records an initial unsourced-environment font-cache warning, not final-run status. The first corpus test used an overly strict shell-word audit excluding harmless `%`/parentheses present in two pinned filenames; that diagnostic and the corrected final result are retained. The preceding successful run predates the empty-output guard, and is not relabeled final-source evidence.

Original criterion and both assigned source/asset duties stay unchanged; this child is DOING under original30.016/01.015 prerequisites. This is a developer tool in the source tree, not an application UI or installed end-user command. No sanitizer or cross-platform proof is claimed. Publication is paused.

Reproduce with the configured ImageMagick environment:

    python3 -B migration/tests/test_painter_preview_labels.py -v
    python3 -B tools/generate_painter_brush_manifest.py
