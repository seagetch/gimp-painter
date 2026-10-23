# Original 05.012: owner thread, main context and worker access

A GMainContext identifies a dispatch domain, not a permanent OS thread. The
BindingStore records its creator GThread and checks that identity; acquiring a
context on another thread does not grant access to the store. Conversely, an
owner-thread synchronous operation does not have to be inside a main-loop
iteration. Adapters must attach their sources to a context dispatched by their
owner thread and must keep that convention through shutdown.
Current GUI sources pass `nullptr` to attach to the global default context.
Pushing a thread-default context does not redirect those explicit default-context
sources. A private context is valid only with a defined compatible dispatch owner.

## Access and final-release contract

- The application's Gimp/GTK objects, stores, signals, image selection/model
  mutation, undo publication and UI callbacks belong to their native owner
  thread. Parent application startup establishes that thread. Worker execution
  does not obtain permission from holding a pointer, strong reference, weak
  reference or context reference
- ObjectRef/WeakRef implement ownership, not thread marshalling. Their resets,
  destructors and last unrefs run where called. A worker must not own a UI object
  or an owning callback capture that could deliver its final unref on that worker
- BindingStore read/with/initialize/activate/close require creator-thread access.
  find returns an address without granting permission. State/generation inspection
  is not an atomic cross-thread message channel. Finalization on another thread
  emits a diagnostic and cleans up; this fallback is not supported UI disposal
- Source destruction is reentrant-safe for its callable, but Source neither
  chooses nor checks the dispatch thread. Context attachment, priority and source
  identity do not replace the adapter's owner/generation checks. Close and capture
  destruction must stay on the intended owner when captures have that affinity
- FairDispatcher tickets, their queue mutations, cancellation and destruction are
  owner-thread operations. Scheduling checks thread identity; dispatch/cancel do
  not make the whole dispatcher thread-safe. A background producer must return
  plain result state to the owner, not manipulate its ticket
- Detached Filter/spool workers receive owned bytes, atomic cancellation/completion
  channels, mutex-protected FilterProgress snapshots and documented worker-only
  files/state. Owner preparation may fill descriptor/options until before_process
  completes and seal_input publishes them; the worker acquires the sealed state
  before processing, after which those request values are immutable. A Process
  std::function must not capture UI/Gimp owners or borrowed BindingStore state,
  even though the C++ function type cannot statically prevent it
- Worker completion publishes data before a release-store; owner polling uses the
  matching completion check, generation and lifecycle admission before importing
  or notifying. Superseded results are discarded. Closing invalidates/cancels
  without waiting for thread exit; worker cleanup does not call the UI owner
- Worker-created GIO/file/subprocess objects may be used and released by that
  worker according to their native API. An isolated Filter helper owns its own
  Gimp/PDB objects inside its process. Neither case transfers a parent GUI object
  into a worker or makes arbitrary native objects thread-safe
- Existing upstream synchronized painting is a separate native execution path.
  Its authorized worker operations on paint buffers/core state follow the native
  paint lock and lifecycle rules. This exception does not authorize GTK calls,
  custom owner-thread BindingStore access or arbitrary UI notifications there
- Native signals execute synchronously in their emitting thread unless an
  explicit dispatcher says otherwise. No common mutex is held while calling UI,
  notifying observers or invoking a user/native callback. Queue ownership and
  plain-value synchronization are distinct from GObject lifetime

## Current routes

The accompanying `cpp-main-context.json` maps current source symbols to ownership
domains, transfers, completion and close behavior. It distinguishes true detached
workers and helper processes from cooperative owner-thread jobs. In particular,
custom MyPaint, Fill/Smudge controller drains, GTK previews, deferred saves and
fair-dispatcher callbacks do not become worker-thread execution merely because
their work is asynchronous or bounded over multiple iterations.

The ordinary upstream GimpPaintTool path remains synchronized by its existing
painting machinery. Geometry/paper integration invoked from that path must stay
within the same supported native buffer/paint operations. The contract preserves
that distinction; it does not declare every GObject operation universally safe on
workers or every GObject operation universally main-thread-only.

## Native verification

`migration/tests/main-context/owner-context.cpp` creates a real GObject/store and
private GMainContext. The owner can mutate without owning that context. A worker
then dispatches a real Source while owning the context: the callback runs there,
but store read/write and the real C close entry reject it as WRONG_THREAD. The
owner's value and close count remain unchanged; final close/destruction happen
exactly once on the creator thread. No UI object or owning callback reference is
moved into the worker. A join occurs only in the test to keep borrowed observations
alive, not in a production close operation.

The fixture passes both native and ASan/UBSan runs. The recorded targeted native
owner-gate, admission, raster and spool suites test their existing worker/value
boundaries. These results establish the listed cases, not race-freedom of all
application code. LSan's previously observed ptrace restriction and the no-RTTI
vptr limitation remain explicit; no LSan or ThreadSanitizer pass is claimed.

Run `python3 tools/check_painter_main_context.py` to check the current contract,
source identities and recorded results. Original 05.012 defines allowed access;
future adapters must satisfy it. Feature responsiveness, shutdown races,
cross-platform tests and all unimplemented routes keep their own WBS gates.

## Source-mapped execution domains

### binding-store-and-handles

Owner: GThread recorded by BindingStore construction; GUI adapters install on UI thread

construct, register, initialize, activate, const read, mutation, close, generation checks on creator thread; ObjectRef/WeakRef retain/copy/lock/release follow referred-object affinity; find is lookup only

Boundary: Context acquisition does not change creator identity; synchronous creator-thread operation does not require current context ownership

Release: Owner final-unrefs; off-thread BindingStore destruction diagnoses then cleans synchronously, not marshalled recovery

Source anchors: `app/painter/binding-store.cpp:16`, `app/painter/binding-store.cpp:19`, `app/painter/binding-store.cpp:59`, `app/painter/object-ref.hpp:22`, `app/painter/object-ref.hpp:41`, `app/painter/object-ref.hpp:73`

### sources-and-fair-dispatcher

Owner: Creating owner thread and explicitly attached context

Source create/replace/close and affinity-bearing callable destruction on owner; FairDispatcher ticket creation/schedule/cancel/destruction and deque access owner-confined

Boundary: Source runs on context iterating thread; no marshalling or affinity guard. FairDispatcher enqueue checks thread, dispatch/cancel rely on contract

Release: Source pins executing callable during self-close; destroying on owner preserves capture affinity

Source anchors: `app/painter/source.hpp:27`, `app/painter/source.hpp:34`, `app/painter/source.hpp:55`, `app/painter/fair-dispatcher.hpp:27`, `app/painter/fair-dispatcher.hpp:32`, `app/painter/fair-dispatcher.hpp:57`, `app/painter/fair-dispatcher.hpp:107`

### filter-owner-adapter

Owner: GUI owner; default-context FairDispatcher

prepare dependencies and native context; sample lower-stack graph into owned packed bytes; import private staging buffer and publish current completed generation; emit progress/state notifications after reading plain channel snapshot

Boundary: Read/Import/Commit/Gate callables are ephemeral synchronous owner callbacks passed to FilterScheduler::step; never in Request/Job

Release: FilterImpl closes scheduler, tickets, connections, native context and refs on owner without join

Source anchors: `app/core/gimpfilterlayer.cpp:119`, `app/core/gimpfilterlayer.cpp:642`, `app/core/gimpfilterlayer.cpp:903`, `app/core/gimpfilterlayer.cpp:930`, `app/core/gimpfilterlayer.cpp:1059`, `app/core/gimpfilterlayer.cpp:1186`

### detached-vector-filter

Owner: Independent std::thread

compute with owned Job input/output, scalar options and approved plain-value Process captures; write success/error/output, then release-store done

Boundary: Owner acquires done before reading non-atomic worker output; generation check rejects stale completion; Process capture graph must exclude UI/Gimp/BindingStore ownership

Release: Job/Process destruction can occur on either side; owner close drops its share, worker later destroys independent state and lifetime token

Source anchors: `app/painter/filter-scheduler.cpp:24`, `app/painter/filter-scheduler.cpp:122`, `app/painter/filter-scheduler.cpp:200`, `app/painter/filter-scheduler.cpp:305`

### detached-spool-and-process-transport

Owner: Independent std::thread; TemporaryFilterRaster tied to constructing worker

own temporary files, quotas, process handles, profile and scratch; SPSC transfer of uniquely owned chunks; run process only after acquired input seal

Boundary: Owner may fill shared native descriptor/options during collection, completes before_process, then seals; no mutation after seal until prior worker completion. Progress uses mutex/try_snapshot; result uses release/acquire atomic scalar

Release: Worker closes files before complete/done publication; admission/lifetime leases outlive resources. Cancellation is a request, not completion

Source anchors: `app/painter/filter-spool.cpp:37`, `app/painter/filter-spool.cpp:65`, `app/painter/filter-spool.cpp:114`, `app/painter/filter-spool.cpp:151`, `app/painter/filter-spool.cpp:186`, `app/painter/filter-raster.cpp:56`, `app/painter/filter-process.cpp:101`, `app/core/gimpfilterlayer.cpp:1114`, `app/core/gimpfilterlayer.cpp:1186`, `app/painter/filter-progress.hpp:30`, `app/painter/filter-procedure.hpp:18`

### isolated-filter-helper

Owner: Main thread of separate gimp-painter-filter-worker process

construct PrivateRuntime and local Gimp/PDB/image/layer/buffer/progress objects; service own default context for native plugin watches; separate lifeline thread only reads pipe and kills process group

Boundary: Only validated scalar/byte wire frames cross process boundary; no parent GUI object crosses

Release: PrivateRuntime drains plugins and releases local GIMP/GEGL on helper main before terminal success; parent validates terminal/EOF/reap/cleanup

Source anchors: `app/painter-filter-worker.cpp:63`, `app/painter-filter-worker.cpp:77`, `app/painter-filter-worker.cpp:91`, `app/painter-filter-worker.cpp:118`, `app/core/gimpfilterprocedure.cpp:219`, `app/core/gimpfilterprocedure.cpp:269`, `app/core/gimpfilterprocedure.cpp:716`, `app/core/gimpfilterprocedure.cpp:872`

### mypaint-preview

Owner: GUI owner, cooperatively scheduled default-context idle

capture settings/color/resources synchronously; duplicate private GimpBrush/GimpPattern; own GeglBuffer and GdkPixbuf; step bounded preview samples and publish complete current revision

Boundary: Weak owner + store generation + preview revision guard; PreviewJob is not a detached worker and GObject snapshots are not transferable plain values

Release: Owner cancels/replaces/closes Source, PreviewJob and resource refs; running step pins current job across reentry

Source anchors: `app/widgets/gimppaintermybrusheditor.cpp:48`, `app/widgets/gimppaintermybrusheditor.cpp:83`, `app/widgets/gimppaintermybrusheditor.cpp:141`, `app/widgets/gimppaintermybrusheditor.cpp:388`, `app/paint/painter-mypaint-surface/gimp-resources.cpp:225`

### custom-paint-and-queued-fill-smudge

Owner: GUI owner; MyPaint ColorTool synchronous Session; Fill/Smudge default-context idle

MyPaint Session/PaintCore operate synchronously from custom input callbacks; Fill/Smudge freeze GObject-bearing stroke options/resources and drain bounded owner work; Undo, GEGL native resources and notifications stay in owner transaction

Boundary: Async/queued names do not imply detached execution; custom tool callbacks override native input and use owner weak/generation validation

Release: Owner cancels/finishes sessions/strokes, closes sources and releases GObject refs

Source anchors: `app/tools/gimppaintermybrushtool.cpp:43`, `app/tools/gimppaintermybrushtool.cpp:174`, `app/paint/painter-mypaint-surface/gimp-painter-session.cpp:101`, `app/paint/painter-mypaint-surface/paint-core.cpp:60`, `app/tools/gimpfillbrushtool.cpp:58`, `app/tools/gimpfillbrushtool.cpp:225`, `app/tools/gimpfillbrushtool.cpp:294`, `app/tools/gimppaintersmudgetool.cpp:225`

### upstream-ordinary-paint-exception

Owner: Existing native GimpPaintTool paint thread and owner transaction

paint worker interpolates against borrowed live native paint tool/core/options/drawables under native paint_mutex; BrushCore invokes Painter geometry, texturize and compatibility paper paste on native buffers/caches

Boundary: Upstream paint queue, paint_mutex, owner flush and PAINT_FINISH lifecycle apply; exception does not permit custom BindingStore or GTK usage from a Filter worker

Release: Native paint-end drains queued PAINT_FINISH before native finish/cancel/end-paint; BrushCore owns retained texture/cache lifetime. This audit identifies route, not complete concurrent-edit race safety

Source anchors: `app/tools/gimppainttool-paint.c:100`, `app/tools/gimppainttool-paint.c:125`, `app/tools/gimppainttool-paint.c:164`, `app/tools/gimppainttool-paint.c:211`, `app/tools/gimppainttool-paint.c:385`, `app/paint/gimpbrushcore.c:248`, `app/paint/gimpbrushcore.c:1217`, `app/paint/gimpbrushcore.c:1360`, `app/paint/gimpbrushcore.c:1451`, `app/paint/gimppainterpaper-paste.cpp:83`
