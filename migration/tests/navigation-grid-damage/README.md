# Grid redraw bound correction — bounded25.009 checkpoint

The original196 parent duties remain open. This checkpoint fixes one independently
reproduced native invalidation defect, not all item geometry, old raster equality,
physical input, platform parity, or the whole branch build.

The unchanged native grid draw admits intersection centers two pixels outside
image bounds and extends arms another three pixels. Existing extents stopped at
the image. Actual server-window capture, after full exposure and then ordinary
hide, retained154 changed pixels. The red source, test and receipt are retained.
The correction adds eight local pixels to every edge:2+3+0.5 rounding+1.5 stroke
radius+1 antialiasing. The draw function and all visible geometry are unchanged.

Four focused registered native cases cover120 primitive renders and120 real
window show/hide scenes:0/37/90degrees, both H/V flips, five grid styles, native
grid-style and default tool strokes. The primitive oracle supplies an independent
pinned old viewport CTM but uses the current primitive; it is not an old-grid
raster oracle. Pixel comparisons allow16 channel levels for transform floating
rounding. Every alpha>16 pixel must fall within independently projected extents.

Real damage acceptance reads the GDK server window after normal GTK dispatch.
Only the initial shown frame receives a full exposure. Hide uses the production
item update signal; no forced redraw or gtk_widget_draw conceals missing damage.
A centered470x470 ROI excludes the oncanvas UI and must return to baseline with
zero pixels differing by more than2 channel levels. Initial ordinary combined
four-case execution passed58.27s; final receipts isolate each registered case
within the unchanged60s budget. Actual1080x1920 root-window images show enabled
oncanvas controls at37degrees before and after hide.

Normal and ordinary Meson ASan+UBSan use the private secondary source/build;
assertions remain enabled, O2, debug symbols off. External dependencies are not
instrumented and LSan remains unavailable. Exact source hashes bind every run.
The older Space evidence is immutable and replayed at a16521caf3, not claimed as
fresh current-source aggregate coverage. Old application runtime is USER-SKIPPED.
Unfinished corner/handle probes are not part of this checkpoint or test binary.
