# MyPaint working-draft name: source disposition

Status: bounded restoration validated; see report.json for exact native/sanitizer scope and limits.

Source basis: migration base `793860747c930d7952d05435bd6ad9e123d9e1de`; complete legacy tree `afa43fae3e920210146abed514f136fd49f671b5`. Legacy line references below refer to that pinned tree. This is a bounded source review, not a runtime pass or a declaration that the old C API is fully replaced.

## User-visible contract

The working brush is distinct from the selected saved brush. Editing its settings or its pending name must not rename the selected resource or silently discard unsaved changes. A user must be able to name a variant, switch brushes, restore the named draft from history, and save it as a separate resource.

Three identities remain separate:

- The selected resource's native object name and factory identity.
- The editable draft's working name.
- The settings document's `parent_brush_name` metadata.

The prior settings-snapshot implementation preserved editable settings but omitted the independent working name. A read-only name field and a Rename action that immediately persists the selected resource did not preserve that workflow.

## Exact directly affected legacy rows

### `gimp_mypaint_options_get_current_brush`

Legacy entry: `app/paint/gimpmypaintoptions.cpp:533–535`.

Selection first duplicates the selected resource into `options->brush` at `389–403`. Property edits at `230–254` and mapping edits at `539–565` affect that draft. The getter returns the draft borrowed; it does not return the selected factory resource.

Complete-tree search finds six getter call sites:

- `app/paint/gimpmypaintcore.cpp:234,267`: obtain the draft and copy its mappings/settings into the painting engine.
- `app/tools/gimpmypaintbrusheditor.cpp:202,601`: inspect draft mappings for curve editing and input enablement.
- `app/widgets/gimpmypaintbrusheditor.cpp:258,299`: supply the draft to the data editor and its preview. The legacy constructor also contains a null-options defect; preserving that defect is not required.

Disposition: painting, curves, and previews continue to use owned settings snapshots through `gimp_painter_mybrush_options_dup_json()` and `PainterOptionsRef::snapshot()`. Working-name access is supplied separately by the new native C name APIs. `gimp_context_get_painter_mybrush()` remains the selected-source getter and is not a substitute for the legacy draft getter. This does not restore the old borrowed-object ABI or settle every named-handle obligation.

### `gimp_mypaint_brush_duplicate`

Legacy entry: `app/core/gimpmypaintbrush.cpp:350–359`; private copy: `781–798`.

The copy preserves the source object name, settings, independent parent-name metadata, group, mappings, text, and icon. Complete-tree search finds two call sites:

- `app/paint/gimpmypaintoptions.cpp:389`: create the editable working copy when selection changes.
- `app/tools/gimpmypaintoptions-gui.cpp:190`: duplicate the named working draft for saving.

The latter caller is essential: `brush_name_edited()` at `165–173` changes only the draft object's name. Save at `177–195` reads that name, duplicates the draft, saves it, and selects the result. Direct draft consumers also occur at `180,201,286`.

Disposition: options use value-owned settings plus an independent working name; saving a newly named draft creates a separate named native resource from those settings. Generic `gimp_data_duplicate()` alone is not equivalent because its native contract clears the name. Exact duplicate/export/handle compatibility and icon deep-copy behavior remain separate open work.

## Supporting presentation and history paths

The related inventory rows are `gimp_mypaint_options_gui` (`app/tools/gimpmypaintoptions-gui.cpp:543`), `gimp_mypaint_options_gui_horizontal` (`573`), `gimp_mypaint_brush_editor_new` (`app/widgets/gimpmypaintbrusheditor.cpp:284`), and `gimp_mypaint_brush_button_with_popup` (`app/tools/gimpmypaintbrushoptions-gui.cpp:325`). The shared modern editor exposes the restored working-name behavior across its presentations. This subset does not close those rows' remaining constructor, popup, compact-layout, export, or handle obligations.

Legacy history retains the draft object in `app/paint/gimpmypaintoptions-history.cpp:62–72`. Its displayed label comes from that object's name in `app/tools/gimpmypaintbrushoptions-gui.cpp:146–160`; selection at `180–189` restores the historical brush. Consequently, settings-edited drafts retained their edited working names.

Name-only history retention is an explicit extension. Legacy selection checks `GimpMypaintBrushPrivate::is_dirty()` (`gimpmypaintoptions.cpp:392–398`), whereas native name changes set the separate GimpData dirty flag (`app/core/gimpdata.c:356–363`). The old source does not establish reliable history retention for a name-only edit.

## Bounded candidate behavior

Implementation anchors are `app/paint/painter-mypaint-surface/gimp-painter-options.cpp` (`OptionsImpl`, `set_draft_name`, `source_renamed`, `ModelCopy`, history restoration), its C header, and `app/widgets/gimppaintermybrusheditor.cpp` (`refresh_identity`, editor construction, `save_editor`, Rename).

- One existing options store owns independent settings, selected source, working name, and name-dirty state. C name reads return owned text; invalid UTF-8 and null input are rejected, while an empty name is allowed during editing.
- Shared editors update the working name without changing the selected resource or `parent_brush_name`. Name notifications do not emit `settings-changed`, reconfigure painting, or mark settings dirty.
- History records and restores the working name together with settings, including name-only drafts. In-memory config copy and duplication preserve it. Runtime name properties are not serialized into profiles; settings Reset preserves the pending name.
- Save with an unchanged name uses the existing writable-source commit/save path. A changed working name uses the separate-resource save path, including when the source is read-only. Save As uses the working name as its starting point.
- Explicit Rename remains an operation on the saved resource. A clean working name follows the source; a separately edited pending name remains independent.
- Save failure retains the draft. Selection after a successful named save is conditional on the captured source, settings, and working name still being current, so newer edits are not replaced by an older save's result.

## Remaining differences and acceptance limits

1. Name collisions still fail without replacement. Legacy Save deleted a same-name factory resource before saving (`gimpmypaintoptions-gui.cpp:185–194`). Overwrite compatibility remains open; this candidate does not silently introduce that destructive behavior.
2. Legacy icon duplication performs a pixel copy through `set_icon_image()` (`gimpmypaintbrush.cpp:754–770,798`). Mutable-icon deep-copy equivalence remains open and is not established by name restoration or generic resource-copy tests.
3. Old borrowed-object symbols and missing named-handle delegates are not claimed restored. This report describes a caller-oriented semantic replacement for this bounded workflow.
4. Final-source native and ASan/UBSan validation passed the seven registrations and 102 cases in each mode. The initial failed RC-autosave teardown run is retained with its fixture correction in report.json. Publication is a separate step.

Candidate controls are present in `app/tests/test-gimp-painter-options.cpp` for draft-name identity/commit, history/copy, external updates/errors, notification lifetime, and reentry; and in `app/tests/test-painter-mypaint-editor.cpp` as `/painter-editor/17-draft-name-history-save`. The actual-C Options/Session case also verifies owned names, source isolation and a name edit during an active stroke without a settings split. Exact source-matched passing case IDs and clean teardown are recorded in report.json.

The 76 C-entry obligations and 31 core-hook obligations remain separate. This report changes no completion counts and does not close either aggregate.
