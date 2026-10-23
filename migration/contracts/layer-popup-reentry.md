# Layer popup reentry guard

Closing an existing popup synchronously emits `popup-closed`. An observer may
destroy its LayerTiles owner or select another image. BindingStore keeps the
object alive during the call, but its image/context can already be cleared.
The old `show_popup()` resumed allocation after this callback and dereferenced
cleared state, or completed an obsolete image request.

Validate closed/image/context before entering; capture image_generation; after
`close_popup()` validate them again and reject changed image generations before
creating any replacement controls. Existing GTK parent/lifetime handling stays
intact. No new ownership store or alternate widget implementation is introduced.

Four regressions cover owner destruction and image switching during replacement,
for both creation and properties popups. They check NULL cancellation, one
callback, image routing, no remaining long-press timer, unchanged layer counts
and weak finalization. The first test draft incorrectly disconnected an already
disconnected handler after GtkWidget destruction; the corrected cleanup checks
`g_signal_handler_is_connected`, retaining every lifecycle assertion.

All four regressions fail against the old widget implementation. With the guard
and corrected cleanup, all four individually pass, and the complete native
canvas suite passes 23/23. Earlier failed candidate runs remain in the evidence.
See `../tests/layer-popup-reentry/validation.json` and `tap.txt`.

This closes one concrete reentry defect, not WBS 08.013 as a whole. Its seven
source obligations, 08.012 dependency and sanitizer acceptance remain open.
