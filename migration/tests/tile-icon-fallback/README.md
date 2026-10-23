# Native layer/tool missing-icon paths

The expected red demonstrates that the pre-fix layer path safely accepted NULL
but silently left the icon area as an empty thumbnail checkerboard. It was not a
NULL-unref crash in the current port. The corrected path paints a cached,
theme-independent missing-icon glyph without another raster allocation.

Five real native cases cover an unregistered layer icon name, a loader returning
NULL, a conversion returning NULL, and actual mapped GtkImage fallbacks for an
unknown or NULL tool icon name. The layer glyph is verified by pixels, repeated
draws do not retry the cached failed load, and the unique conversion-fault pixbuf
is finalized exactly once. Retained tool GtkImages survive owner destruction and
finalize on the final release. The tool production path is unchanged: GTK owns
its missing-image fallback rather than the old manual pixbuf unref path.

All cases run with oncanvas enabled in the actual GUI under authenticated
1080×1920×24 Xvfb. The first layer-failure scene is captured as a real portrait
window; see ui-result.json for its dimensions and visual inspection status.
The ordinary canvas/layer aggregate is recorded separately with exact tested
fixture checkpoints; it is not an assertion about future edited fixtures.

Sanitizer scope and RTTI-only units are enumerated in asan-build.json. Other app
units and dependencies are uninstrumented, LSan is disabled, and this is not a
whole-application sanitizer or full historical UI-parity claim. An excluded
fixture attempt confused GtkWidget dispose weak-notification with final storage
release; the corrected retained-icon check uses qdata finalization. Its failure
and exact earlier test source are retained, not represented as a production red.
Original target
budgets are unchanged. Inherited environment maps are omitted from receipts.

Scope and dependency rationale: [contract](../../contracts/tile-icon-fallback.md).
