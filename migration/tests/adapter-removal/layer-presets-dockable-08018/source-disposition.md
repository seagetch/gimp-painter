# Layer Presets exact-row disposition addendum

Public base: `a6f5109fd04f100899e20a195262857ee51387a6`; tree `a338c4de19a976224fa97ba70c35cb97a064bb48`. Legacy: `afa43fae3e920210146abed514f136fd49f671b5`.

**Bounded dockable restoration accepted: native-v3 36/36 and ASan/UBSan-v1 36/36. Real row-click double application remains open.** The companion JSON accounts for all **76 exact entries** in original order; the **31 core hooks remain separate**. No whole row, original TODO, 08.018 or overall migration count is closed. This review reads completed evidence; it does not rerun tests/builds/verifiers or publish.

## Final candidate identity

The stable four-file seal is `candidate-source-seal.json`, SHA-256 `3ef390d5a2faaa3362736485ee7e52ae8a4c53401635bfca006ed4d58f6b2880`. Both verified runs bind that exact seal and the current file bytes:

- `app/dialogs/dialogs.c`: `dd2320e084f0a49e7d55e60844bf64417e82b2190fa911a2fa086daa48884aae`
- `app/actions/dialogs-actions.c`: `d65cf83962610c3bef8fd728bf76fb6094c3684303c6cafa963b94849856f883`
- `app/widgets/gimplayerpresetview.c`: `9d9293289f0a6821dfe9ad3c5a0688a7d77ef207dd49714372b2f21258d18228`
- `app/tests/test-layer-presets-ui.c`: `0b44e96027df51e49f7b094eb794b764499e2937f9c00231bb8e0104143677ef`

The production changes are non-NULL `GIMP_HELP_DOCK` in the Layer Presets dialog/action registrations and a GimpLayerPresetView `GimpDocked` override clearing only optional `get_preview`. Inherited context/menu/button-bar/aux-info methods remain. The existing layer icon is used by native tab fallback; Preview+Name normalizes to Icon+Name. No new context resource property, selected-preset thumbnail or manual page is claimed.

The final test correction awaits natural GTK mapping through a local weak pointer, two-second monotonic bound, nonblocking context iterations and 1ms sleeps. It does not retain the widget or force visibility, parent, configuration or mapping. Destroyed/unmapped widgets still fail; mapped and source/history assertions remain. Both final aggregate runs pass. Earlier help-only preview failure and action-after-factory mapping failure remain failed evidence rather than being relabeled.

## Exact original-row disposition

| Row | Exact legacy entry | Bounded credit | Still open |
| --- | --- | --- | --- |
| 75 | `preset_factory_gui_dialogs_actions_entry_point` | Real factory/action dockable construction, help/preview integration, context, no implicit Apply, normal Close Tab and reopen | Original C export/named-handle disposition; real row-click double Apply; whole-row acceptance |
| 1 | `features_entry_point` | One native GUI registration effect of old preset restore | All-feature/headless/HTTP initialize–restore–exit; old entry/named facade |
| 74 | `preset_factory_gui_action_group_entry_point` | Wrapper-derived menu/explicit Apply callback and cached manager/group teardown | Original entry/named delegate; full action-factory scope; physical row-click dispatch |
| Other 73 | Individually enumerated in JSON | No new exact-row disposition | Prior independent accepted subsets and remaining duties retain their own scope |

Row 76's preferences/configuration-path behavior is unchanged. Backend and MyPaint suites are useful controls, not automatic closure of every original entry they happen to invoke.

## Pinned legacy callers and intended semantics

- Legacy `app/app.c:175` calls `features_entry_point(gimp)`. Its factory table selects PresetGuiFactory for GUI builds and attaches initialize/restore/exit (`app/gimp-features.cpp:49–81,87–132`).
- `PresetGuiFactory::initialize_factory()` registers LayerPresetGuiConfig; `restore()` registers its dialog entry and action-group callbacks (`app/presets/preset-factory-gui.cpp:93–96,177–217`).
- The exact row-75 C wrapper forwards to `dialogs_actions_entry_point()` (`:240–243`); that method builds native string actions with `dialogs_create_dockable_cmd_callback` (`:123–144`). `app/actions/dialogs-actions.c:335` invokes it, and `menus/dialogs-menuitems.xml:28` exposes `dialogs-layer-presets`.
- `LayerPresetGuiConfig::view_dialog_new_entry()` names `gimp-layer-preset-list`, uses small preview/non-singleton/dockable metadata, and supplies **non-NULL empty text** as its help ID (`layer-preset-gui.cpp:189–210`). Its action uses `"gimp-layer-preset-dialog"` (`:223–232`), found only there in the pinned app/menu source. This is not evidence of a manual page.
- The actual implementation is `LayerPresetFactoryView`, derived from GimpDataFactoryView. Selection and activation apply to the current context image/layer (`:56–98`). `create_view()` supplies list view, context, factory, `<LayerPresets>`, `/layer-presets-popup` and `layer-presets` (`:121–149`). Native GimpLayerPresetView already preserves that list/data-factory structure with explicit Apply/Refresh.
- The apparent new/duplicate item-tree preset dropdown callers (`gimpitemtreeview.c:424–433,460–466`) are inactive `#else` branches under literal `#if 1`. They are neither enabled nor claimed as active legacy workflows by this fix.

The restoration is semantic/native integration, not binary compatibility: this candidate does not recreate the old singleton factory, GArray assembly, original exported forwarding functions or promised C++ named facades. The exact old-symbol obligations require an explicit replacement/retirement disposition. Absence of old names is neither proof of missing behavior nor automatic permission to mark the rows complete.

## Executed acceptance and limits

Both saved strict verifications pass the same three registered suites:

| Suite | Native-v3 | ASan/UBSan-v1 | Unchanged deadline |
| --- | --- | --- | --- |
| Layer Presets UI | 6/6, 9.33 s | 6/6, 20.82 s | 120 s |
| Layer-preset backend | 11/11, 1.37 s | 11/11, 1.78 s | 90 s |
| MyPaint editor | 19/19, 19.40 s | 19/19, 69.68 s | 120 s |

All three process exits, Meson exit and X server exit are zero in each mode. Referenced result/registration hashes and registration-source hashes were rechecked against the saved reports. Timeouts were not expanded; no multiplier was used. Isolated HOME/XDG/TMP and authenticated loopback X controls are recorded. C consumers compile with `cc`; no explicit C11 claim is made. LeakSanitizer is disabled. The diagnostic exception is narrowly limited to one exact known nonfatal libcloudproviders DEBUG bus-launch notice per relevant GUI process; it is not blanket warning suppression.

Reports:
- `c-boundaries-validation-20261013/validation/layer-presets-dockable-candidate-native-v3/verified-results.json`
- `c-boundaries-validation-20261013/validation/layer-presets-dockable-candidate-asan-v1/verified-results.json`

The two new cases independently exercise actual factory raise and the actual `dialogs-layer-presets` UI action, with no creating fallback on the action path. They execute the real wrapper, notebook/dock ownership, three preview tab styles, inherited context, wrapper-derived menu, repeated raise, two Close Tab/reopen cycles, cache/weak-owner teardown and source-preserving no-implicit-Apply checks. Context detach/reattach remains nonmutating. Source identity/count/selection, bytes/geometry/name/opacity/mode, dirty state and Undo/Redo depths are checked across event drains. Programmatic selection and explicit Apply are tested and undone.

The new wrapper tests do not activate Refresh through the wrapper menu, assert generated clone type/source linkage explicitly, or compare top-history record identities. Older inner-view/backend controls retain those separate scopes. Passing programmatic selection is not proof of every real pointer event sequence, as the ordinary application observation below demonstrates. Source/compile/execution provenance has been reconciled; this review is not an independent build reproduction.

## Ordinary application and the next concrete gap

The ordinary candidate receipt records open → Close Tab → reopen → repeated open/raise → close → normal quit, exit **0**. Opening/reopening/repeated opening preserves the original two-layer canvas. The source XCF remains byte-identical (`a096d27e263f54996d96a84cb0e75323bd3c23b826acd2e130b073ceed123122`), runtime inputs are stable, and no resource-initialization errors are recorded. The parent reports the before/after images were shared with the user before commit. The ordinary executable uses the candidate production build made at native-v2; final v3 changes only the regression mapping wait. Visual capture contributes no extra automated case count.

**One real click on the Clone layer preset row added two clones.** The recorded input helper emits exactly one left-button press/release pair (`visual-prep/native_input.py:26–30`). The inspected `action-10.png` shows two added layers and its captured window title reports four layers from the original two; successive Undos return to three then two. Afterward, the explicit Apply button produces one added layer (`after-explicit-apply-one-clone.png`, three total). This is an open duplicate-dispatch/application gap. Its callback cause has not been established here. General Layer Presets Apply correctness and full legacy UI parity are therefore **not accepted**.

Evidence is `visual-prep/candidate-01/results.json` and `visual-prep/candidate-01/screenshots/capture-receipt.json`, with the original screenshots. The normal candidate quit success does not establish the cause of the earlier ordinary baseline quit -11. The next bounded work is to reproduce the real row-click event sequence and diagnose why it applies twice; no fix for that gap is included in this four-file candidate.

## Preserved open scope

- Legacy MyPaint overwrite remains open; existing collision rejection is not legacy deletion/replacement. Prior draft/preset/copy restorations are neither reversed nor reaccepted here.
- Exact row 58's configurable foreground/background colorpicker setter and API disposition remain open.
- Perspective rows 4–5 and 61–70 retain the blocked eleven-file guard and valid-index/wrong-point identity gap. No integration or acceptance credit.
- ImageGenerator dependency/retirement proof stays independent.
- Original exported C functions/named-handle contracts are not closed merely because native behavior is restored. All 76 entries and separate 31 core hooks retain exact accounting; zero whole-row completions.
