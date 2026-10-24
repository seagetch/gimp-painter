# Missing named MyPaint editor C entry — read-only scope audit

Explicit required route: cpp-c-entry-map row39 names
`gimp_mypaint_brush_editor_new` → `MypaintBrushEditorRef::create`, TODO. Handle
registry row46 names old runtime GimpMypaintBrushEditor/typed handle, also TODO.
This is not automatically satisfied by the alternative native editor entry.

Pinned signature: `GtkWidget *(GimpContext *, GimpMenuFactory *)` in old header54
and implementation284. Sole original production call is dialogs constructor813,
passing real Context and its factory's menu factory. Actual original implementation
has known erroneous Options type check and NULL Options variable, explicitly
called out in original24.015/mypaint-editor-create; do not preserve those bugs.
Native constructor fixes ordinary Context handling and floating widget ownership.

But a bare alias loses observable menu arguments: old constructor installs
menu-factory, menu identifier `<MypaintBrushEditor>`, UI popup path and DataEditor
factory/context/data. Native GimpEditor still supports owned menu-factory and menu
UI, but current MyPaint editor creation supplies none. Old menu has one edit-active
toggle, callback casts to GimpDataEditor. Native MyPaint editor is GimpEditor, not
GimpDataEditor, so re-registering the old callback is invalid. Old edit-active
false stops data-view following context; old inherited aux session stores
edit-active/current-data. Native canonical shared-model editor lacks a matching
pin/session API. These are unmatched semantics, not justification to discard
menu_factory or assert old runtime layout compatibility.

Bounded feasibility: restore exact C header/symbol plus named handle against the
sole native runtime and explicitly PARTIAL compatible construction/ownership;
the menu argument must be deliberately specified and tested, and outstanding
edit-active/menu/session obligations retained. Alternatively implement the full
safe native menu/pin mapping before admitting constructor behavior; this is not a
one-line shim and interacts with shared-model semantics. No new duplicate store,
old GimpDataEditor cast, or second editor implementation is warranted.

Suggested next review decision: confirm intended shared-model policy for old
edit-active=false before implementation. Existing constructor cases should not be
duplicated. Separate original layout/type and menu/session duties from narrow C
linkability with explicit limitations; do not close section24 parent dependencies.
No code edited or runtime invoked in this audit. Held popup/preview paths and the
paused Options acquisition diagnostic are excluded.

## Exact source-derived state transitions (not historical runtime evidence)

1. GimpDataEditor init sets edit_active TRUE. Menu registration initially lists
   the toggle FALSE, but its update callback reads editor state and synchronizes
   action active state. This is not a GObject serialized property: only context,
   data-factory and data are installed by DataEditor.
2. Context attachment retains Context, derives data type from factory, connects
   the matching context resource-change signal, and calls data_changed.
3. data_changed calls virtual set_data only while edit_active TRUE.
4. Switching TRUE→FALSE changes only the boolean; no resource snapshot/Options
   copy is created. Switching FALSE→TRUE queries current Context data and calls
   set_data immediately.
5. get_aux_info appends edit-active="true"/"false" then current-data=name when
   held data exists. set_aux_info processes list order: case-insensitive "true"
   enables; every other value disables. current-data is looked up by exact name
   only when currently disabled; missing names are ignored (no retained missing
   reference). Named resource ownership is through the DataEditor data field.
6. IMPORTANT subclass exception: MyPaint set_data ignores supplied data and
   fetches current brush from its Options Context. Its numeric controls bind to
   that Context. Consequently inherited saved current-data cannot faithfully
   restore an arbitrary pinned brush through this override, and edit-active=false
   does not freeze editable Options settings. It only suppresses base-driven
   data-view update. The constructor is itself invalid as documented above;
   these are exact control-flow facts, not a claimed working old pinned editor.
7. Old set_data replaces retained data/name-change handler; dispose attempts
   save_dirty before virtual set_data(NULL), then detaches Context. Because the
   subclass ignores supplied NULL too, reproducing generic base teardown behavior
   blindly is unsafe. Current widget has a separate owned shared Options model
   and explicit Save actions; no automatic equivalence claim is justified.
8. Current native docked interface only overrides set_context (plus inherited
   GimpEditor behavior), not DataEditor get/set_aux_info/edit_active. Current
   shared model always follows connected Context resource selection. Common
   editor menus/actions/session pinning remain an explicit separate design gap.

Architecture decision must distinguish intended usable native shared-model
semantics from quirks of an unreachable/broken old constructor. Do not fabricate
independent pinned editing from the generic base implementation, or advertise a
thin exact-symbol wrapper as full menu/session behavior. No implementation is
proposed until that boundary is chosen by the parent architecture review.
