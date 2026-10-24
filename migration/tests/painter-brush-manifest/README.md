# 30.017/brush-manifest-generator — bounded implementation

Replace recursive directory installation with a reproducible sorted explicit Meson list. All 363 pinned original assets retain exact bytes and destinations: 177 brushes, 178 previews (including root orphan FX_blender_prev.png), two notices and six support lists. The old Ruby generator selected 354 nested .myb/_prev.png entries; an independent pinned Git census matches that subset exactly. The root preview and notices/support are intentionally retained rather than reproducing the old generator omission. No asset or preview pixels are rewritten.

`tools/generate_painter_brush_manifest.py` validates the immutable captured lock identity and every actual asset before generating only meson.build; default mode checks reproducibility. Missing/extra/modified assets, duplicate or unsafe names, symlinks and changed locks are rejected. New assets require an explicit reviewed lock/provenance update, not automatic discovery.

Nine test groups PASS, including actual standalone Meson setup/install of the production generated list and SHA-256 comparison of every installed file; a separate real install checks spaced/Unicode group and filename. Four existing legacy resource-fixture tests PASS. First run exposed renderer reliance on pre-sorted input; retained first-run.log records that failure, fixed by sorting within render. The first shell setup also referenced the wrong cwd before the tests ran; no product write occurred in that failed command.

This is an asset-only Meson installation, not a full application rebuild/install, runtime brush/preview proof, Windows/macOS or label-generator acceptance. Original 30.017/brush-manifest-generator remains DOING under unmet original dependencies 30.016/01.015; original criterion and both source duties are unchanged. The separate ImageMagick preview-label task is untouched. Publication remains paused.

Reproduce with the configured Meson environment:

    python3 -B tools/generate_painter_brush_manifest.py
    python3 -B migration/tests/test_painter_brush_manifest.py -v
    python3 -B migration/tests/test_mypaint_resource_fixtures.py -v
