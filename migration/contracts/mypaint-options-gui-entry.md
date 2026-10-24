# Original options GUI entries: bounded native presentation mapping

Status: bounded entry restoration verified (normal10, selected5-unit sanitizer8,
42 header routes/88 probes, two actual portrait captures). Parent08.018
criterion remains `C 入口から旧 Interface::cast を排除`; full original GUI
semantics and parent completion are not implied by restoring these two symbols.

Legacy pin: afa43fae3e920210146abed514f136fd49f671b5.
Original signatures in app/tools/gimpmypaintoptions-gui.h:

- `GtkWidget *gimp_mypaint_options_gui (GimpToolOptions *tool_options)`
- `GtkWidget *gimp_mypaint_options_gui_horizontal (GimpToolOptions *tool_options)`

The old header also declared gui_full, but the pinned app tree has no definition
or consumer (git grep finds that declaration only); it is not invented here.

The source-compatible signatures return a floating GTK widget. Both factories
use the existing native editor with vertical/horizontal orientation. Typed
MypaintOptionsRef methods call the corresponding original C entry and sink the
floating widget. No new GType, implementation store, canonical-tool lookup or
second settings model is introduced. An independent options object returned by
`gimp_mypaint_options_new()` has no tool-info and must remain the exact edited
model. Invalid, disposed or unavailable options/application are rejected; native
Gimp lifetime remains borrowed and must outlive the presentation.

## Functional operation coverage and differences

| Original implementation | Native presentation | Scope/remaining obligation |
|---|---|---|
| Basic/Details notebook (:543) | Settings, Shape & Paper, Input Curves plus brush list/actions | Native layout replacement, not identical widget tree/tab labels |
| Horizontal basic controls (:573) | Horizontal full editor orientation | Not the old compact toolbar geometry |
| opaque, radius-logarithmic, slow-tracking, hardness (:412–487) | Exact named numeric controls backed by the same Options | Verify direct C/typed factories, independent/canonical isolation and peer refresh |
| Numeric settings and switches, gimpmypaintbrusheditor.cpp:692–825 | Generated native numeric/switch controls | Existing metadata and same Options properties; no separate store |
| Preset selection and custom-history selection, gimpmypaintbrushoptions-gui.cpp:180–190,221–318 | Brush list and draft-history controls | Native immutable Resource history, not borrowed legacy duplicate GObject identity |
| Input selection, enable/disable, point editing and X/Y range, gimpmypaintbrusheditor.cpp:393–555 | Input Curves controls, point graph and explicit Rescale action | Native rescaling requires explicit action instead of each range spin change; native valid constant curves are retained rather than old constant-Y collapse |
| Draft name, gimpmypaintoptions-gui.cpp:168–180 | Draft name entry | Separate unsaved native name/name-dirty state, not mutable legacy duplicate object's name |
| Save, :183–207 | Save/Save As native actions | **Unmatched legacy semantics:** old same-name delete-and-replace is not restored; native collision, writability and conflict protections remain |
| Delete, :211–227 | Native selected-resource delete | **Unmatched legacy semantics:** old clean-draft-name lookup differs from native source identity/protection checks |
| Popup page memory | Existing native editor popup memory | Layout/popup ownership is native; no held popup-renderer path is edited |
| Size reset, :524–535 | No matching action required | Old entire function body was commented out; no implemented effect to claim |
| Per-setting reset buttons, gimpmypaintbrusheditor.cpp:742–750 | No matching action required | Old signal connection was commented out; tooltip alone is not implemented behavior |
| History name editing, gimpmypaintbrushoptions-gui.cpp:194–197 | No matching action required | Old handler is empty |

The old GUI blindly read tool-info->tool_type even though it did not use the
result. The native entry must not reproduce that accidental null dereference.
Supporting independent options safely does not claim old layout/runtime ABI.

## Required proof

- Actual C consumers and typed delegates, both presentations.
- Independent Options versus canonical model identity; no tool-info assignment.
- All numeric/switch/text controls present; four Basic controls synchronize to
  the exact model and peer view, without changing the canonical model.
- Curve edits and clear through real GTK control callbacks update the same draft
  and peer controls; no fake replacement model.
- Owner-first teardown, invalid/disposed Options and closed app rejection;
  actual toggle callback disposes Options during acquisition, and actual child
  show emission closes the application during construction without publishing
  or leaking the editor.
- Actual rendered vertical/horizontal screenshots under authenticated portrait
  Xvfb1080x1920 with oncanvas enabled. Captions must identify GTK-control API
  interactions rather than physical mouse input.
- Exact selected source archive excluding all eight held providers' candidates;
  focused sanitizer scope and remaining semantic gaps stated explicitly.

Neither this contract nor the bounded wrapper task closes unmatched legacy
save/delete/history/curve timing semantics, original readiness, or parent08.018.
Do not weaken or silently erase those obligations when updating entry inventory.

## Temporary model lease publication audit

The sealed native `gimp_painter_mybrush_editor_set_context()` obtains both an
owned `ref_for_context()` result and an owned context reference, then transfers
them into `EditorImpl.options` and `EditorImpl.context` before building controls
(editor.cpp:710–719). For direct Options input both are the exact supplied model.
The wrapper still owns the widget and checks its live binding before publishing.
Therefore releasing its temporary `model_owner` cannot be the last non-toggle
model reference in this live-editor invariant. This differs from constructor13,
whose temporary ToolInfo lease had no corresponding persistent editor owners.
No new causal last-model-release test is claimed by the existing10/8 receipts.
Application lifetime remains borrowed; no arbitrary post-return application
retirement guarantee is introduced.
