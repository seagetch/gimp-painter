# Legacy radial drag-zoom arithmetic

The unchanged `start_scaling` and `do_scaling` bodies are extracted from the
pinned old display event source. GTK grab/cursor/expose calls are no-op stubs,
and the scale call records its requested factor. This is source-arithmetic
execution, not a native mouse/tablet capture and not a zoom-model clamp test.

The 2,400 rows cover even/odd viewport centers, horizontal reflection,
anisotropic starting scales, all quadrants, stationary samples and a far start
point that produces negative requested factors on an inward drag. Columns are:
width, height, mirror, initial scale-x/y, start-x/y, current-x/y, requested scale.
Both initial anchoring and update preserve the old `sqrt` arithmetic and the
300-pixel unit distance; they do not accumulate frame-to-frame deltas.

Reproduce with `python migration/tests/capture_zoom_arithmetic.py --legacy
../gimp-painter-legacy`. The full original source is checked against its pinned
Git blob before extraction. The manifest binds the generated source and trace.
