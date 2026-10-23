# LayerTileView GTK3 parent and lifetime contract (08.013)

The legacy GimpLayerTileView GtkBox is replaced by GimpPainterLayerTiles, which
derives from GimpImageEditor and therefore GimpEditor/GtkBox. A single native
GType and BindingStore Slot<Tiles> implement the widget. C instance/class
layouts contain only their native parent; the former C++ Interface::cast and
static NewGClass instance are not part of the new interface.

## Parent and action routing

The native image-editor context and image are the action-data source. The
legacy actions.c special cases for Gimp/context/image/display are replaced by
the existing native GimpImageEditor branch and context/display fallback. Each
canvas tile keeps its own context and display, so a global active-image change
does not redirect its actions. NULL image remains NULL instead of falling back
to another display. See the seven source-specific replacements and frozen Git
blob identities in ../tests/layer-widget-lifecycle/source-duty-review.json.

## Terminal teardown and reentry

The destroy vfunc closes BindingStore before chaining to its GTK parent. The
slot closes long-press, rebuild and autoscroll sources, popup controls and all
connections, drops model/drag state and clears the parent's docked context,
image and private context. An externally retained destroyed widget remains a
valid GObject but must not reconnect an image or recreate interaction state.
The set_image adapter allows NULL during close to disconnect the parent's
flush callback; non-NULL assignment requires an active binding.

Parent set_image runs under the existing owner lease. A sensitivity-notify
callback can destroy the widget or change its image; the binding resumes only
if still open and the parent still points to the requested image. bind_image
retains its incoming image, increments image_generation, then checks closed
state and generation after synchronous popup closure and context notification.
An interrupted assignment cannot retain the new image until finalization or
overwrite the image selected by a newer nested call.

## Verification scope and recovered evidence

Three isolated baseline cases reproduced post-destroy reconnection, retention
after popup-close destruction, and replacement of a newer image. The fixed
normal full canvas suite passed 27/27 in 7.348 seconds. The matching standard
ASan/UBSan application suite passed 27/27 in 14.251 seconds with no sanitizer
diagnostics. Weak pointers assert owner/image finalization; post-destroy image
flush probes disconnected parent callbacks. Both-canvas action routing is
explicitly covered. The parent-notify positive control ran after fixed source
staging and is not counted as baseline red evidence.

These summaries and TAP lines were reconstructed from recorded successful
execution output after a workspace reset. Final raw Meson logs and original
publication summaries were lost; neither raw-log recovery nor a fresh run is
claimed. Library checkpoint v15 preserved the exact tested sources and patch;
source hashes and the reconstructed unified diff match their prior records.
The reports explicitly retain this provenance.

Leak detection was disabled; this is not LSan evidence. Original task 08.013
can close with its seven source obligations after 08.012 acceptance. Full
all-target aggregate, platform and WBS29 gesture/visual acceptance stay separate.
