# Independent FilterLayer implementation and evidence

This is an implementation slice, not a statement that tasks 15–18 or all old
FilterLayer files are fully compatible. The old reference is gimp-painter
`afa43fae3e920210146abed514f136fd49f671b5`; the host is the checked-out GIMP 3.0
branch. Shared foundations remain `app/painter/BindingStore`, `ObjectRef`,
`Connection`, and `Source`. There is no legacy Interface/NewGClass or second
implementation store.

## Actual integration

- `app/core/gimpfilterlayer.{h,cpp}` registers a real `GimpLayer` subclass with
  a single typed `FilterSlot`. Ordinary mode, opacity, mask and offset machinery
  remains the host layer machinery. The pickable opacity query preserves the
  legacy zero result; pixel content is locked while definition editing is allowed
- `gimpfilterlayer-handle.hpp` is the named public owning C++ handle. C functions
  expose typed GObject pointers; exceptions are contained at C/vfunc boundaries
- `gimp_filter_layer_set_definition()` is the no-Undo loader/model setter.
  `gimp_filter_layer_edit_definition()` records a dedicated
  `GIMP_UNDO_FILTER_LAYER_DEFINITION` entry. Its own GObject implementation also
  uses BindingStore; Undo/Redo swaps immutable definition snapshots and requests
  reevaluation from current lower content, never sharing an active runner
- The procedure string, original serialized bytes, converted argument model,
  current generation and committed-cache generation are separate records.
  `filter-procedure`, `filter-arguments` and `filter-original-definition` are
  read-only properties notified by the setter/Undo path
- Original serialized bytes are the complete old PROP_FILTER_SPEC payload,
  including its original name, ordered arguments, terminator and unknown tail.
  The execution mapping does not normalize or replace that byte record
- `gimpfilterlayer-arguments.hpp` owns scalar/string/known numeric-array values.
  Persistent image/drawable/GObject arguments and object arrays instead retain
  weak links plus original type and core ID descriptors. The descriptor survives
  target expiration; temporary getter copies are made only on the owner thread.
  An expired object array is reported unavailable instead of returning dangling
  pointers. Nested value arrays are bounded and recursively converted. Opaque
  boxed/pointer arguments are rejected transactionally; raw-only definitions
  remain supported for unknown conversions. Unknown executable values are not
  silently fabricated

## Scheduler and source boundary

`app/painter/filter-scheduler.{hpp,cpp}` is independent of GIMP and GObject.
Workers own only independent byte vectors, a callable over scalar options and
atomic cancellation/completion flags. They never retain a GIMP image, drawable,
GEGL graph, UI object, slot borrow or main-thread callback.

| State | Checkpoint and transition |
|---|---|
| clean | Complete cache published or loaded cache accepted; no dirty work |
| waiting | Dirty work waits for current lower dependencies or attachment |
| preparing | Read at most 32,768 RGBA8 pixels per owner-thread quantum |
| running | Exactly one worker owns the prepared snapshot for that generation |
| cancelling | Cancellation requested; worker completion is still outstanding |
| importing | Copy at most 32,768 result pixels into a private staging buffer |
| failed | Preserve definition/cache and stop retries until a new edit |
| closed | Invalidate generation, destroy sources/connections and request cancellation; never join |

Changes coalesce to a full-raster dirty generation rather than accumulating an
unbounded list of dirty rectangles. This is correct for the supported whole-image
procedures; regional optimization is not claimed. Requests over 64 Mi pixels are
reported unsupported instead of overflowing allocations. UI work is dispatched
at priority 150 in 2 ms timer quanta. Lower visible FilterLayers must settle
first; child groups are traversed and completion/failure signals wake waiters.
Above-layer pixel changes and this layer's own cache publication do not restart
it. Topology snapshots avoid restarting merely because an unrelated layer was
added above. Hidden FilterLayers retain dirty work until shown.

The input is read from this layer's input proxy in the already-built parent
stack graph. Preparation happens outside operator evaluation. All input reads,
GObject calls and GeglBuffer updates remain on the owner thread. The current
GimpDrawable buffer is replaced only after the result has been fully imported.
GEGL/pickable evaluation merely reads that committed cache and never starts or
waits for a job. The previous completed cache can remain displayed while a newer
generation is pending; upper FilterLayers cannot consume it as current input.

A generation change during preparation, worker execution or staged import
invalidates the old work. A cancelled worker must finish before a replacement
starts. Close detaches the UI immediately and lets the independent worker finish
without any callback into a dead layer. Errors and dependency cycles (including
CloneLayer references back to a FilterLayer) stop the same-generation retry loop.
Cycle inspection is bounded to 4,096 visited nodes per dependency traversal.

## Confirmed execution route and PDB audit

GIMP 3 no longer registers `plug-in-edge`. The source audit found:

- `app/pdb/gimpprocedure.c:gimp_procedure_real_execute_async()` executes an
  internal procedure synchronously; calling it does not establish responsiveness
- `app/plug-in/gimppluginprocedure.c:gimp_plug_in_procedure_execute_async()` uses
  the plug-in manager with `synchronous=FALSE`; ordinary separate plug-in
  processes run asynchronously, but persistent procedures enter a nested loop in
  `gimppluginmanager-call.c` while waiting for extension acknowledgement
- `gimpplugin-message.c:gimp_plug_in_handle_proc_return()` records returned values
  and closes the plug-in, with asynchronous error handling on that path
- `gimpplugin.c:gimp_plug_in_close(TRUE)` sleeps and waits/terminates an external
  process. It is not suitable as this layer's nonwaiting owner-thread close

Accordingly the first supported route is a separate CPU compatibility executor,
not a synchronous internal PDB call, hidden nested main loop, forced in-process
thread kill, or standard GEGL nondestructive filter/effect.

`filter-edge.{hpp,cpp}` ports the old detector and relevant shadow-merge result
semantics. It supports all six algorithms, all three border modes, five-argument
Sobel default and six-argument form. It preserves the old minimum amount clamp,
integer truncation, channel arithmetic and alpha-zero hidden RGB behavior.

`migration/fixtures/legacy-edge/` contains 76 genuine old-PDB input/output cases,
all compared byte-for-byte by `painter-filter-edge`. See its README and hashed
capture report for the exact sources, executable and tested parameter matrix.
This proves the compatibility executor on those RGBA8 cases; it does not alone
prove the entire GIMP 3 lower-stack projection equals the old projection.

## Tests and measurements

- `app/painter/tests/test-filter-scheduler.cpp`: 14 pure scheduler tests including
  cancellation versus completion, no duplicate launch, edits during preparation
  and import, exception handling/no automatic retry, bounded chunks, loaded cache,
  dependency priority, nonwaiting destruction, read/import rejection, inert closed
  requests and commit reentry
- `app/tests/test-gimp-filter-layer.c`: 20 real-GIMP cases as of this record,
  including cache publication, chain/group ordering, cycle recovery, visibility,
  offset, removal/Undo, definition Undo/Redo, raw unknown data, weak-finalization
  counters for object-valued arguments and Undo, signal teardown and failed-duplicate temporary release
- `app/tests/test-gimp-filter-layout.cpp`: actual C/C++ layout comparison and
  named owning handle construction in that same test executable
- `migration/tests/filter-layer-testlog.{txt,json}`: normal Meson results
- `migration/tests/run_filter_layer_sanitizers.py` and
  `filter-layer-sanitizers.json`: reproducible focused ASan/UBSan run. Private
  thin archives replace instrumented members without editing production archives.
  Instrumented C++ units use consistent RTTI for UBSan's vptr checks; remaining
  upstream/dependency units are uninstrumented. LeakSanitizer is disabled because
  ptrace restrictions prevent it from running; explicit weak-finalization tests
  cover the image/layer/argument ownership cycles. The JSON lists actual units
- `run_filter_scheduler_sanitizers.py` and `filter-scheduler-sanitizers.json`
  independently instrument all 14 scheduler regressions with ASan/UBSan

A simple 2048×1536 single-layer run measured roughly 0.81–0.85 s end-to-end,
8.9–9.7 ms maximum owner-thread quantum and 364–367 serviced 2 ms heartbeats in
normal tests. These are observations on this execution environment, not a fixed
reference-machine p95/p99 acceptance gate. More complex lower graphs, many
images and sustained painting still need their own workloads and thresholds.

## Remaining work and non-claims

- `plug-in-gauss` is also present in the old `test2.json` preset. Its exact
  executor/fixture port is being implemented separately; do not infer support
  until its integration and parity tests land. Other legacy PDB procedures still
  retain their definitions but report unsupported execution
- A complete mapping inventory beyond bundled presets, plug-in process adapter,
  crash handling and unresponsive external-procedure isolation are outstanding
- The old reader's image-ID/GValue-pointer crash is a negative fixture; safely
  retaining those bytes does not turn it into a successful legacy round trip
- Normal Open/XCF persistence integration and definition/cache save-generation
  reconciliation are separate changes. Core tests are not save/reopen proof
- There is no new creation/menu/editor UI or GimpProgress adapter in this slice
- Whole lower-stack parity (especially custom modes, masks, component visibility,
  group/passthrough behavior, high precision and indexed inputs), sustained input,
  cross-image fairness, maximum topology traversal cost and reference-machine
  latency percentiles remain broader compatibility gates
- The baseline `save-and-export` test failure is unrelated; this record does not
  claim the entire upstream app suite is green
