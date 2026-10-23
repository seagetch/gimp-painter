# Original preset preferences entry and real native page navigation

Restores `GArray *preset_factory_gui_prefs_entry_point(void)` and the exact
seven-field `PrefsDialogEntry` C source structure. Each call allocates independent
caller-owned array storage; strings are borrowed static data. A single native
folder descriptor feeds both this metadata API and the existing preferences page,
including its existing position after Fonts, labels, reset text and property names.
No C++ registry, implementation placement, redundant backend or application store
is introduced. ApplicationRef uses shared ObjectRef ownership and returns the
existing ArrayRef from the sole C function; metadata is application-independent,
including an empty handle. Other ApplicationRef operations remain open.

An actual navigation test exposed the native page's NULL help ID: that value was
also its GtkStack name, so selecting Layer Presets changed the heading but left
its path editor/reset control unmapped. The descriptor now has a unique stable
`gimp-prefs-folders-layer-presets` key. The causal failed run is retained, including
all prior17 passing UI cases. This is not a claim that a new help document exists.

Historical metadata differs (old label/icon/help), so the original entry remains
PARTIAL for historical semantics. The old dynamic registry/Interface/binary ABI
is excluded. No mutable old backend is duplicated. Previous no-op and dialog
proofs remain immutable and are replayed without current-source comparison;
this successor binds the combined current sources and reruns all prior cases.

The image is the actual native preferences page selected through GtkTreeView's
native cursor API, not a pointer gesture. The portrait root is1080x1920 with
oncanvas UI enabled. Prior actual pointer-gesture assertions remain in the suite.
Selected sanitizer units retain no-RTTI, exclude vptr/LSan and do not instrument
dependencies. A fixture attempt to instantiate a second process-global Gimp was
rejected by the native unit singleton; the excluded log is preserved, and the
final ownership test adopts a legitimately transferred reference to the existing
application instead of manufacturing a second application.

## Explicit remaining owner:08.018/preset-legacy-gui-compatibility

Pinned legacy `afa43fae3e920210146abed514f136fd49f671b5`,
`app/presets/layer-preset-gui.cpp:169–176,223–231`:

| Field | Legacy | Current native |
|---|---|---|
| prefs tree_label | Layer Presets | same |
| prefs label | Layer presets Folders | Layer Preset Folders |
| prefs icon | folders-layer-presets | folders-tool-presets |
| prefs help_data | empty string | gimp-prefs-folders-layer-presets |
| prefs fs_label | Select Layer Presets Folders | Select Layer Preset Folders |
| prefs path_property_name | layer-presets-path | same |
| prefs writable_property_name | layer-presets-path-writable | same |
| dialog label | Layer presets | Layer Presets Dialog |
| dialog short label | NULL | Layer Presets |
| dialog help | gimp-layer-preset-dialog | GIMP_HELP_DOCK |

Legacy prefs/dialogs enumerate the mutable config registry; current native entries
expose the sole built-in layer-preset descriptor. Original enumeration/registration
semantics and all differing fields remain explicit open criteria of the above
owner task, not silently accepted parity. Existing native GUI behavior is preserved
except the demonstrated broken page selection. Interface/binary layout compatibility
is not evidenced here. The original empty action-group operation remains empty.

## Successor source and native startup audit

`migration/tests/features-legacy-contract/` now records the exact static
`typeid(config)` key, both legitimate built-in registration callsites, ten
native replacement boundaries and actual before-entry/initialize/restore/exit
observations (native13+6; selected sanitizer13+6). The preceding discussion
is historical checkpoint context, not evidence that the old registry provided
a per-dynamic-type plug-in extension API. Public metadata was restored in
`migration/tests/preset-legacy-metadata/`; before-entry availability and old
lazy observer timing remain explicitly different, with original task criteria
unchanged and no claim of arbitrary-call or old binary parity.
