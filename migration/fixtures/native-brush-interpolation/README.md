# Native interpolation before resumable extraction

`trace.tsv.gz` was captured by executing the unmodified GIMP3
`gimp_brush_core_interpolate()` from the already built application archives,
before introducing any continuation state or pause API. Exact source and
producer hashes, commands and baseline revision are in `runtime.json`. The
trace producer owns no alternate C++ GObject bridge: its native C-shaped brush
subclass only records the callbacks and updates native transform dynamics.

The180 scenarios cover horizontal/vertical/diagonal/corner/grid-epsilon/tiny
and stationary-pressure motions, spacing5/20/250, brush size1/17, pressure
size/spacing dynamics, seeded native jitter, their combination and zero-scale
fallback. Records include all14 floating device/view axes and reflection,
current and previous coordinates, last painted point, accumulated brush/pixel
distance and transformed brush state at every native dab and motion endpoint.

This is a native GIMP3 pre-refactor numerical oracle, not an independently run
old GIMP2.8 Fill Brush oracle. It complements the pinned old full-stroke/Undo
fixtures in `../legacy-fill-brush/`. Do not substitute a comparison between two
new implementations for either independent baseline.

The raw stdout and stderr are retained. Normalization keeps only INTERP records;
ordinary startup/data-path diagnostics are not fixture values. The original
capture exit was0. No source hooks were added until the producer compiled and
ran against the untouched algorithm.
