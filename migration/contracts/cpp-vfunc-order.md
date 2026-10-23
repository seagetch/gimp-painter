# Original 05.011: parent vfunc order and callback reentry

This is the type-specific contract required by original 05.011. It supplements
the shared ownership/store contract; it does not mark each feature implementation
or the old 111-vfunc behavioral migration complete. The machine-readable
`cpp-vfunc-order.json` records current types, assigned class/interface slots,
parent order, reentry rules and source identities. Types used only through a
handle do not acquire a new overriding class merely by being referenced.

## Rules for adapters

1. Assign the exact native slot type in the real class/interface initializer.
   Invoke a saved parent implementation through that parent class. Calling the
   public virtual dispatcher to chain up can recursively dispatch to the same
   override and is not a parent call. An inherited slot needs no forwarding stub.
2. Instance initialization only registers the declared implementation state.
   The default constructed route calls the native parent, then activates and
   connects the subclass. The table identifies deliberate alternatives: plain
   value owners can activate in init; private procedure progress installs its
   state after construction. Do not replace these with a universal parent-first
   or activation-first rule. Construct properties can arrive before constructed.
3. Dispose/destroy closes owned asynchronous work and connections before the
   listed parent phase. Close is idempotent; repeated and nested native disposal
   must still chain as required. The store and permitted immutable state remain
   present for parent cleanup until finalization. Finalization is not a place to
   revive the owner or call back into a destroyed implementation.
4. Property IDs are local to their defining class. Follow each adapter's stated
   own-property and parent-property dispatch; do not blindly forward an invalid
   ID or accidentally interpret a parent's property as a child's. Inherited
   properties, explicit overrides and invalid IDs are separate cases.
5. Painting, projection, undo, saving, interface and widget handlers are not all
   additive. Some replace a native algorithm, some call the parent first, and
   some chain only for fallback modes. Preserve the row's exact branch order,
   outputs and ownership. Never run Filter workers from GEGL source evaluation.
6. A parent call, notification, native virtual method or destroy notifier can
   synchronously reenter. Retain the owner for the full operation that uses it,
   retain/copy callback state before disconnecting it, and validate the relevant
   active/closed or generation state before applying delayed results. A strong
   reference prevents finalization; it does not make a disposed owner mutable.
7. Never carry a borrowed implementation past a callback boundary without the
   store's operation lease. Close must detach state before destructors or signal
   disconnection can reenter. No mutex is held while calling native/user code.
   Idle, input, preview and model callbacks use the type-specific guards listed
   in the table. Synchronous callback entry guards do not prove that every
   possible future callback composition has been tested.
8. The error mapping and permitted execution contexts retain their own original
   05.012/05.013 tasks. This contract does not silently certify every class-init
   failure, every platform ABI or every feature's exception behavior.

## Coverage and exclusions

The inventory enumerates production application C++ GTypes, including private
service/stream owners, plus native C compatibility counterparts. Conditional
types remain visible: the big-endian stream and optional HTTP owner have contracts
even when not instantiated in this Linux configuration. The existing native
MaskComponents C++ class is explicitly identified as upstream inheritance rather
than a new Painter implementation.

All 90 legacy handle-registry entries are classified. The 23 old custom runtime
types have an explicit current counterpart, native composition or pending
disposition. ImageGenerator's absent placeholder types retain their original
31.011/31.012 exclusion prerequisites. This document does not authorize removal
or invent an implemented replacement. Old GTK helper classes replaced by native
widgets retain their separate feature behavior obligations. A later added or
replaced GType must add its slot/order row before it satisfies this contract.

## Evidence and completion boundary

Existing native types also host behavior without introducing a new subclass.
Their explicit lifecycle hooks keep their native parent chain:

| Existing owner | Current hook order and reentry boundary | Source |
| --- | --- | --- |
| Gimp | Dispose HTTP service; close application store before data factories/configuration disappear; then native parent dispose. Repeated close is allowed; shutdown must not restart work | app/core/gimp.c |
| GimpPlugInManager | Instance init installs provider slot; parent constructed then activate; close provider slot before parent dispose. Finalize releases native plug-in definitions only afterward | app/plug-in/gimppluginmanager.c; app/core/gimpfilterparametereditor.cpp |
| GimpDisplayShell | Native constructed finishes shell setup before Canvas initialization; Canvas close and perspective-snap reset occur before shell teardown and parent dispose | app/display/gimpdisplayshell.c; app/display/gimppaintercanvasui.cpp |
| GimpImage | Perspective-guide disposal runs before undo/container teardown and parent dispose. The native image owns its guide state; existing guide types use their separate rows | app/core/gimpimage.c; app/core/gimpimage-perspective-guide.c |
| GtkDialog / GtkWidget attachments | Actual destroy signals close dialog/presentation slots. These are signal handlers, not new vfunc overrides; they do not manually invoke a parent class closure. Weak owner/generation or epoch guards invalidate detached controls | app/dialogs/painter-layer-dialog.cpp; app/widgets/gimppaintercompactoptions.cpp |

The current native hierarchy regression passes three cases: inherited/own
properties and interface dispatch, interface exception return, and parent-dispose
reentry. The real C instance/constructed/property/dispose/finalize fixture passes
its lifecycle case. It asserts reverse slot close order, parent reads after
close, repeated/nested disposal and destruction only after the last owner. These
are real C/C++/GObject tests, not hand-written simulations of vtable calls.

The immediately preceding 04.019 checkpoint records the same implementation
sources and the full 40-case foundation suite, including its ASan/UBSan run.
Feature test references in the type rows identify relevant coverage; a source
test name by itself is not a new runtime PASS. The new checkpoint checks the
type/slot inventory and source hashes and preserves that distinction.

Run `python3 tools/check_painter_vfunc_order.py` to verify source identities,
current-type coverage, legacy dispositions and the four native contract results.
No production vfunc behavior is changed merely to make a uniform ordering rule.
Original 05.011 closes when the type-specific contract is complete and consistent;
it does not wait for all later feature/platform WBS tasks or close them implicitly.

<!-- BEGIN CURRENT TYPE CONTRACTS -->

## Current type contracts

### EndianInput → G_TYPE_INPUT_STREAM

Source: `app/xcf/painter-xcf-arguments.cpp`

- Activation: Big-endian-only private C-layout input stream. Factory initializes borrowed array data/length/width before synchronous snapshot read. No BindingStore.
- Cleanup: Inherit GInputStream disposal/finalization; borrowed data not freed by stream.
- Reentry: read checks cancellable, bounds bytes to block size, maps component byte order and advances position. Caller retains source values during synchronous conversion; no UI/user callback or asynchronous raw-pointer escape.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GInputStream.read_fn | `endian_input_read` | replacement reads borrowed components, no parent read |

### GimpCanvasPerspectiveGuide → GIMP_TYPE_CANVAS_ITEM

Source: `app/display/gimpcanvasperspectiveguide.cpp`

- Activation: Register OverlaySlot in init; native parent constructed first establishes shell, then activate/connect/refresh. Shell display may arrive later through notify::display.
- Cleanup: dispose closes all signal connections and guide refs before native parent; no custom finalize.
- Reentry: Signal payload locks weak owner and checks store generation; with/read lease protects Impl. close clears state and refs; draw/extents read snapshots and do not mutate guide. Extent calculation bounds integer conversion.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.constructed | `constructed` | parent -> activate -> connect/refresh |
| GObject.dispose | `dispose` | close store -> parent |
| GimpCanvasItem.draw | `draw` | replacement, no parent drawing |
| GimpCanvasItem.get_extents | `extents` | replacement, no parent extents |

### GimpCloneLayer → GimpLayer

Source: `app/core/gimpclonelayer.cpp`

- Activation: Instance init ensure/emplace -> constructing. Parent constructed first -> verify slot -> activate -> watch_owner_image. binding_failed closes the store; construction factory rejects it.
- Cleanup: Close store before native layer dispose. Close increments reference_generation, destroys idle source/connections, clears weak owners/source and balances mirrored preview freeze before parent teardown. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: Signal Callback stores weak owner and BindingStore generation. visit_callback locks owner, checks accepts, then takes a with lease. reference_generation invalidates in-flight set_source/update/restore after synchronous notifications; Flag guards resolving/updating/restoring/editing_reference; dependency cycles are checked; one owned idle source coalesces refresh.
- Property dispatch: Own get_property ID 1 = binding-failed, no own setter. Parent property IDs are handled by their GObject property owner; direct foreign ID call warns.
- Construct reentry: No own construction setter. Parent image/name/property callbacks may run while CloneSlot is constructing; read may succeed, with and generation-accepts must fail until parent returns and derived activation finishes.
- Limits: The lease protects implementation lifetime, not transaction atomicity. Generations are checked at explicit emission/flush boundaries. duplicate lacks a scoped owner for the newly allocated copy after parent duplicate, unlike FilterLayer; exception cleanup of that path is not established by this read-only audit. No new failing execution was observed.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | Parent invariants finish while store is constructing; activate only afterwards. |
| GObjectClass.get_property | `get_property` | Own property IDs only; unexpected IDs warn, with no explicit parent forwarding. Normal GObject property dispatch uses the property-defining class; do not call this adapter directly with foreign parent IDs. Own ID 1 reads the C binding_failed flag without requiring active store. |
| GObjectClass.dispose | `dispose` | Close own store and callbacks before native parent disposal emits disconnect or releases native resources. |
| GimpItemClass.duplicate | `duplicate` | Native copy/cache/mask/provenance first; then resolve source and copy clone reference metadata only if copy is a CloneLayer. |
| GimpItemClass.is_content_locked | `content_locked` | Intentional replacement: always TRUE; initialize optional locked_item to this item. |
| GimpItemClass.scale | `scale` | Intentional legacy no-op for a derived reference layer; parent pixel transform is suppressed. Other inherited operations remain native. |
| GimpItemClass.flip | `flip` | Intentional legacy no-op for a derived reference layer; parent pixel transform is suppressed. Other inherited operations remain native. |
| GimpItemClass.rotate | `rotate` | Intentional legacy no-op for a derived reference layer; parent pixel transform is suppressed. Other inherited operations remain native. |
| GimpItemClass.transform | `transform` | Intentional legacy no-op for a derived reference layer; parent pixel transform is suppressed. Other inherited operations remain native. |
| GimpPickableInterface.get_opacity_at | `opacity_at` | Intentional 0.0 hit-opacity policy; do not invoke inherited drawable opacity. Other Pickable slots stay inherited. |

### GimpCloneLayerUndo → GimpItemUndo

Source: `app/core/gimpclonelayer.cpp`

- Activation: Instance init ensure/emplace -> constructing. Parent constructed validates image/item -> require CloneLayer and source-undo kind -> capture target state through active target slot -> activate -> install source image watcher. Any recorded failure closes store.
- Cleanup: dispose and free close store before their parents. Close disconnects saved source watcher, releases weak identities and cached buffer, clears saved names. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: Weak owner/generation guard on saved source notify::image. pop requires active undo and target stores, leases both, calls parent, rechecks undo generation, then capture/restore. Clone restore checks target generation after callbacks before swapping undo snapshot; late callbacks become inert.
- Property dispatch: No own property setter/getter. item is defined by GimpItemUndo; image/time/undo-type/dirty-mask by GimpUndo; name by GimpObject.
- Construct reentry: Inherited construct properties are ready before derived constructed; snapshot initialization/activation runs only after native parent returns. Source watcher is installed after activation.
- Limits: No same-Undo nested-pop busy flag; arbitrary recursive external undo is not promised. Guarded source replacement/close is covered. free is a lifecycle boundary and subsequent pop must be inert.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `reference_undo_constructed` | Parent constructs native undo identity before feature snapshot/watch installation. |
| GObjectClass.dispose | `reference_undo_dispose` | Close store before inherited native GimpObject dispose. |
| GimpObjectClass.get_memsize | `reference_undo_memsize` | Read own saved string capacities/cache size, then add native undo memsize. Boundary failure returns 0 and skips parent, an explicit fallback limitation. |
| GimpUndoClass.pop | `reference_undo_pop` | Only after active undo/target validation and leases; parent first, then generation check and clone snapshot restore. Rejected/closed calls skip parent. |
| GimpUndoClass.free | `reference_undo_free` | Close own state before parent releases item, preventing source callbacks during item destruction. |

### GimpDeferredSave → G_TYPE_OBJECT

Source: `app/dialogs/file-save-deferred.cpp`

- Activation: Register Pending slot in instance init; parent constructed first, then activate.
- Cleanup: close disconnects sources/connections and detaches request before destruction; dispose closes before parent dispose; store finalizer owns Impl destruction.
- Reentry: Source retains operation; request snapshots and weak owner/progress locks survive callbacks. running blocks nested attempts; closed and display/image identity are checked after pending-paint/save callbacks; no nested main loop.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.constructed | `constructed` | parent constructed -> activate store |
| GObject.dispose | `dispose` | close_save -> parent dispose |

### GimpFillBrush → GimpBrushCore

Source: `app/paint/gimpfillbrush.cpp`

- Activation: BrushSlot emplaced in init; parent constructed then activate. begin owns image/drawable/options, connects target watches, sets starting and one-shot permits, calls native start, then INIT. step owns shared segment/dab leases. finish marks ending and started=false before FINISH/commit/cancel; release scratch and frame follows. close sets closed+cancel_requested first, defers while starting/preparing/draining/ending and otherwise rolls back. dispose closes before parent; native finalize inherited.
- Cleanup: BrushCore pre_paint, post_paint, interpolate, get_paint_buffer and signal closures remain inherited; PaintCore push_undo and final resource destruction remain native. Boolean handles_* class flags are not vfuncs.
- Reentry: Native callbacks may occur during projection flush, start, paint publication and finish. Captured revision is rechecked after flush/interpolation/publication; local shared segment/dab and object leases survive invalidation. Target buffer changed uses a shared atomic TargetWatch and ignores owned_write; query-pending-paint reads busy without borrowing Impl; Connection owns/disconnects closure identity. release_frame sets busy=false and disconnects before releasing resources. finish_frame marks ending before dispatching FINISH. Independent target pixels/format/geometry are preserved rather than stale rollback. Generic operation cancellation is checked before commit seal; postcommit notifications cannot undo the sealed result. No claim that every arbitrary synchronous signal recursion is rejected by BindingStore itself; the concrete flags and revisions above define the supported transition discipline.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → activate own store; Inherited construction properties must be ready before active callbacks |
| GObjectClass.dispose | `dispose` | close own BindingStore → parent dispose; Reentrant callbacks from parent teardown must see closing/closed state |
| GimpPaintCoreClass.check_start | `check_start` | require start_permit && starting && !closed → consume permit → arm native_start; The native base check_start slot is NULL; admission must happen before native allocations/callbacks |
| GimpPaintCoreClass.start | `native_start` | require native_start_armed && starting && !closed → consume arm → parent BrushCore.start; Native resource/brush setup follows owned admission |
| GimpPaintCoreClass.paint | `paint` | INIT/FINISH clear dab state; MOTION queues bounded-fill dab; errors recorded; Own renderer replaces base no-op paint; pre_paint/post_paint remain native |

### GimpFillBrushOptions → GimpPaintOptions

Source: `app/paint/gimpfillbrush.cpp`

- Activation: Settings slot emplaced in instance init; constructing set_property uses initialize; constructed calls parent then activate; dispose closes the store before parent; no own finalize (qdata store final destruction). No owned signals/sources in the settings slot.
- Cleanup: All GimpPaintOptions context/config handlers and finalization inherited; no new config interface.
- Reentry: Unknown IDs warn; these types do not implement explicit fallback to parent get/set. Do not classify the absence of a chain as a lost inherited-property bug: normal GObject property dispatch resolves the defining property owner. The contract records this exact behavior rather than claiming the foundation fallback wording is implemented for arbitrary direct vfunc invocation.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `options_constructed` | parent constructed → activate own store; Inherited construction properties must be ready before active callbacks |
| GObjectClass.dispose | `options_dispose` | close own BindingStore → parent dispose; Reentrant callbacks from parent teardown must see closing/closed state |
| GObjectClass.set_property | `options_set` | own id → constructing initialize or active mutation; unknown id → warning; Own properties are handled here; ordinary inherited-property dispatch uses the property-defining class |
| GObjectClass.get_property | `options_get` | const typed-slot read → own output; unknown id → warning; Own values only; no synthetic parent-ID fallback |

### GimpFillBrushTool → GimpBrushTool

Source: `app/tools/gimpfillbrushtool.cpp`

- Activation: Controller slot in init; BrushTool parent constructed first (native preview core), then activate. dispose closes controller/store before parent Tool disposal may redispatch HALT. Controller close publishes closed and invalidates; revision++, source closed, connections/strokes swapped out, input reset, then disconnections, halt and cancellation. No own finalize.
- Cleanup: BrushTool cursor_update/options_notify/paint_start/end/flush, PaintTool modifier/key handling and underlying color picking are inherited. No own options property/config vfuncs.
- Reentry: with() admits only active stores, making parent-dispose HALT a safe own-state no-op; inherited parent control still runs. tick() rejects dispatch recursion and retains shared front stroke; compares revision after each callback-producing phase. committing suppresses self-generated dirty/clean invalidation; dispatching suppresses own drawable update invalidation. Idle callback captures weak owner + BindingStore generation, takes a strong owner lease, requires accepts(generation), and enters typed slot. Source handles destruction during its own dispatch; if an invalidation destroys that source and new input arrives, it rearms the newer controller source. Signal closures for target/display/image observations hold raw tool pointer, with Connection disconnection at close; these are synchronous GObject notifications, not queued weak-generation payloads. Their callbacks use active-store gate. Deferred idle is separately weak/generation validated. saving signal calls public COMMIT only when input exists and image still matches. Public gimp_tool_control always follows COMMIT by HALT (gimptool.c:656); commit_halts counts nested pairs. Watch signals include image selection/mask/dirty/clean/precision/saving/pending, drawable removed/lock/format/geometry/update, display image and shell destruction.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → activate own store; Inherited construction properties must be ready before active callbacks |
| GObjectClass.dispose | `dispose` | close own BindingStore → parent dispose; Reentrant callbacks from parent teardown must see closing/closed state |
| GimpToolClass.control | `control` | COMMIT seal current envelope + increment commit_halts + schedule; HALT consume commit_halts or invalidate; then parent control; Parent retains native tool control behavior |
| GimpToolClass.button_press | `press` | picker enabled → parent only; otherwise validate target, freeze options, enqueue stroke and schedule; Painting uses owned resumable controller; picker preserves inherited behavior |
| GimpToolClass.motion | `motion` | picker enabled → parent only; otherwise enqueue input, update preview with pause/resume; Do not start an extra native PaintTool transaction |
| GimpToolClass.button_release | `release` | picker enabled → parent only; otherwise halt control then cancel or seal/schedule input; Owned asynchronous transaction may remain after pointer release |
| GimpToolClass.oper_update | `oper_update` | invalidate pending strokes if display changed → parent oper_update; Parent BrushTool supplies native preview behavior |
| GimpDrawToolClass.draw | `draw` | reject unrepresentable off-canvas cursor/line → otherwise parent draw; Canvas preview numeric domain is narrower than accepted input |
| GimpPaintToolClass.get_outline | `outline` | reject off-canvas transformed outline → otherwise return parent outline; Preserve native outline while avoiding invalid canvas properties |

### GimpFilterLayer → GimpLayer

Source: `app/core/gimpfilterlayer.cpp`

- Activation: Instance init ensure/emplace -> constructing. Parent constructed -> verify slot -> activate; failed binding closes. Factory/duplicate attach signal graph after native layer creation.
- Cleanup: Close store before native layer dispose. Close native-context and scheduler first, deactivate progress, destroy source/connections, release staged buffer and weak dependency graph. Saved definition/argument values remain in implementation until destruction. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: Weak owner + binding generation tokens gate signals and idle dispatch. dispatching suppresses recursive step execution. Scheduler generation and definition_revision distinguish cancellation/replacement and stale publications. publish_definition rechecks after configure, buffer publication and each notify. Progress start rechecks its token after synchronous filter-progress-changed; worker state uses snapshots and owner-thread delivery.
- Property dispatch: Own read-only IDs 1..5 = binding-failed/filter-procedure/filter-arguments/filter-original-definition/filter-opaque-arguments; no own setter. Foreign direct IDs warn; inherited owner properties stay native.
- Construct reentry: No own construction setter. Feature store stays constructing through parent constructed; inherited construction callbacks cannot enter active Filter mutation. Graph attachment follows active construction in factory or duplicate.
- Limits: Store leases allow mutable reentry; they do not roll back prior native effects. Definition publication has explicit current checks. UI/image Undo-history atomicity and arbitrary observer loops are not implied. Saved data remains readable after close; progress is inactive.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | Complete native image/item construction before active-only feature callbacks. |
| GObjectClass.dispose | `dispose` | Close own state/callbacks before native parent disposal. |
| GObjectClass.get_property | `get_property` | Own property IDs only; unexpected IDs warn, with no explicit parent forwarding. Normal GObject property dispatch uses the property-defining class; do not call this adapter directly with foreign parent IDs. IDs 1..5 read binding_failed and copied/ref-counted definition data; data getters use const store reads. |
| GimpItemClass.duplicate | `duplicate` | Parent duplicates native pixels/mask/provenance, then scoped copy owner installs feature definition, attachments/configuration and cache state. Binding token/revision checks reject reentrant replacement and scoped owner releases failed partial copy. |
| GimpItemClass.is_content_locked | `content_locked` | Intentional replacement: always TRUE and optional locked_item is this item. |
| GimpPickableInterface.get_opacity_at | `opacity_at` | Intentional 0.0 hit-opacity policy; other Pickable operations remain inherited. |
| GimpProgressInterface.start | `filter_progress_start` | Own interface implementation; no parent progress backend. Reject current progress, snapshot scheduler generation, publish active/cancellable/text, emit progress-changed, return self only if still current after reentry. |
| GimpProgressInterface.end | `filter_progress_end` | Own interface implementation; no parent progress backend. If current, clear active/cancellable before emitting; no mutation after emission. |
| GimpProgressInterface.is_active | `filter_progress_active` | Own interface implementation; no parent progress backend. Const store read; true only when active and scheduler generation still matches and scheduler not closed. |
| GimpProgressInterface.set_text | `filter_progress_text` | Own interface implementation; no parent progress backend. Active lease, current-generation gate; bounded copy then notification. |
| GimpProgressInterface.set_value | `filter_progress_value` | Own interface implementation; no parent progress backend. Ignore non-finite value; active/current gate, clamp to [0,1], then notification. |
| GimpProgressInterface.get_value | `filter_progress_get_value` | Own interface implementation; no parent progress backend. Const read; current fraction, otherwise 0.0. |
| GimpProgressInterface.pulse | `filter_progress_pulse` | Own interface implementation; no parent progress backend. Active/current gate, saturating pulse increment then notification. |
| GimpProgressInterface.message | `filter_progress_message` | Own interface implementation; no parent progress backend. Active/current/severity gate, bounded message snapshot and saturating revision increment; emit then report accepted TRUE. |
| GimpProgressInterface.cancel | `filter_progress_cancel` | Own interface implementation; no parent progress backend. Active lease; if current and cancellable, cancel the captured generation through the C entry. |

### GimpFilterLayerUndo → GimpItemUndo

Source: `app/core/gimpfilterlayer.cpp`

- Activation: Instance init registers slot. Parent constructed -> initialize snapshot from target const read -> activate. binding_failed captures failure, but this constructed adapter does not explicitly close on failure; store may remain constructing until dispose.
- Cleanup: Store closes before inherited native dispose; payload/cache released. No own free override: inherited GimpItemUndo.free releases item and leaves store closure to dispose. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: pop holds active undo and target store leases. It copies current cache, advances definition revision, swaps snapshot/model before publish_definition. That publication checks target generation/revision after callback boundaries. Memsize accepts only active store for own bytes.
- Property dispatch: No own properties or property adapters; native parent item/image/undo metadata are inherited.
- Construct reentry: Native parent constructed returns before snapshot initialize then activate. Its recorded failure leaves constructing/partial state until later dispose; no active callbacks should be installed in that failure path.
- Limits: No own nested-pop or undo-generation guard around publication. Snapshot fields are committed before notifications; outer pop performs no slot mutation after publish. Construction-failure close asymmetry is a documented path, not an observed runtime failure. No guarantee for reuse after inherited free.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `undo_constructed` | Native item/image properties first, then snapshot cache/definition and activate. Failure sets binding_failed but does not explicitly close. |
| GObjectClass.dispose | `undo_dispose` | Close snapshot store before inherited native dispose. |
| GimpUndoClass.pop | `undo_pop` | Always calls parent first, including closed/missing-store cases, then boundary validates active undo and target before swapping/publishing. |
| GimpObjectClass.get_memsize | `undo_memsize` | Bounded own-memory estimate (zero for absent/non-active store) then always add parent, saturating at G_MAXINT64. |

### GimpLayerPreset → GIMP_TYPE_DATA

Source: `app/core/gimplayerpreset.c`

- Activation: Native C compatibility resource replacing old JSON resource. Inherit constructed; factory installs initial deep-copied JSON. No C++ BindingStore.
- Cleanup: Clear owned JsonNode before parent finalize; native dispose inherited.
- Reentry: set_json builds replacement before publication, retains destination across name/dirty notifications. save deep-copies a snapshot before stream writes; copy uses same setter. No callback receives borrowed C++ state.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.finalize | `finalize` | clear JSON -> parent finalize |
| GimpData.save | `save` | replacement writes owned snapshot to supplied stream, no parent save |
| GimpData.get_extension | `extension` | replacement static .json extension, no parent |
| GimpData.copy | `copy` | replacement deep-copy setter, no parent copy |

### GimpLayerPresetView → GIMP_TYPE_DATA_FACTORY_VIEW

Source: `app/widgets/gimplayerpresetview.c`

- Activation: Native C view. Inherited construction assembles container/menu; factory sets ready only after action button setup. No C++ BindingStore.
- Cleanup: dispose marks menu_closed and ready false, removes owner-keyed menu cache once, then native parent dispose.
- Reentry: ready suppresses construction/refresh selection application; applying prevents nested preset execution. apply retains view through callback and resets state before unref. Selection order preserves parent selection bookkeeping before eligible legacy single-click application.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.dispose | `dispose` | remove menu cache once -> parent dispose |
| GimpContainerEditor.select_item | `select_item` | parent selection -> ready/eligible apply |
| GimpContainerEditor.activate_item | `activate_item` | replacement explicit apply, no parent activate |

### GimpOperationMaskComponents → GEGL_TYPE_OPERATION_POINT_COMPOSER

Source: `app/operations/gimpoperationmaskcomponents.cc`

- Activation: Existing upstream native C-layout operation; property construction and GEGL prepare initialize scalar format/process state; no BindingStore.
- Cleanup: No subclass dispose/finalize, inherited native cleanup.
- Reentry: No UI/signal-owned C++ callback state. GEGL owns evaluation lifetime; prepare selects native processing format. Process forwards whole input/aux only on documented mask fast paths; otherwise native parent dispatch invokes point kernel.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.set_property | `gimp_operation_mask_components_set_property` | own property cases only; invalid IDs warn; no parent call |
| GObject.get_property | `gimp_operation_mask_components_get_property` | own property cases only; invalid IDs warn; no parent call |
| GeglOperation.prepare | `gimp_operation_mask_components_prepare` | replacement: set formats/process, no parent prepare |
| GeglOperation.get_bounding_box | `gimp_operation_mask_components_get_bounding_box` | replacement: mask-selected or union bounds, no parent |
| GeglOperation.process | `gimp_operation_mask_components_parent_process` | input/aux forwarding returns directly; otherwise parent process |
| GeglOperationPointComposer.process | `gimp_operation_mask_components_process` | replacement kernel; no parent point process |

### GimpOperationPainterLegacy → GIMP_TYPE_OPERATION_LAYER_MODE

Source: `app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c`

- Activation: Native GEGL layer operation with POD arithmetic state. No C++ BindingStore; inherited native lifecycle.
- Cleanup: No custom dispose/finalize.
- Reentry: prepare stages nonlinear compatibility fields before parent prepares formats, then records source extent. Pure pixel process uses supplied arrays/ROI and never starts Filter execution or UI work. GEGL evaluation owns operation lifetime; native platform/precision feature acceptance remains separate.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GeglOperation.prepare | `prepare` | set compatibility fields -> parent prepare -> capture aux bounds |
| GimpOperationLayerMode.process | `process` | replacement legacy pixel arithmetic, no parent process |
| GimpOperationLayerMode.get_affected_region | `affected` | replacement UNION result, no parent |

### GimpPainterDeviceOptions → GimpToolOptions

Source: `app/core/gimptooloptions.c`

- Activation: Empty class_init and instance_init. Native inherited construction only; no BindingStore or C++ slot.
- Cleanup: No override. Inherit gimp_tool_options_dispose: clear tool_info before parent context disposal. No override/no store. All parent property/config/constructed/dispose/finalize handlers remain inherited.
- Reentry: No feature-specific callback adapter/guard. Inherited notify::tool handler and GimpConfig reset are unchanged; type identity makes gimp_tool_options_check_tool_info accept any registered tool for a migrated context snapshot.
- Property dispatch: No own properties/adapters. Inherit GimpToolOptions tool override and tool-info/legacy-native-spacing/legacy-native-hardness plus native GimpContext/GimpObject properties. Parent handlers warn unknown IDs; their legacy one-shot geometry setters can call nested g_object_set where supported.
- Construct reentry: No C++ store/activation at any point. Native GimpContext construction and GimpToolOptions notify::tool handler determine callback order; exact subtype relaxes only tool-info type matching. No additional reentry guard is introduced.
- Limits: No extra guarantees beyond native GimpToolOptions/GimpContext. It deliberately does not masquerade as a complete numerical tool-options subtype.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| Inherited | No override | Native parent dispatch remains unchanged |

### GimpPainterHttpd → G_TYPE_OBJECT

Source: `app/httpd/httpd.cpp`

- Activation: Register and activate ServiceSlot in init; start later configures application/server on a strong local owner.
- Cleanup: dispose closes service before GObject parent; Service close cancels pending requests/hooks before native refs are released.
- Reentry: Async and server callback payloads lock weak State, closed/generation guards suppress stale work; Soup/native operations run through existing owner-context routes. Dispose never waits for callbacks.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.dispose | `dispose` | close store -> parent dispose |

### GimpPainterLayerTiles → GIMP_TYPE_IMAGE_EDITOR

Source: `app/widgets/gimppainterlayertiles.cpp`

- Activation: Register, activate and build slot in init. Public factory later sets context/menu/image. Inherit constructed.
- Cleanup: Widget destroy closes slot before parent destroy; inherited native dispose/destroy route and store finalizer remain responsible for remaining ownership.
- Reentry: use only admits active store and retains owner during slot call. Signal dispatch copies weak-owner target/command before callbacks. closed/syncing/image_generation and moved popup/child owners prevent stale model/idle results and destructive reentry from reusing released controls.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GtkWidget.destroy | `close_widget` | close store -> parent destroy |
| GimpImageEditor.set_image | `set_image` | parent set_image -> active-slot bind_image |

### GimpPainterMybrush → GimpData

Source: `app/core/gimppaintermybrush.cpp`

- Activation: Instance init registers slot. Parent GimpData constructed/thaw runs while store constructing; then activate. binding_failed closes store and factory rejects object.
- Cleanup: Close before inherited GimpObject dispose. Brush close releases icon; resource JSON stays readable for const serialization until finalization. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: copy retains source and destination across snapshot, mutation and gimp_data_dirty notifications. Store read/with leases pin implementation; resource/icon replacements happen before outward signals. set_json/set_icon additionally hold owner leases through dirty/settings/size signals.
- Property dispatch: No own property overrides. GimpObject name, GimpData mime/file/image etc and inherited Viewable properties use native owner handlers.
- Construct reentry: Parent GimpData constructed calls gimp_data_thaw (gimpdata.c:777), which can call dirty and emit invalidate-preview/name-changed before derived activation. BrushSlot already exists; const read is permitted, active write rejected. Own adapter does not activate before parent returns.
- Limits: No generation or recursive-notification suppression for dirty/settings-changed/size-changed. A callback can close or change the owner; memory stays valid, but arbitrary transactional atomicity is not promised. Direct preview after close reflects released icon.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | GimpData sets writable and thaws native data before feature activation. Read callbacks can see constructing slot; active writes must fail until activation. |
| GObjectClass.dispose | `dispose` | Close own icon state before native disconnect/final resource teardown. |
| GimpDataClass.save | `save` | Parent save is NULL. Const read copies resource, stamps current native name and writes snapshot to stream under boundary; returns FALSE/GError on failure. |
| GimpDataClass.get_extension | `extension` | Parent extension NULL; return immutable .myb suffix, no store access. |
| GimpDataClass.copy | `copy` | Parent copy NULL. Source/destination leases, copy resource/icon, mutate destination, then native dirty notification. |
| GimpViewableClass.get_size | `get_size` | Own icon dimensions or 48x48 fallback. Initialize valid outputs to 0 before boundary; absent output pointer returns FALSE. |
| GimpViewableClass.get_new_pixbuf | `get_pixbuf` | Own icon scaled proportionally; NULL for missing icon/invalid dimensions. Native caller supplies fallback icon, no parent image generation. |
| GimpViewableClass.get_new_preview | `get_preview` | Calls own get_pixbuf and converts to native TempBuf; NULL if no icon. |
| GimpTaggedInterface.get_checksum | `checksum` | SHA256 of owned encoded JSON copy, not parent checksum. Failure returns NULL. |

### GimpPainterMybrushEditor → GIMP_TYPE_EDITOR

Source: `app/widgets/gimppaintermybrusheditor.cpp`

- Activation: Register EditorSlot in init. Parent constructed first; then activate unless binding failed. Context/UI are installed by public factory/set_context.
- Cleanup: Both destroy and dispose close store before their native parents; repeated close is harmless; finalize inherited.
- Reentry: Signal callbacks lock weak owner, check generation/UI revision, copy callable before invocation and use store lease. Closed/syncing/replacing guards and retained old widgets protect replacement/notifications; preview request numbers invalidate stale work. NULL docked context detaches UI through same slot.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObject.constructed | `constructed` | parent -> activate |
| GtkWidget.destroy | `destroy` | close store -> parent destroy |
| GObject.dispose | `dispose` | close store -> parent dispose |
| GimpDocked.set_context | `docked_init::set_context lambda` | replacement interface handler: set context or detach, no parent interface invocation |
| GimpDocked.get_title | `docked_init::get_title lambda` | replacement returns allocated title, no parent |

### GimpPainterMybrushOptions → GimpPaintOptions

Source: `app/paint/painter-mypaint-surface/gimp-painter-options.cpp`

- Activation: OptionsSlot emplaced in init. selected_changed can initialize during construction; constructed calls parent then activates and sets up shared application history/resource selection. close disconnects selected signal and releases selected/history. Parent PaintOptions handles its native resources/finalize. ApplicationHistory is a behavior slot on Gimp, initialized explicitly only if no store exists, activated then shared by options; application-close stops its source and clears entries.
- Cleanup: GimpPaintOptions finalize and Config reset inherited; normal GObject inherited-property dispatch unchanged. No claim that legacy registry TODO handles become complete just because this current adapter exists.
- Reentry: notify_changed holds owner, captures generation, checks generation+revision before sync_resources, each property notify and settings-changed emission. Nested notification owns newer revision and older notification batch stops. select_source publishes selected/draft/baseline/conflict and increments revision before closing old connection and unref old selected; finalizers can reselect safely. settings-changed closure uses WeakOptions owner+generation. source_changed ignores other selection and own committing; dirty draft becomes conflict rather than silently overwritten. commit snapshots source/baseline/revision, sets committing before resource replacement, clears it only on active state, and reconciles only if selected identity/revision still match. Cancellation/close cannot borrow retired Impl. History idle owns weak HistoryState; snapshots strong targets+generation before emissions so recursive options creation/removal cannot invalidate iteration. Close sets closed and destroys source before clearing observers/entries.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → activate → attach application history → select current/default resource; failure closes; Construction properties ready before revisioned resource notifications |
| GObjectClass.dispose | `dispose` | close own binding → parent dispose; Disconnect selected-resource signal/release state before parent notifications |
| GObjectClass.set_property | `set_property` | own JSON/settings dispatch; constructing numeric initialize vs active with; notify after mutation; Own numeric/JSON model; inherited property owner dispatch remains native |
| GObjectClass.get_property | `get_property` | constructing initialize const lambda; otherwise read; fill own JSON/settings/dirty/conflict; Serialization/cleanup read of own draft |
| GimpContextClass.painter_mybrush_changed | `selected_changed` | select_source; publish complete replacement then release old objects/connections; notify; Own canonical draft selection replaces base empty closure |
| GimpContextClass.brush_changed | `brush_changed` | if parent closure exists call it → synchronize brushmark name; Keep PaintOptions brush-link behavior before own named-resource update |
| GimpContextClass.pattern_changed | `pattern_changed` | if parent closure exists call it → synchronize texture name; Keep any inherited context pattern behavior |
| GimpConfigInterface.copy | `config_copy` | snapshot source → parent copy; on success and matching flags restore model snapshot → notify; Native context properties copy first, full draft/dirty/conflict/selection semantics restored afterward |
| GimpConfigInterface.duplicate | `config_duplicate` | snapshot source → parent duplicate → restore snapshot → release owned target; Native duplicate constructs object before model restore |
| GimpConfigInterface.serialize_property | `nullptr` | clear inherited interface override; Inherited custom serializer understands Context property IDs only; use generic config value serialization for own settings |
| GimpConfigInterface.deserialize_property | `nullptr` | clear inherited interface override; Inherited custom deserializer understands Context IDs only; own settings use generic path |

### GimpPainterMybrushTool → GimpColorTool

Source: `app/tools/gimppaintermybrushtool.cpp`

- Activation: ToolSlot emplaced in init. constructed creates one GimpPainterSession after parent, initializes then activates, closes on binding failure. close moves session/connections out, clears pressed/hover/weak targets, disconnects, halts/stops drawing, clears native drawable/display state, then cancels retired session. dispose close precedes inherited Tool HALT. No own finalize.
- Cleanup: Inherits ColorTool cursor_update/can_pick/pick/picked and DrawTool/Tool behavior except enumerated overrides. Standard GimpMybrushTool (C, PaintTool subclass) remains a separate registered tool and is not the migrated extended tool.
- Reentry: Operation retains owner and captures generation; sets busy, rejects nested input. Destructor only operates if generation still accepted, clears busy, takes pending inactive sample, then processes it after the outer call unwinds. Targets are weakly observed by WeakTool{owner,generation}; callbacks lock owner and require accepts. stopping prevents recursive cancellation. image_dirty ignores busy/stopping and inactive Session; saving finishes Session; query_pending reports only executing input busy. Input retains display/image/drawable and Session across engine/projection/display callbacks. Generation is rechecked before clearing error. Close or target cancellation clears queued release so it cannot revive the old operation.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → validate Painter options → create Session → initialize slot → activate; failed binding closes; Session owns painting transaction; parent construction must expose tool options |
| GObjectClass.dispose | `dispose` | close binding → clear native input/control → parent dispose; Publish retired own state before inherited HALT |
| GimpToolClass.control | `control` | HALT/COMMIT finish session and clear input if usable → parent control; Preserve legacy commit-on-HALT and native control |
| GimpToolClass.button_press | `press` | unusable/non-normal return; picker→parent only; otherwise if not busy set pressed and input paint; ColorTool is used for picking; Session owns real painting |
| GimpToolClass.button_release | `release` | unusable return; picker→parent only; otherwise clear pressed/halt; cancel or inactive input; Release lets engine decide logical split |
| GimpToolClass.motion | `motion` | unusable return; picker→parent only; otherwise input using pressed/control-active state; Full-motion idle/hover goes through Session |
| GimpToolClass.modifier_key | `modifier` | constrain modifier enables/disables native foreground color picker; Own modifier behavior selected for ColorTool subclass |
| GimpToolClass.oper_update | `oper_update` | picker→parent only; otherwise update cursor/start-stop redraw by proximity/display; Own circle preview lifecycle |
| GimpDrawToolClass.draw | `draw` | unusable/no-image return; picker→parent only; otherwise read radius and draw circle; Own radius preview; native picker rendering remains inherited |

### GimpPainterPaintGate → GimpPaintCore

Source: `app/paint/gimppainterpaintgate.cpp`

- Activation: Instance init ensures/emplace GateSlot and immediately activates. There is no constructed override; this intentionally records current early activation, since slot needs no construction properties. dispose closes before parent; inherited PaintCore finalization. GateImpl close moves Operation away then requests cancel. Admission retains core/generation/shared Operation; destructor rolls back batch before dropping pending-paint signal, restores coordinate fields unless committed, and removes slot operation only if generation is still accepted.
- Cleanup: All native paint/interpolate/pre/post/get-buffer/push-undo handlers and GObject properties/finalize inherited. The ordinary entry is refused; gimp_painter_paint_gate_stroke is a C entry using Session batch, not another vfunc.
- Reentry: Admission rejects an already present operation; generation validation after session construction, each segment, samples and before commit rejects close/cancel. WeakOperation query callback locks a shared Operation and reads active, never raw Impl. Close moves slot operation before invoking Session cancel. Admission owns Operation independently, so pending-paint stays true until actual rollback completes even if owner is disposed inside evaluator callback.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.dispose | `dispose` | close own store → parent dispose; Cancel current Operation before inherited teardown |
| GimpPaintCoreClass.check_start | `refuse` | set explicit GIMP error → FALSE; No direct native start: atomic Stroke/Path/PDB adapter must own Session batch |
| GimpPaintCoreClass.start | `refuse` | set explicit GIMP error → FALSE; Defensive refusal even when start vfunc called directly |

### GimpPainterProcedureProgress → GObject

Source: `app/core/gimpfilterprocedure.cpp`

- Activation: GObject init is empty and store absent during g_object_new/constructed. ProcedureProgress C++ constructor explicitly ensure/emplace(sink)/activate afterwards. Never expose progress before that explicit installation.
- Cleanup: Own dispose finds/closes store before GObject dispose. ProgressState.close only clears active; sink/data/failure survive until store destruction. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: Every interface method uses boundary plus active with lease (including read-like queries). FilterProgress snapshot is taken before sink callback; no progress mutex is held while sink executes. Sink exceptions are caught/stored and later rethrown by ProcedureProgress.finish inside C++ caller.
- Property dispatch: No own properties and no property adapters.
- Construct reentry: Opposite special case: no store exists during instance init or native constructed. C++ ProcedureProgress installs and activates the store after g_object_new returns, before exposing its GimpProgress pointer.
- Limits: Private helper only; sink delivery is synchronous and has no binding-generation or nested-emission guard. start always returns progress after sink even if a hypothetical sink reenters close/end; such hostile sink behavior is not separately validated. Private cancellation uses worker cancellation outside this interface; cancel/get_window_id are unassigned.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.dispose | `gimp_painter_procedure_progress_dispose` | Close explicit behavior store if present; then native GObject dispose. |
| GimpProgressInterface.start | `progress_start` | Independent private sink adapter. Set active/value and start data before emitting sink; returns same progress unconditionally after sink. No parent progress state. |
| GimpProgressInterface.end | `progress_end` | Independent private sink adapter. If active, mark inactive and end data before sink. |
| GimpProgressInterface.is_active | `progress_active` | Independent private sink adapter. Active store lease returns state.active; closed/missing store returns FALSE. |
| GimpProgressInterface.set_text | `progress_text` | Independent private sink adapter. If active, update bounded data and emit sink. |
| GimpProgressInterface.set_value | `progress_value` | Independent private sink adapter. If active and finite, clamp value to [0,1], publish data then sink. |
| GimpProgressInterface.get_value | `progress_get_value` | Independent private sink adapter. Active store lease returns last stored value, even after end; closed/missing store returns 0. |
| GimpProgressInterface.pulse | `progress_pulse` | Independent private sink adapter. If active, update data and emit sink. |
| GimpProgressInterface.message | `progress_message` | Independent private sink adapter. Requires sink and valid severity (not active flag); update message and emit; FALSE if sink failure was recorded. |

### GimpPainterProvenance → GObject

Source: `app/core/gimp-painter-provenance.cpp`

- Activation: ensure/emplace/activate in instance init, not constructed; no construction properties or active-callback setup, no own constructed. GimpObject holds child through an explicit typed C seam; no late slot insertion into feature owner.
- Cleanup: Child store closes before GObject dispose; close is no-op so immutable retained values remain readable. Native containing GimpObject marks provenance_closed before disposing child and emits disconnect after that. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: Outer native owner and child are strongly leased for changes; replacements publish via swap before retired GBytes/GVariant values can run custom finalizers. Containing GimpObject marks finalizing before clearing child, and _lease rejects both finalizing reads and closed writes. Transport free zeroes public fields before user value finalizers.
- Property dispatch: No own or inherited application properties and no property adapters.
- Construct reentry: Special case: store is activated in instance init, before native parent constructed. No construction setters/signals use it and child is not handed to external code until component installation. No derived constructed callback exists.
- Limits: Init boundary has no public binding_failed flag; missing/partial store fails later boundaries. Reentry-safe ownership does not serialize or reject every nested edit; supported reads/edits observe published replacement.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.dispose | `dispose` | Find and close store under boundary first; then parent GObject dispose. Values persist until store destruction. |

### GimpPainterSession → GimpObject

Source: `app/paint/painter-mypaint-surface/gimp-painter-session.cpp`

- Activation: SessionSlot emplaced in init; construct-only options property initializes strong Options ref. parent constructed then connect settings/notify, activate, refresh. close moves core/options away before disconnect/cancel, and guarded PaintCore cancel is nonblocking/deferred when required. No own finalize.
- Cleanup: GimpObject base properties/lifecycle beyond these four slots remain inherited. PainterSession is current adapter infrastructure, not a second renderer.
- Reentry: WeakSession owner+generation gates settings-changed and native context notify; context filtering excludes resource selection names handled elsewhere. refresh sets pending and revision; busy/refreshing coalesces nested configuration. Bounded 16-iteration loop takes independent options/core/revision snapshot; publishes only matching revision; checks generation before/after error notify. Beyond 16 leaves an error and pending settings for retry. operate retains owner and independent core/options; busy/refreshing rejects recursive painting/configuration. Deferred pending refresh runs after busy clears; active notify only if generation still accepted. finish/cancel during sample/start call current PaintCore stop instead of nested operate. PaintCore defers guarded stops; cancel wins over simultaneous finish.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → require options/connect → activate → refresh; failure closes; Options are construct-only, refresh requires active store |
| GObjectClass.dispose | `dispose` | close binding → parent dispose; Retire core/options and disconnect before native teardown |
| GObjectClass.set_property | `set_property` | PROP_OPTIONS only → initialize retained options; Only construct-only option input; unknown IDs warn |
| GObjectClass.get_property | `get_property` | read options/active/error into GValue; unknown warn; Own serialization/status reads allowed through read lease |

### GimpPainterSmudge → GimpBrushCore

Source: `app/paint/gimppaintersmudge.cpp`

- Activation: SmudgeSlot emplaced in init; parent constructed then activate. begin stores strong frame resources and shared watch, starts native transaction through consumed permit, then INIT. motion_begin/step use processing and LocalCoordinates RAII. finish_frame defers while starting/processing, marks ending+inactive before FINISH, commit/cancel, scratch release. close publishes closed+cancel and defers guarded operation rollback. dispose closes before parent; no own finalize.
- Cleanup: BrushCore pre_paint, post_paint, get_paint_buffer and signal handlers inherited; PaintCore undo/finalize inherited. Own interpolate is an intentional replacement; handles_* entries are boolean capabilities.
- Reentry: buffer changed callback owns shared Watch, ignores writing and records external changes; pending callback reads atomic busy. Connections removed before frame resources retire. starting/processing/ending plus local_coordinates protect operation transitions; revision and frame_current checks detect changed buffer/format/offset/extent/attachment and reject stale publication. FINISH may reenter cancellation; commit is sealed only after FINISH and current-target validation. Existing native frame is kept alive by local owner/image/drawable/options leases.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → activate own store; Inherited construction properties must be ready before active callbacks |
| GObjectClass.dispose | `dispose` | close own BindingStore → parent dispose; Reentrant callbacks from parent teardown must see closing/closed state |
| GimpPaintCoreClass.check_start | `check_start` | validate coordinates → require owned start/nontransitioning state → consume permit → arm start; Reject unowned native transaction before native work |
| GimpPaintCoreClass.start | `start` | consume arm → validate coordinates, single drawable, byte RGB/Gray and options → parent BrushCore.start; Keep native brush setup within owned transaction |
| GimpPaintCoreClass.paint | `paint` | INIT clears accumulator/error; FINISH clears accumulator; MOTION renders own legacy smudge; Own byte renderer replaces base paint |
| GimpPaintCoreClass.interpolate | `interpolate` | validate → translate native coordinates to drawable-local → begin/step native resumable interpolation → restore coordinates; Calls native interpolation helper instead of parent vfunc to preserve old local-double arithmetic |

### GimpPainterSmudgeOptions → GimpPaintOptions

Source: `app/paint/gimppaintersmudge.cpp`

- Activation: Settings slot emplaced in instance init; constructing set_property uses initialize; constructed calls parent then activate; dispose closes the store before parent; no own finalize (qdata store final destruction). No owned signals/sources in the settings slot.
- Cleanup: Native GimpPaintOptions context/config handlers and finalization inherited. This is separate from standard GimpSmudgeOptions.
- Reentry: Unknown IDs warn; these types do not implement explicit fallback to parent get/set. Do not classify the absence of a chain as a lost inherited-property bug: normal GObject property dispatch resolves the defining property owner. The contract records this exact behavior rather than claiming the foundation fallback wording is implemented for arbitrary direct vfunc invocation.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `options_constructed` | parent constructed → activate own store; Inherited construction properties must be ready before active callbacks |
| GObjectClass.dispose | `options_dispose` | close own BindingStore → parent dispose; Reentrant callbacks from parent teardown must see closing/closed state |
| GObjectClass.set_property | `options_set` | own id → constructing initialize or active mutation; unknown id → warning; Own properties are handled here; ordinary inherited-property dispatch uses the property-defining class |
| GObjectClass.get_property | `options_get` | const typed-slot read → own output; unknown id → warning; Own values only; no synthetic parent-ID fallback |

### GimpPainterSmudgeTool → GimpBrushTool

Source: `app/tools/gimppaintersmudgetool.cpp`

- Activation: Controller slot in init; BrushTool parent constructed first (native preview core), then activate. dispose closes controller/store before parent Tool disposal may redispatch HALT. Controller close publishes closed and invalidates; revision++, source closed, connections/strokes swapped out, input reset, then disconnections, halt and cancellation. No own finalize.
- Cleanup: BrushTool cursor_update/options_notify/paint_start/end/flush, PaintTool modifier/key handling and underlying color picking are inherited. No own options property/config vfuncs.
- Reentry: with() admits only active stores, making parent-dispose HALT a safe own-state no-op; inherited parent control still runs. tick() rejects dispatch recursion and retains shared front stroke; compares revision after each callback-producing phase. committing suppresses self-generated dirty/clean invalidation; dispatching suppresses own drawable update invalidation. Idle callback captures weak owner + BindingStore generation, takes a strong owner lease, requires accepts(generation), and enters typed slot. Source handles destruction during its own dispatch; if an invalidation destroys that source and new input arrives, it rearms the newer controller source. Signal closures for target/display/image observations hold raw tool pointer, with Connection disconnection at close; these are synchronous GObject notifications, not queued weak-generation payloads. Their callbacks use active-store gate. Deferred idle is separately weak/generation validated. saving signal calls public COMMIT only when input exists and image still matches. Public gimp_tool_control always follows COMMIT by HALT (gimptool.c:656); commit_halts counts nested pairs. Watch signals include image selection/mask/dirty/clean/precision/saving/pending, drawable removed/lock/format/geometry/update, display image and shell destruction.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → activate own store; Inherited construction properties must be ready before active callbacks |
| GObjectClass.dispose | `dispose` | close own BindingStore → parent dispose; Reentrant callbacks from parent teardown must see closing/closed state |
| GimpToolClass.control | `control` | COMMIT seal current envelope + increment commit_halts + schedule; HALT consume commit_halts or invalidate; then parent control; Parent retains native tool control behavior |
| GimpToolClass.button_press | `press` | picker enabled → parent only; otherwise validate target, freeze options, enqueue stroke and schedule; Painting uses owned resumable controller; picker preserves inherited behavior |
| GimpToolClass.motion | `motion` | picker enabled → parent only; otherwise enqueue input, update preview with pause/resume; Do not start an extra native PaintTool transaction |
| GimpToolClass.button_release | `release` | picker enabled → parent only; otherwise halt control then cancel or seal/schedule input; Owned asynchronous transaction may remain after pointer release |
| GimpToolClass.oper_update | `oper_update` | invalidate pending strokes if display changed → parent oper_update; Parent BrushTool supplies native preview behavior |
| GimpDrawToolClass.draw | `draw` | reject unrepresentable off-canvas cursor/line → otherwise parent draw; Canvas preview numeric domain is narrower than accepted input |
| GimpPaintToolClass.get_outline | `outline` | reject off-canvas transformed outline → otherwise return parent outline; Preserve native outline while avoiding invalid canvas properties |

### GimpPerspectiveGuide → GObject

Source: `app/core/gimpperspectiveguide.cpp`

- Activation: Instance init registers state slot. Construct properties use initialize without changed signal. Parent constructed then activate; binding_failed closes store and factory rejects.
- Cleanup: Store closes before GObject dispose; GuideImpl.close is no-op, allowing const state reads while retained/closed. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: mutate retains guide beyond with through changed signal; state commits before notification. Property setter initializes during construction, otherwise with then emits changed. Image integration retains image/incoming guide, publishes new guide before removed, and sets disposing + clears pointer before dispose notifications.
- Property dispatch: Own IDs 1/2 = id/angle, read-write construct. Unknown IDs warn; direct parent is GObject and defines no relevant application properties.
- Construct reentry: Construct property setters use initialize and do not emit changed. They never activate. Parent constructed returns before activation. Active setter mutates then emits changed; closed setter fails without changed emission.
- Limits: No busy/generation guard prevents recursive changed handlers. Model mutation notification is synchronous and may be superseded. Property setter relies on its GObject call context for lifetime after with; public mutate has an explicit outer lease. removed class signal slot is registered but has no assigned default handler.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | Call GObject constructed before activating own registered property state. |
| GObjectClass.dispose | `dispose` | Close own store before GObject dispose. |
| GObjectClass.set_property | `set_property` | Own property IDs only; unexpected IDs warn, with no explicit parent forwarding. Normal GObject property dispatch uses the property-defining class; do not call this adapter directly with foreign parent IDs. ID/angle: constructing initialize, otherwise active mutation followed by changed; no change signal during construct property setting. |
| GObjectClass.get_property | `get_property` | Own property IDs only; unexpected IDs warn, with no explicit parent forwarding. Normal GObject property dispatch uses the property-defining class; do not call this adapter directly with foreign parent IDs. ID/angle const store reads remain possible constructing/closed while owner not finalizing. |

### GimpPerspectiveGuideTool → GimpDrawTool

Source: `app/tools/gimpperspectiveguidetool.cpp`

- Activation: ToolSlot in init, parent constructed then activate. with() active-only protects inherited dispose→HALT. dispose first cancels edit with active slot then closes; close clears weak image, guide, before and editing state. No own finalize or own async source/signal connections. Registration uses standard GIMP_TYPE_TOOL_OPTIONS, not a recreated custom GuideOptions.
- Cleanup: DrawTool has_display/has_image/key_release/active_modifier/get_popup and Tool defaults inherited. No custom options GType in current registration.
- Reentry: DrawPause balances native pause/resume; image is weak, guide/before strong, and motion/finish compare installed image guide identity before mutating/undoing. No dedicated busy, finishing or generation guard surrounds finish/model mutations. Model/image update, flush and draw-resume may synchronously redispatch callbacks. Existing guards are lifecycle admission, editing/display/guide identity and owner lease from BindingStore; this audit does not claim arbitrary recursive edit callbacks are comprehensively blocked. finish currently updates editing/changed after callback-producing image-set/undo/flush work. This is an observation and test-coverage limit, not a proven defect in this read-only audit.
- Limits: Recorded feature evidence is historical unless separately source-matched; this task does not re-execute each feature suite.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | parent constructed → activate; Tool construction precedes active own callbacks |
| GObjectClass.dispose | `dispose` | finish(cancel) while active → close store → parent dispose; Restore interrupted edit before clearing own state; parent HALT must not reopen it |
| GimpToolClass.control | `control` | HALT cancel edit, close own fields/reset operation/stop draw → parent control; Native DrawTool cleanup follows own edit cancellation |
| GimpToolClass.button_press | `press` | attach display → duplicate before state → activate edit → add/remove/hit selection; Own guide editing replaces generic tool press |
| GimpToolClass.button_release | `release` | pause drawing → finish(cancel when release cancel); Own undo/cancel transaction |
| GimpToolClass.motion | `motion` | require editing/target/guide/display and current installed guide → move point; Own guide coordinates replace generic motion |
| GimpToolClass.modifier_key | `modifier` | Shift enables Add; Ctrl enables Remove; matching release restores Move; Own operation mode only |
| GimpToolClass.oper_update | `oper_update` | if not editing attach guide/display with drawing paused; Own guide overlay association |
| GimpToolClass.cursor_update | `cursor_update` | set guide hit/operation cursor → parent cursor_update; Parent publishes chosen native cursor |
| GimpToolClass.key_press | `key_press` | Escape cancels and TRUE; other keys FALSE; Only own Escape editing command is claimed |
| GimpDrawToolClass.draw | `draw` | snapshot guide state → handles and two-point line; Parent default draw contributes no guide geometry |

### GimpPerspectiveGuideUndo → GimpUndo

Source: `app/core/gimpperspectiveguideundo.cpp`

- Activation: Instance init registers slot and records binding_failed. Construct-only guide property duplicates guide into slot through initialize. Parent constructed then activate under boundary; this path does not consume/update binding_failed or explicitly close failures.
- Cleanup: Store closes before inherited native dispose; close resets snapshot. Native Undo.free is inherited empty default, no own free override. No own finalize assignment. Native parent finalizers remain inherited; BindingStore qdata destruction closes idempotently, rejects finalizing reads, and destroys each implementation once. A close hook must not first access native parent resources at that late fallback.
- Reentry: pop commits replacement snapshot before gimp_image_set_perspective_guide emits removed/changed. Active Undo store lease pins implementation; image setter has its own retained image and publish-first/disposing guard.
- Property dispatch: Own ID 1 = guide, writable construct-only; handler only duplicates guide through initialize. Native image/undo-type/dirty-mask/name remain handled by native defining parents. Foreign direct IDs warn.
- Construct reentry: Guide snapshot property is set while UndoSlot constructing, before constructed. It may construct a separate duplicate Guide, whose own store becomes active, but does not activate this Undo. Own Undo activation occurs after parent returns; failure path is weaker as recorded.
- Limits: No nested-pop suppression or feature generation guard; only ordinary edit undo/redo and image model reentry cases are recorded. Construction failure detection is weaker than CloneUndo: factory/push does not reject binding_failed; allocation-failure behavior is not established here.

| Slot | Adapter | Parent order and contract |
| --- | --- | --- |
| GObjectClass.constructed | `constructed` | Native GimpUndo image assertion first, then attempt activate. Failure swallowed by boundary; no explicit failed-construction close. |
| GObjectClass.dispose | `dispose` | Close/release guide snapshot before native dispose. |
| GObjectClass.set_property | `set_property` | Own property IDs only; unexpected IDs warn, with no explicit parent forwarding. Normal GObject property dispatch uses the property-defining class; do not call this adapter directly with foreign parent IDs. Own construct-only guide ID 1 duplicates supplied guide; initialize only, never active mutation. |
| GimpUndoClass.pop | `pop` | Always call empty native parent then active store lease; capture current guide, duplicate saved guide, commit new snapshot, publish image replacement. |
