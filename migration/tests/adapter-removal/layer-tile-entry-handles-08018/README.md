# Original LayerTileView construction and property routes (08.018 rows 32–33)

This restores the original zero-argument C constructor/type names and usable
context/image configuration on the existing native layer panel. The actual
Canvas constructor now uses the owning LayerTileViewRef and those properties.
It does not introduce a second widget, GType, implementation store or UI model.

## Contract and ownership

- gimp_layer_tile_view_new(void) returns a floating, initially unbound widget.
  LayerTileViewRef::create calls that original provider and sinks it once;
  retain adds a reference to an already-owned or parented widget.
- The original READWRITE|CONSTRUCT context and image properties retain their
  exact inputs. Object getters return caller-owned references. Assignment order,
  NULL, repeated replacement and independent image/context lifetimes are covered.
- The context getter returns the exact supplied context. A separate private
  action context preserves each canvas's image/display isolation. Its image and
  display properties are explicitly defined locally, even when the supplied
  context inherits them. Configuration never imports the global active image.
- An image can be bound before a context. Rows and image identity remain usable;
  native preview creation waits until context exists. Clearing context cancels
  pending preview work, retains the independently bound image/rows and makes
  context-dependent actions inert. Restoring context recreates real previews.
- Newer nested requests win during popup closure, native parent notification and
  preview rebuild. Destroy is terminal. Retained storage does not keep a GTK
  widget operational or reconnect a closed binding.
- The existing native factory uses the same property/configuration helper.
  Configure this widget through its properties or supported native factory;
  direct external GimpDocked context replacement is outside this restored route.

Source compatibility is operation-level only. GimpLayerTileView aliases the
existing GimpPainterLayerTiles type. The native GimpImageEditor parent, runtime
GType name, GTK3 presentation and class layout replace the historical GtkBox and
C++ Interface. Old binary layout, subclass ABI and Interface::cast compatibility
are not claimed.

## Original source correspondence

Pinned legacy commit: afa43fae3e920210146abed514f136fd49f671b5.
The original header declares the no-argument factory and GType provider;
app/widgets/gimplayertileview.cpp:326–331 installs both construct properties,
:452–474 and :549–563 implement exact retaining pointer getters/setters, and
:1594–1595 exposes the C entries. The original create_layer_view caller in
app/display/gimpdisplayshell-overlays.cpp constructs first, then assigns context
and image. The current route preserves that deferred configuration contract
through the one native Tiles store; see source-disposition.json for exact scope.

## Final acceptance

42 native and 42 ASan+UBSan case executions pass. In each mode, the new focused
suite has 20 actual-C11 and 12 C++14 cases, followed by ten individually selected
existing canvas regressions. All 22 processes, Meson commands and authenticated
X servers exit 0; no skips, timeouts or sanitizer diagnostics are accepted.
LSan is disabled. This is not the full canvas or application regression suite.

Focused controls cover 162 nullable assignment transitions; construction order;
exact getter ownership; repeated replacement; private/source context isolation;
preview invalidation; same-target, ABA, close and refresh reentry; closed/invalid
inputs; safe wrong-thread binding admission without GTK worker-thread calls;
handle copy/move/parent release orders; actual selection/visibility, clone and
preset actions followed by Undo; and both real application canvases.

The two-canvas case starts each action while the other image is globally active
and verifies the intended image/result and Undo. Normal popup dismissal restores
canvas focus and may update the global active display before command execution.
That normal UI behavior is retained. Separate configuration and standalone
context controls establish source-context nonmutation and private action routing.

The exact existing selections, each run in both modes, are:

- selection_and_visibility
- image_switch_disconnect
- popup_controls_and_clone
- destroyed_tiles_reject_image
- image_switch_popup_owner_close
- image_switch_popup_newer_image
- parent_image_notify_owner_close
- multiple_canvas_context_and_dock_transfer
- popup_repeat_and_owner_close
- long_press_cancel_destroy

Both public-header gates pass: 377 compile probes (179 C11 and 198 C++14), with
explicit additive coverage for the restored C header. Four positive and three
negative owning-handle compile controls pass. The 48 policy tests and existing
90-contract/76-entry checks pass. Strict evidence verification and corruption
controls are recorded in report.json, together with independent review, source,
binary and raw-evidence hashes.

The native application was incrementally relinked and contains exactly one
original factory and type symbol, as do both focused test binaries. Normal
configured Canvas appearance is unchanged: context/image are assigned before
parenting/showing. The incomplete API-only preview state is not a new ordinary
workflow, so no artificial fixture screenshot is used. No new full-application
shutdown or ASan application-link claim is made.

## Retained failures and limits

The first candidate run exposed a test's incorrect conflation of GTK weak
notifications with final storage release. Qdata destruction witnesses now prove
final release, separately from terminal GTK disposal. A subsequent candidate
exposed real NULL-context preview drawing; deferring renderer creation fixed it
without suppressing warnings. Other fixture corrections respect automatic signal
teardown, native resource identifiers and normal canvas focus. Failed logs and
source seals remain retained. Verifier drafts were corrected to recognize a C++
forward declaration, actual header dependencies and source-proven per-mode Meson
runtime options; final results use the final strict verifier.

Only rows 32–33 implementation evidence changes. The other 74 entries, separate
31 core hooks, type/handle catalog, 141/661 progress count and overall 08.018 stay
open/unchanged. This does not close broad WBS29 visual/gesture parity, introduce
new ToolTileView or MyPaint selectors, retire ImageGenerator, or alter the
permission-blocked perspective work.

## Reproduction

Using the established configured native/ASan build and normal GUI test environment:

    meson compile -C BUILD -j2 layer-tile-entry-contracts painter-canvas-ui
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs layer-tile-entry-contracts
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs painter-canvas-ui --test-args '-p /painter-canvas-ui/CASE'

Run each listed CASE separately. The focused registration timeout remains 120
seconds and each selected canvas registration keeps its original 60 seconds.
No timeout multiplier, alternate UI implementation or sanitizer suppression is
part of this acceptance.
