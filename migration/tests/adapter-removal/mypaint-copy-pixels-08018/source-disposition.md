# MyPaint exact-row disposition addendum

Reviewed publication: `0b8137a8deab4618e445f2edd9bd124e72cc10d4`, tree `fd36e347deab3e2a865d8dc70ebea9531aa23a60`.
Legacy: `afa43fae3e920210146abed514f136fd49f671b5`.
Inventory: `c-entry-map-08018-20261014/all-76-entries.json`; indices below are one-based entries, not newly invented tasks.

**Result: two directly affected rows have verified bounded semantic restorations; neither entire row is complete. Generic resource copy pixel independence remains open at the reviewed publication.** The four supporting presentation rows receive only the bounded workflow evidence. All original maps and TODOs remain unchanged.

This review reads pinned Git source and existing acceptance evidence. It does not rerun runtime tests or treat the older local HEAD/overlaid worktree as the publication. The complete legacy Git tree is now available; the earlier map's recovered-71-file limitation no longer applies to this specific reference census. No wider retirement or whole-tree compatibility proof follows.

## Exact rows and dispositions

| Row | Exact legacy entry | Accepted bounded behavior | Still open |
| --- | --- | --- | --- |
| 36 | `gimp_mypaint_brush_duplicate` (`app/core/gimpmypaintbrush.cpp:350`) | Independent working name and named Save; full preset settings, mappings, metadata and stored icon survive startup, history and Save | Generic mutable-pixbuf independence; name-preserving original ABI/`MypaintBrushRef::duplicate` disposition; legacy overwrite |
| 50 | `gimp_mypaint_options_get_current_brush` (`app/paint/gimpmypaintoptions.cpp:533`) | Actual painting/editor callers consume the working draft through owned JSON/Resource snapshots; draft-name and startup/Reset gaps repaired | Borrowed-object ABI and `MypaintOptionsRef::get_current_brush` disposition; no whole-row acceptance |
| 35 | `gimp_mypaint_brush_button_with_popup` (`app/tools/gimpmypaintbrushoptions-gui.cpp:325`) | Shared editor's restored name/preset workflow | Original button/thumbnail/list/history role, export and named creator disposition |
| 38 | `gimp_mypaint_brush_editor_new` (`app/widgets/gimpmypaintbrusheditor.cpp:284`) | Shared editor's restored name/preset workflow | Constructor/API consolidation and original named creator disposition |
| 53 | `gimp_mypaint_options_gui` (`app/tools/gimpmypaintoptions-gui.cpp:543`) | Shared editor's restored name/preset workflow | Original exported GUI/named delegate disposition |
| 54 | `gimp_mypaint_options_gui_horizontal` (`app/tools/gimpmypaintoptions-gui.cpp:573`) | Shared editor/compact presentation keeps the restored workflow | Full compact-toolbar mapping, original export/named delegate disposition |

## Caller-oriented source reconciliation

Legacy duplicate copies the native name and private settings, group, mappings, text and icon (`gimpmypaintbrush.cpp:350–359,781–799`). `set_icon_image()` allocates and copies live image bytes (`756–770`). Its two actual callers are selection-to-working-draft (`app/paint/gimpmypaintoptions.cpp:389`) and named Save (`app/tools/gimpmypaintoptions-gui.cpp:190`). Name editing at `165–173` changes the draft; Save at `177–195` reads that name and duplicates the draft.

The six legacy getter calls are painting (`app/paint/gimpmypaintcore.cpp:234,267`), curve/input editing (`app/tools/gimpmypaintbrusheditor.cpp:202,601`), and the widget editor/preview (`app/widgets/gimpmypaintbrusheditor.cpp:258,299`). They require the draft, not selected-resource identity.

At the publication, `PainterOptionsRef::snapshot()` calls the owned-string `gimp_painter_mybrush_options_dup_json()` and decodes an owned value (`gimp-painter-options.hpp:19–24`). Session consumes it at `gimp-painter-session.cpp:57,136`; the editor uses it at `gimppaintermybrusheditor.cpp:150,365,408,656,677`. Owned draft-name reads and edits use `gimp-painter-options.cpp:474–484`; history stores the independent name with the settings (`121–135,567–584`). Name-only history is a deliberate extension, not proven old behavior.

Named Save creates a fresh named resource from the draft at `gimppaintermybrusheditor.cpp:653–670`; it does not call generic `gimp_data_duplicate()`. It rejects collisions at `661`. Unchanged-name Save commits to the selected writable resource and retains a failed Save as a draft (`675–687`). Explicit Rename remains distinct.

Startup uses one `prepare_restore()` call after native Reset (`app/tools/gimp-tools.c:281–303`); it stages the full selected resource without selecting or adding history (`gimp-painter-options.cpp:493–522`). NULL synthetic-property default means no override (`359–369,460`); direct C `set_json(NULL)` still fails. Explicit Reset retains its scalar/bool/text reset semantics while preserving mappings/icon/non-property metadata. Later saved options override normally; specified shape/paper bindings can intentionally update text later during context restoration.

## Acceptance actually established

The machine-readable companion records every checked file/hash and case ID.

- Name publication `79d36312c27a6a8c866d17a87beb53fae05127ff`: all 11 frozen package files match Git; all 17 source seals and 20 referenced evidence hashes match. Final native and ASan/UBSan v3 runs each have seven clean process exits, **102 cases**. Earlier teardown failure and sanitizer timeout remain failed, excluded evidence.
- Preset publication `0b8137…`: all 10 frozen files match Git; all 19 source seals and 23 evidence hashes match. Native and ASan/UBSan v1 runs each have seven clean process exits, **103 cases**. Both packages have 12 passing verifier negative controls. Registered 60/120-second bounds were unchanged; actual-C compilation is recorded; LeakSanitizer is disabled.
- Relevant cases include the five `/painter-options/draft-name-*` cases, `/painter-c/options-draft-name-history-copy`, `/painter-options/native-config-reset-defaults`, `/painter-c/options-prepare-restore-saved-json`, and `/painter-editor/17-draft-name-history-save`.
- The old-source Reset control fails on icon retention. Ordinary-app candidate readback retains **43 settings, three mapped settings**, and decoded 128×128 stored-icon pixels equal to the unchanged source sidecar. Full JSON matches except intended name/opacity edits and embedded PNG representation. This is stored-icon preservation, not rendered-stroke pixel parity.

Evidence: `mypaint-semantics-20261014/publication-frozen/transfer.json`, `mypaint-icons-20261014/publication-frozen/transfer.json`; raw runs under `c-boundaries-validation-20261013/validation/{draft-name-candidate-*-v3,preset-restore-candidate-*-v1}/`. Published reports remain in `migration/tests/adapter-removal/{mypaint-draft-name-08018,mypaint-preset-restore-08018}/report.json`.

## Pending generic-copy acceptance and preserved limits

At the pinned publication, resource `copy()` retains the same `GdkPixbuf` (`app/core/gimppaintermybrush.cpp:63–74`), and `ref_icon()` exposes that owned mutable object (`230–234`). `gimp_data_duplicate()` deliberately clears the native name/file (`app/core/gimpdata.c:1295–1310`); factory duplicate calls it (`gimpdatafactory.c:464`). Existing resource tests compare JSON, roundtrip a stored icon, and check finalizer reentry. **Neither published seven-suite acceptance set includes the resource or factory suite.** They cannot establish generic-copy pixel independence.

A candidate needs actual-C `gimp_data_copy()` and `gimp_data_duplicate()` controls proving equal live pixels in independent storage, bidirectional post-copy mutation isolation, RGB/RGBA/rowstride and missing-icon cases, and retained source/destination lifetime/failure/reentry controls. Re-decoding cached embedded PNG alone does not establish copying the live icon at duplication time. Run final-source resource/factory acceptance and affected workflow regressions; do not carry old results forward as fresh passes.

Overwrite remains open: legacy Save deletes a colliding factory resource before saving; current collision rejection is unchanged. Colorpicker row **58** remains open: caller-selected foreground/background mode is not settled by current foreground modifier behavior. The blocked perspective guard remains open (rows **4–5,61–70**). The **76 C entries and separate 31 core hooks remain separate**; 08.018 and migration count **141/661** are unchanged. No aggregate completion is inferred.

## Accepted copy candidate supplement

Against the pinned publication above, this candidate restores independent live
icon pixel storage for generic GimpData copy and duplicate. Both separate
old-production controls fail when mutating target pixels changes the source.
Candidate native and ASan/UBSan runs each pass all 78 cases in seven actual
registrations, including eight actual-C resource cases and two factory cases.
The copy cases preserve complete JSON metadata and curves, copy live pixels
newer than serialized PNG, and isolate writes in both directions. Missing-icon,
self-copy, source lifetime, explicit icon serialization, and existing icon
finalizer reentry remain covered. See report.json for exact terminal evidence.

This supplies the bounded pixel-independence acceptance missing from row 36
at the publication reviewed above. It does not complete row 36, change native
generic duplicate naming, retire the old name-preserving delegate, or resolve
overwrite compatibility. Row 50 keeps its separately established draft behavior.
The original 76-entry inventory and separate 31 hooks remain unchanged.
