# MyPaint editor context and ownership contract

This is the specification required by **24.001/editor-context-contract**:
“`GimpContext` を正しい型として受け、選択中ブラシと編集 options の取得方法、NULL の扱い、所有権を明記する”.
It does not close parent 24.001 or its resource/tool dependencies. The 18 original source obligations are preserved verbatim in the accompanying trace; a file-level implementation obligation is not satisfied merely by documenting its call boundary.

## Supported boundary and ownership

| Operation | Input / result contract |
|---|---|
| `gimp_mypaint_brush_editor_new(context, menu_factory)` | A valid live `GimpContext`, not an Options cast. NULL/wrong context returns NULL. A non-NULL factory must be a valid factory registering the named editor menu; NULL requests no menu. The result is a floating `GtkWidget*`, which the C caller must sink or attach to an owning GTK container. Failure yields no editor ownership. |
| `gimp_painter_mybrush_editor_new(context, horizontal, error)` | Same real-Context model route, selectable presentation, NULL on failure with the native error channel. No caller Options is fabricated or replaced by the presentation. |
| `MypaintBrushEditorRef::create` / `PainterEditorRef::create` | Calls the exact named C constructor, sinks the floating reference into the sole typed owner, throws on failed construction. `retain` adds ownership of an existing native editor, not another runtime type/model. |
| `gimp_painter_mybrush_editor_set_context(editor, context, error)` | Acquires the canonical owned Options and retains the supplied Context before installing the supported context binding. NULL is not the detach spelling: it fails acquisition. This contract does not assert successful replacement under arbitrary acquisition callbacks. |
| `gimp_docked_set_context(editor, NULL)` | Explicit ordinary dock detach: disconnects bindings, closes queued preview work and prompts, releases Options/Context references and removes generated controls. The editor/menu owner survives and may be reattached to a valid context. |

The named and native runtime getters identify one `GimpPainterMybrushEditor`, derived from `GimpEditor`; the compatibility name is an opaque alias. Old GimpDataEditor instance/class layout, subclass virtual overrides, and historical runtime-name equality are not supported by that alias.

## Editing Options versus selected resource

The editor obtains a **transfer-full** reference from `gimp_painter_mybrush_options_ref_for_context(context, error)` and adopts that reference in its existing implementation slot. Its numeric/curve/name draft operations use this one canonical Options model. The editor also retains the original supplied Context for its ordinary context-binding lifetime. Tool panel, dock, horizontal and popup presentations are not separate mutable draft owners.

`EditorImpl::ctx()` is the Context interface of those owned canonical Options. `gimp_context_get_painter_mybrush(ctx)` supplies a **borrowed** selected `GimpPainterMybrush*`; it is not a transfer-full return and must not be unrefed by a caller merely for reading it. A caller retaining it beyond the immediate owner-stable use, including signal-emitting UI updates, explicitly takes its own reference. The current native identity-refresh and prompt paths do so. Selection/name/save actions must confirm their current model/target before acting; holding a resource reference does not prove that it is still selected or semantically live.

An absent selected resource is not a NULL Options model. Supported canonical model/default and preserved unresolved-name rules are specified separately in `mypaint-context.md` and `mypaint-options-session.md`. The editor's identity display accepts no selected resource and supplies a generic heading; editing uses its Options draft. No cast from arbitrary Context to the legacy GimpMypaintOptions layout is permitted.

## Evidence and explicitly unproved cases

`migration/tests/named-editor-entry/` is immutable predecessor evidence for ordinary valid/NULL/wrong constructor routing, C floating versus typed sunk ownership, shared draft, actual Save/SaveAs, detach/reattach and normal destruction. Its current five-file source successor is `migration/tests/editor-type-alias/validate.py`; use that current gate rather than treating old runtime hashes as current execution. The broader historical editor/context receipts describe their own exact inputs and sanitizer scope, not an automatic current HEAD claim.

The Options acquisition-reentry diagnostic remains platform-paused. This specification does not rerun it, prove arbitrary reentry/target-replacement/disposal acquisition behavior, or convert a strong reference into a semantic-liveness guarantee. No new runtime test is claimed here. Original information-view/session behavior (editable held-resource name, held writability/options sensitivity, preview and autosave) remains unmatched; edit-active must not be represented as arbitrary editable draft pinning or a no-op checkbox.

## Source duty mapping

The accompanying `source-duty-trace.json` retains every original work ID, source blob/range/hash, action and acceptance text. Dialog declaration/registration/constructor duties map to the ordinary real-Context constructor boundary. Legacy GUI base/editor/options declarations map to canonical Options ownership and presentation routing. Curve-view source/header duties map only to the documented controls' model boundary; this document does not claim their whole curve behavior or layout is proven. Widget editor duties map to the explicit Context, selected-resource and ownership contract while leaving the old DataEditor residuals above open. No original implementation duty or parent dependency is marked DONE by this specification.
