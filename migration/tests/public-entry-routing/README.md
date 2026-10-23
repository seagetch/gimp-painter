# Original 08.018 public-entry routing review (in progress)

The canonical `cpp-c-entry-map.tsv` has **76** entries. Its original acceptance
is removal of old Interface::cast from the C entry into the new implementation,
not deletion of public operations. An absent spelling, a renamed modern type,
or a passing legacy-token scan alone cannot discharge this acceptance.

`tools/audit_painter_public_entries.py` reports every canonical row, exact source
token locations and optional `nm` observations from specified native binaries.
It deliberately does **not** label observations PASS or infer a function
implementation from a header/caller. G_DEFINE_TYPE expands get_type definitions;
unused providers can be garbage-collected from the application link while
remaining available to dedicated native test links. Four tool tests exercise
empty results, exact spelling/line/multiplicity, duplicate canonical names, and
exact linked symbol matching with binary identity. Run:

```
python3 -B tools/tests/test_painter_public_entries.py
python3 -B tools/audit_painter_public_entries.py --binary BUILD/app/gimp-3.0 \
  --binary BUILD/app/tests/gimp-painter-mybrush --output observations.json
```

## Complete row disposition for further semantic review

Numbers are one-based canonical **data** rows, excluding the TSV header.
These are implementation leads and remaining obligations, not new acceptance
statuses. The canonical existing PARTIAL qualifications remain authoritative.

| Rows | Original family | Native provider / remaining obligation |
|---|---|---|
| 1 | Application feature entry | Startup now uses explicit native core/data/tool hooks and application-owned history/HTTP lifetimes, not the old process-global Feature list. Need complete factory lifecycle/initialization-order mapping; test-painter-type-startup.cpp is relevant, not proof of every old hook. |
| 2–3 | BucketFillBrush type/register | app/tools/gimpfillbrushtool.cpp native GimpFillBrushTool and normal tool callback registration; 08.012 covers manager/options. Old type spelling and subclass ABI are not preserved. |
| 4–5 | Canvas perspective type/new | app/display/gimpcanvasperspectiveguide.cpp, native OverlaySlot. Original public names retained; inspect shell/image ownership and native overlay lifetime, not legacy subclass layout. |
| 6–8 | Popup renderer clicked/type/new | Existing popup-entry-handles-08018 evidence; clicked signal metadata remains explicitly PARTIAL. |
| 9–14 | Clone operations/Undo type | Existing clone-entry-handles-08018 evidence; mode enum, source reentry and old Undo semantics remain explicitly distinct. |
| 15–18 | Shell canvas attach/detach/update/opacity | app/display/gimppaintercanvasui.cpp Canvas controller Slot and native shell overlay integration; old four public operations are absent. Native dock transactions/pointer opacity are not automatically source-compatible replacements. |
| 19–20 | Editor dropdown creation | app/widgets/gimpeditor-cxx.cpp and editor-dropdown-handles-08018; dedicated test link retains provider even if application link garbage-collects it. |
| 21–26 | Filter layer operations | Existing filter-entry-handles and filter-legacy-procedure-entry contracts. Original set_procedure remains absent; checked Blinds-only preparation is not arbitrary procedure preparation. Do not alias the historical name to saved-definition storage. |
| 27–29 | ImageGenerator options/tool/register | No native placeholder; removal requires still-open 31.011 dependency/asset proof and 31.012 disposition. Existing cpp-registration review expressly does not authorize silent omission. |
| 30–31 | JSON resource type/new | Native app/core/gimplayerpreset.c owns JsonNode copy, stream load/save and duplication. It replaces actual layer-preset consumer, not yet a proven generic JsonResource public API. Old root property/subclass/general consumer semantics require review; do not create a duplicate JSON model just for symbols. |
| 32–33 | Layer tile type/new | Existing layer-tile-entry-handles-08018; same native GimpPainterLayerTiles controller, explicit source-only alias, no historical GtkBox class ABI. |
| 34,39,45–46 | Brush use/selection/null-motion | Old class-vfunc and use_count APIs are absent. Old base begin/end vfuncs were empty, select returned self and null-motion returned TRUE, but public wrappers maintained use_count and dispatched overridable class slots. Native immutable resource/session snapshots and ordinary GimpBrush resource leases differ; subclass behavior cannot be discharged by no-op compatibility wrappers. |
| 35 | Brush selector | Existing mypaint-selector-entry-08018; historical history-brush identity remains PARTIAL. |
| 36 | Brush duplicate | Existing mypaint-named-duplicate-08018; named independent native resource route retained. |
| 37–38 | Brush editor type/new | app/widgets/gimppaintermybrusheditor.cpp, one EditorSlot over canonical options; modern context/horizontal/error factory signature differs from historical constructor. Parent lifecycle/page tests do not prove historical constructor API. |
| 40 | Brush standard | gimp_painter_mybrush_get_standard in native resource implementation; borrowed singleton weak reset exists, but name/internal identifier differ. An owning handle must retain, not adopt. |
| 41,43 | Brush type/new | Bounded implementation in this change restores original names/signature through sole native type/factory and BrushSlot. No legacy layout/runtime type identity claim. Fresh mixed-language runtime evidence required below. |
| 42,44 | Brush load/save | app/core/gimppaintermybrush.cpp native GFile/GInputStream load and GimpData GOutputStream save vfunc; filename-era signatures, error contracts and independent stream/file ownership require explicit migration. |
| 47 | Brush core Undo type | Native paint-core/session Undo uses current GimpPaintCore and GEGL snapshots; old specialized type is not present. Need exact old record-role mapping, not a new empty type. |
| 48–50,52–56 | Brush options operations/type/GUI | app/paint/painter-mypaint-surface/gimp-painter-options.cpp OptionsSlot owns canonical draft/history; app/tools/gimppaintermybrushtool.cpp builds editor GUI. Old get_current_brush returned options->brush; is_ready tested that pointer, get_brush_mode always returned NORMAL, and copy_brush_props copied brush-size/angle/aspect-ratio properties. Those precise operations, construction and GUI signatures are not all restored. Source-less draft and immutable snapshots require explicit contract mapping. |
| 51,57 | Brush mapping query/set | Existing mypaint-mapping-getter/setter evidence; setter notification differences remain explicit. |
| 58 | Tool color picker | Existing mypaint-colorpicker-08018; native GimpColorPickTarget replaces old GimpColorPickMode. |
| 59–60 | Brush tool type/register | app/tools/gimppaintermybrushtool.cpp native tool registration and shared ToolSlot; old names/type identity absent. |
| 61–65,67–68 | Perspective model operations | Original C names in app/core/gimpperspectiveguide.cpp resolve GuideSlot through store/read/mutate; existing C and owning-handle tests cover point/ownership/close behavior. Finite-coordinate rejection and bounded three-point native state must not be described as unqualified old behavior. |
| 66,69–70 | Perspective options/tool/register | app/tools/gimpperspectiveguidetool.cpp retains tool C names; native options identity must be reviewed separately from guide model. |
| 71 | Popover factory | Native GtkPopover/GtkMenuButton under layer/compact controllers; popover-lifecycle.md explicitly replaces old custom class and omits old class ABI. Factory route remains a public-operation review obligation. |
| 72–73 | Tool tile type/new | Native GtkBox/scroller/overlay inside Canvas controller Slot; tool-widget-lifecycle.md accepts parent/teardown replacement, not old public constructor/type ABI. |
| 74–76 | Preset GUI hooks | Native actions/actions.c, dialogs/dialogs.c and preferences-dialog.c use static action/dialog/preference registration. layer-presets.md records removal of old registry ownership/leaking GArray; exact historical hook return/ownership must be reconciled before claiming public API completion. |

## Scope boundaries

Parent 08.018 remains open. `08.018/core-extension-hooks` remains a separate
unchecked dependent task: container/list/viewable/core helpers and public
header hunks are not covered by this 76-row historical C++ entry inventory.
08.019 handle migration and 31.011 placeholder exclusion must not be silently
marked complete by this review. No UI change or screenshot is claimed here.

## Existing aggregate gate failure

A fresh original `4a83db492ffe90db2372866675d56a45b7355f3f` Git archive and
its own unmodified checker/policy reproduce exactly the candidate's six
unaudited object-data identities in session.c and gimppaintertexttoolbar.inc.
Both scan 2,491 production sources, 705 data sites and four pinned dormant
findings. `legacy-scan-comparison.json` records exact identities and commands.
The checker's 69 unit tests pass, but its actual production scan fails; this
change neither broadens the allowlist nor describes that aggregate gate as green.
