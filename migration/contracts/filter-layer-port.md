# Independent FilterLayer implementation and evidence

This is an implementation slice, not a statement that tasks 15–18 or all old
FilterLayer files are fully compatible. The old reference is gimp-painter
`afa43fae3e920210146abed514f136fd49f671b5`; the host is the checked-out GIMP 3.0
branch. Shared foundations remain `app/painter/BindingStore`, `ObjectRef`,
`Connection`, and `Source`. There is no legacy Interface/NewGClass or second
implementation store.

Current point mappings, normalized-double native precision, original Gaussian alias
editing and the 108-row source/runtime inventory are specified in
[`filter-native-execution.md`](filter-native-execution.md). Those bounded extensions
do not complete general PDB/process isolation or responsiveness acceptance.

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
  uses BindingStore; Undo/Redo swaps definition and COW committed-buffer
  snapshots, restores original pixels conservatively stale, and requests
  reevaluation from current lower content, never sharing an active runner.
  Unsupported/opaque definitions retain those original pixels without a worker.
  Retained buffer and serialized payload sizes participate in Undo accounting
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
  silently fabricated. A separate optional opaque-argument GBytes record carries
  an unavailable converted model without inspecting or executing it. Nonnull
  zero-byte records are distinct from no opaque model. Loader installation,
  duplication and definition Undo/Redo preserve it; ordinary definition/snapshot
  setters explicitly clear it. It is distinct from the complete old PROP payload.
  Ordinary GValue-array input is also capped at 65,536 total
  nodes/references, preventing exponential expansion of a compact nested DAG
- Writer snapshots enumerate every slot's type, scalar value, null/empty state,
  object-reference descriptor and nested array even after targets expire.
  Snapshots retain immutable values and IDs; liveness is queried at access time.
  The import path accepts explicit optional resolved targets and preserves
  unresolved IDs without guessing bindings. It bounds depth to 32 and the total
  slots plus reference descriptors to 65,536, and rejects inconsistent shapes,
  types, null flags or binding an expired descriptor. Import into a layer creates
  no Undo and does not rematerialize expired objects
- `GimpFilterLayerSnapshot` version 1 supplies current/committed generations,
  cache completeness and diagnostic execution state. The loader normalizes
  validated saved lineage into a fresh runtime epoch after all pixels/topology:
  a complete equal-generation cache stays clean;
  a stale/incomplete cache requests one fresh evaluation. Stored counters are
  restricted to 0..G_MAXINT64 with cache <= current. UINT64_MAX and future cache
  tokens are rejected transactionally; zero, invalidation after rejection, and
  maximum-accepted lineage → edit → snapshot → reopen are tested. Absolute saved
  counters never become runtime job tokens. Active jobs are cancelled before
  normalization. Execution states are not resumed literally
- `gimp_filter_layer_get_definition_revision()` is a separate, session-local
  same-object equality token. It changes for each definition installation,
  explicit NULL args, import, edit, Undo/Redo and duplicate installation. Cache
  restoration and lower-layer updates leave it unchanged. An XCF adapter can
  preserve unreadable saved argument metadata only while the loaded definition
  remains untouched. The token is not persisted or restored; exhaustion rejects
  further installations instead of wrapping
- Definition edits pin their owner/image through callbacks. Binding generation
  and definition revision are checked around Undo creation, attachment,
  configuration and each property notification. Closure stops publication;
  reentered newer definitions are not overwritten. All replacement fields are
  published before caller-supplied retired-byte payload destructors run.
  Duplication checks reentry before certifying the copied cache and normalizes
  the original freshness/completeness relationship instead of marking every
  copy current. Tests cover ordinary/frozen Undo dirty callbacks, undo-event
  closure, configuration/property reentry, retired-payload callbacks, and stale,
  incomplete and reentered duplicates
- Image Undo/Redo holds an operation-local image lease through stack pop and
  deferred notification thaw. Strong Undo/Redo holds the lease across every pop
  and records weak-Undo classification before callback entry, never revisiting
  a borrowed Undo pointer afterward. This fixes a reproduced fatal invalid redo
  stack when a clean callback released the last outside image reference.
  Ordinary/strong Undo and Redo each exercise clean/dirty, undo-event, and
  deferred-notify last-owner release; strong event cases also clear history.
  Explicit disposal/reentrant arbitrary history mutation beyond these cases is
  not a general upstream Undo reentrancy guarantee

## Scheduler and source boundary

`app/painter/filter-scheduler.{hpp,cpp}` is independent of GIMP and GObject.
Workers own only independent byte storage, a callable over scalar options and
atomic cancellation/completion flags. Small rasters use vectors; above one Mi
pixels the mapped Edge/Gaussian adapter uses worker-owned spill files and bounded
input/result queues. They never retain a GIMP image, drawable,
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
procedures; regional optimization is not claimed. The generic vector API retains
its 64-Mi-pixel allocation check, but mapped live Edge/Gaussian layers use spill
execution above one Mi pixels and no longer inherit that refusal. Raster extent,
64-bit file offsets, per-line legacy numerical limits, allocation and storage
failures are checked. A shared owner-thread
FIFO admission pool permits at most two active preparation/worker/import jobs
and 1 GiB of declared feature working storage. Small vector jobs reserve their
input, result and staging footprint plus worst-case Gaussian scratch. Spill jobs
reserve bounded queues/transpose bookkeeping plus 72 times their maximum axis
for IIR state; GEGL-managed input/staging/committed tiles retain the host's shared
cache/swap policy. OS filesystem cache/backing is separate. This is not a bound
on total GIMP/system memory or arbitrary callables.
Contention stays waiting without reading pixels, failing, or losing dirty work.
The existing paced dispatcher retries; FIFO prevents smaller later requests
from starving a larger front request. Preparation edits/dependency waits release
both allocation and lease (vector clear alone retained capacity). Read callbacks
receive independent bounded chunks, so reentrant invalidation cannot invalidate
their data or retain abandoned aggregate capacity. Hidden/dependency-waiting
layers discard private abandoned staging buffers as well. Closed or
cancelled workers keep their own lease until their independent storage is
actually destroyed. Owner cancellation unlinks queued entries in O(1), so
repeated edits behind a busy request cannot accumulate tombstones. UI work is dispatched
at priority 150 by one 2 ms FIFO source shared across layers and images. Each
source callback processes one bounded quantum, then rotates that ticket to the
back of the queue. Preparation starts at 1,024 pixels per changed generation;
read/import budgets are separate and adapt with 2–4 ms hysteresis between 1,024
and the absolute 32,768-pixel maximum. Native lower-node cache sampling requests fresh cache data; it never uses
the GEGL dirty-cache flag. Pixel bounds do not bound
arbitrary GEGL operator cost or cold format setup. Default-priority input can
preempt between quanta rather than
waiting behind a batch of ready per-layer timers. The dispatcher contains no
GObject or implementation store; adapters carry weak generation tokens. Lower visible FilterLayers must settle
first; child groups are traversed and completion/failure signals wake waiters.
Above-layer pixel changes and this layer's own cache publication do not restart
it. Topology snapshots avoid restarting merely because an unrelated layer was
added above. Hidden FilterLayers retain dirty work until shown.

For spill jobs, admission starts an independent I/O worker to collect bounded
chunks. Procedure start is counted separately, only after a complete sealed
snapshot is flushed. The owner never performs spill file I/O or waits on the
worker. Each direction has two queue slots; current producer/consumer chunks are
also bounded. The directory is the expanded current GEGL swap setting copied
before launch, never an XCF argument or an implicit `/tmp`. Cancellation retains
the job/lease until actual completion, including input collection and output
draining. Result chunks may be imported into private staging while the worker
reads them, but only a successful entire transfer permits publication. Malformed
input keeps the dispatch continuation alive until collecting-worker cancellation
completes and releases admission; the regression follows the actual returned
continuation flag instead of unconditionally polling a settled state. A later
read failure discards staging and keeps the previous cache. Private raster files
are destroyed before completion is visible. See `filter-storage.md` and the
kernel evidence for component resource bounds and platform limitations.

Cold graph preparation first snapshots active stack nodes, including layers above
the Filter and children before group parents. It constructs at most one cold
layer node per dispatcher quantum, holds an owned endpoint/image across the
vfunc, and returns before reconsidering any callback-mutated snapshot. Planning
uses no resolving Clone getter. Removal, definition replacement and image/binding
closure during construction are tested, as is later resolution to a Filter cycle.
The host eagerly initializes twelve Babl conversion fishes during its first
layer-mode preparation. The adapter primes those same immutable cache entries,
in the host's order, for default/native image spaces in timed owner-thread steps.
This does not evaluate pixels, change color conversion or move GObjects to a
worker. A single Babl call or custom node can still exceed the 2 ms target.
Read-only graph/read timing maxima separate setup from subsequent GEGL sampling;
the whole-step scope also includes state-change observers, early returns and local
destructors, with a deliberately slow observer regression. All maxima are
cumulative observations, not acceptance thresholds.

The input is read from this layer's input proxy in the already-built parent
stack graph. Preparation happens outside operator evaluation. All input reads,
GObject calls and GeglBuffer updates remain on the owner thread. The current
GimpDrawable buffer is replaced only after the result has been fully imported.
GEGL/pickable evaluation merely reads that committed cache and never starts or
waits for a job. The previous completed cache can remain displayed while a newer
generation is pending; upper FilterLayers cannot consume it as current input.
After a guarded completed-buffer swap and drawable invalidation, completion also
sends `gimp_image_flush()`. That schedules the host's asynchronous projection
chunks without waiting. Drawable invalidation alone left the native canvas
showing an initially transparent Replace cache until another user action. The
regression pre-renders that cache, verifies both cache and image projection after
completion without a test-side flush, and tests image-close reentry from flush.

A generation change during preparation, worker execution or staged import
invalidates the old work. A cancelled worker must finish before a replacement
starts. Close detaches the UI immediately and lets the independent worker finish
without any callback into a dead layer. Errors and dependency cycles (including
CloneLayer references back to a FilterLayer) stop the same-generation retry loop.
Cycle inspection now uses an iterative unique-node walk with one 4,096-node
budget for the entire reachable graph, including Clone references into other
images. Successful topology validation is cached. Weak endpoint/source identity,
Filter state and generation are checked each quantum; content/topology edits,
source reassignment/expiration and owner format/profile changes invalidate it.
Clone inspection uses owned nonresolving reference snapshots; saved pending
names cannot execute callbacks in borrowed traversal. If a changed reference is
detected without a signal, invalidation occurs after iterator/snapshot scopes
end and revalidation waits until the next dispatcher quantum. Tests resolve a
pending name explicitly with remove/close callbacks and close the owner from
fallback invalidation. Graph construction and completed input reads recheck
nonresolving dependency snapshots before work can launch. A test resolves a
pending name into a self-cycle from a layer get-node hook, both with ordinary
notifications and suppressed Clone update signals: no worker starts and the
cycle fails without recursive GEGL evaluation. A failed traversal disconnects
its partial subscriptions, avoiding signal cycles
in a malformed cross-image graph. Twenty-layer chains coalesce already-waiting
dependency notifications rather than amplifying a single edit through every DAG
path. Cross-image pending dependencies, an explicitly closed dependency with no
state signal, reassignment/expiration and edit-after-cycle failure are tested.

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

`filter-gauss.{hpp,cpp}` ports canonical `plug-in-gauss` with both IIR and
integer RLE methods, the old radius-to-sigma conversion, per-pass alpha rounding,
small-radius RLE fallback and the RLE endpoint asymmetry. Its 104 genuine PDB
captures cover the bundled test2 preset's 25/25/method-0 call. The actual
FilterLayer integration additionally compares both methods byte-for-byte on
an opaque captured input. Four additional explicit names are supported:
`plug-in-gauss-iir`, `plug-in-gauss-rle`, `plug-in-gauss-iir2`, and
`plug-in-gauss-rle2`. The single-radius forms retain their original integer flags
(including noncanonical truthy values); both disabled flags are a successful
identity when the radius is positive. The two-radius forms retain five argument
slots and use a fixed method. Conversion never rewrites the saved name, argument
types/order/values or original bytes. No arbitrary PDB lookup is introduced.

The new real-old corpus exposed a previously incorrect negative-axis assumption:
`1 + ceil(radius)` can shrink the disabled axis. A nonempty remaining region
leaves an unwritten transparent shadow border, so original RGB survives while
border alpha becomes zero. A zero/negative remaining region writes no shadow;
on a fresh drawable the old PDB returns success with identical input pixels,
while logging its missing-shadow assertion. The port preserves these pixels
without reproducing that internal diagnostic. Overflowing old signed-int region
arithmetic is rejected safely. These are whole, unselected RGBA/Gray-alpha
semantics, not a claim about selections or stale external shadow storage.

`legacy-gauss-alias` contributes 88 genuine output buffers and eight calling-error
probes; `legacy-gauss-negative` contributes 112 more genuine buffers. All seven
input load/export checks preserve native bytes. Both vector and file-backed
kernels compare all 200 buffers; the actual GIMP typed adapter also checks every
byte plus definition preservation. Failed argument shapes/numerical limits keep
the prior cache without idle retries, and both successful identity aliases run
through the live spill transport above one Mi pixels.

`migration/fixtures/legacy-edge/` contains 76 genuine old-PDB input/output cases,
all compared byte-for-byte by `painter-filter-edge`. See its README and hashed
capture report for the exact sources, executable and tested parameter matrix.
This proves the compatibility executor on those RGBA8 cases; it does not alone
prove the entire GIMP 3 lower-stack projection equals the old projection.

## Encoded color and execution admission

Input sampling and result import use RGBA8 or Gray-alpha8 in the drawable's
native Babl color space. An unqualified sRGB format was incorrect: the Adobe RGB opaque Gaussian
fixture differed in 208 of 288 bytes before this correction. Both Gaussian
methods now match all reference bytes under sRGB and Adobe RGB. Profile and
format changes invalidate prepared/running/staged work. Reassigning profiles
while a worker is pending discards that generation and publishes a fresh result.
These are targeted native-encoding transfer tests against genuine old reference
bytes; the captures themselves were unprofiled, not a new old-ICC capture.

Exact old-byte execution admits non-linear RGB and Gray U8. Native Gray input is sampled as
`Y'A u8`, then Y is replicated across the channel-independent byte kernel;
result import extracts Y and alpha without an RGB-to-Gray color conversion.
`migration/fixtures/legacy-gray-filter/` contains 77 genuine old-PDB Gray and
Gray-alpha outputs: all detectors/borders, both Gaussian methods, degenerate
sizes, tile boundaries, hidden Gray, low alpha and plain Gray without alpha.
Every byte matches the independent executors. Actual GIMP tests compare opaque
reference pixels under default, custom linear and custom Lab Gray profiles,
and with a source without alpha. This establishes native channel transfer, not
all lower-stack composition or an old profiled-image oracle.

Unsupported reruns explicitly fail while retaining definitions and committed
cache; loading an existing completed cache does not discard it or attempt
quantization. The old edge and Gaussian registrations accept RGB*/GRAY*, not
indexed, and both actual indexed probes fail as recorded in the Gray corpus.
High precision and linear/perceptual storage use the explicit modern extension
in `filter-native-execution.md`; they have no 2.8 byte-level oracle.

Stored procedure names and string arguments from XCF are untrusted input.
Execution must stay an explicit allowlist of side-effect-free transformation
mappings or use an independently reviewed isolation boundary. Looking up an
arbitrary existing PDB/script/eval/file procedure and invoking it while opening
an image would permit code execution or file writes. Unsupported definitions
and cache are retained with a visible failure state; arbitrary dispatch is not
an acceptable shortcut to procedure compatibility.

## Tests and measurements

- `app/painter/tests/test-filter-scheduler.cpp`: 34 pure scheduler tests including
  cancellation versus completion, no duplicate launch, edits during preparation
  and import, exception handling/no automatic retry, bounded chunks, loaded cache,
  dependency priority, nonwaiting destruction, read/import rejection, inert closed
  requests and commit reentry
- `app/tests/test-gimp-filter-layer.c`: 78 real-GIMP cases as of this record,
  including cache publication, chain/group ordering, cycle recovery, visibility,
  offset, removal/Undo, definition Undo/Redo, raw unknown data, weak-finalization
  counters for object-valued arguments and Undo, signal teardown and failed-duplicate temporary release
- `app/tests/test-gimp-filter-layout.cpp`: actual C/C++ layout comparison and
  named owning handle construction in that same test executable
- `app/painter/tests/test-filter-gray.cpp`: 77 native Gray/Gray-alpha byte
  comparisons against the actual old executable; `run_filter_gray_sanitizers.py`
  independently instruments both kernels and this corpus, including float-cast
  overflow checks
- `migration/tests/filter-layer-testlog.{txt,json}`: normal Meson results
- `migration/tests/run_filter_layer_sanitizers.py` and
  `filter-layer-sanitizers.json`: reproducible focused ASan/UBSan run. Private
  thin archives replace instrumented members without editing production archives.
  Instrumented C++ units use consistent RTTI for UBSan's vptr checks; remaining
  upstream/dependency units are uninstrumented. LeakSanitizer is disabled because
  ptrace restrictions prevent it from running; explicit weak-finalization tests
  cover the image/layer/argument ownership cycles. The JSON lists actual units
- `run_filter_scheduler_sanitizers.py` and `filter-scheduler-sanitizers.json`
  independently instrument all 34 scheduler regressions with ASan/UBSan
- `work-admission.hpp`, `test-work-admission.cpp`, and
  `run_work_admission_sanitizers.py`: nine strict-C++14 and ASan/UBSan cases
  cover FIFO/byte/job limits, cancellation, 100,000 queued edits, worker-thread
  release, owner destruction without waiting, move ownership and thread guards.
  Four scheduler cases exercise admission through real preparation/cancellation/
  import lifecycle; an actual GIMP case queues a third image, coalesces edits,
  closes a preparing image and verifies the queued latest generation completes
- `fair-dispatcher.hpp`, `test-fair-dispatcher.cpp`, and
  `run_fair_dispatcher_sanitizers.py`: eight FIFO/input-priority/reentry/teardown
  regressions, independently passing strict C++14 and ASan/UBSan

Actual GIMP fairness/teardown cases additionally check that a 16×16 image
finishes while a 2048×1536 image is still preparing input, twenty repeated lower
edits converge to the final measured Sobel pixel, and image/source/layer objects
are finalized immediately when closed during a Gaussian worker. Retained public
Filter handles are also tested with queued/running work: image disconnect closes
the binding and prevents later publication, while the common GimpItem weak-image
link makes subsequent image lookup/finalization safe. A buffer-notify callback
that closes the image during the final swap is also covered: no later drawable
update or state notification is emitted for the closed binding. An 8193×8193
layer now completes one real spill-backed Edge run, preserves its definition and
verifies both a transparent first pixel and an opaque far-edge result. Generated
1025×1025 RGB Adobe/Gray Lab-profile images compare every byte against the
existing vector Gaussian route for both methods. These are live transfer tests,
not new old-runtime captures. Retained handles and image teardown also exercise
running spill workers. Streamed image projection refresh, twenty repeated source
edits, and configured-directory failure/cache retention followed by explicit
retry are tested as well. `settled()` stays false while a failed/rejected worker
is still completing cancellation.

`run_filter_spill_measurements.py` / `filter-spill-measurements.json` retain
the cold-graph checkpoint’s exact-source historical observations, before alias
adapter changes. They record three fresh-process 8193×8193 runs against the exact live-adapter source/executable
hashes. Configured test swap is on overlayfs. All runs complete and verify the
far-corner pixel, taking 12.40–12.82 seconds of filter work; the 2 ms heartbeat
p95 is 2.21–2.25 ms and p99 is 3.09–4.25 ms. Maximum intervals are
23.1–31.6 ms with cooperative cold preparation (previously 89–119 ms).
Whole-process peak RSS is 1,393,368–1,393,928 KiB (about 1.33 GiB),
including resident GIMP/GEGL tiles and other host allocations, and excluding OS
filesystem cache/backing. Bounded worker queues/files therefore do not certify
low total application/system memory. Final image unref takes 92.4–106.7 ms in
these completed-image cases: nonwaiting worker cancellation does not bound host
GEGL/cache destruction cost. These are shared-host observations of a sparse
single-layer Edge workload, not dense/complex/multi-image or Windows acceptance.

Earlier vector-only measurements before adaptive/spill integration for a
simple 2048×1536 single-layer run were roughly 0.81–0.85 s end-to-end,
8.9–9.7 ms maximum owner-thread quantum and 364–367 serviced 2 ms heartbeats in
normal tests. These are observations on this execution environment, not a fixed
reference-machine p95/p99 acceptance gate.

`run_filter_latency_measurements.py` / `filter-layer-latency.json` likewise retain
the cold-graph checkpoint’s exact-source historical observations of the actual
1024×1024, 64-partially-opaque-layer workload in three fresh GIMP processes,
recording first use and an edit in the same graph. The report records source
hashes, reproducibility-only environment, wall times, cumulative Filter quantum
maximum and per-phase 2 ms heartbeat p50/p95/p99/max intervals. Host load is
uncontrolled. Earlier exploratory fixed-budget runs reached about 40 ms warm
quanta; cache-enabled adaptive sampling reduced one comparable warm observation
to 9.94 ms (1.40 s total versus 0.67 s fixed-budget), but outliers remained.
Earlier cold layer-node construction showed 80–102 ms stalls. Cooperative node
planning and Babl cache preparation now measure 25.2–29.0 ms maximum graph/setup
quanta in the three fresh processes, but individual Babl calls remain synchronous
and later GEGL read quanta reach 54.8 ms. The scope timer includes callbacks and
local destructors; graph/read maxima report separate subspans.
The report retains every quantified first/edit phase for its exact source and
executable hashes. Repeated measurements still show heartbeat outliers above
100 ms, including edit phases; the current three-run first/edit samples reach
129.7 ms maximum and 24.1 ms p99 in edit phases. These observations are not a
fixed-machine acceptance gate. More complex
operators, many images and sustained painting still need workloads and limits.

## Configured spill-space boundary

The trusted config-owned memory/job/spill pool is now implemented and tested;
see `filter-spill-admission.md`. Byte Edge/point/identity reserves `8*w*h` logical
file bytes and vertical Gaussian `12*w*h`; the double extension uses `64*w*h`
and `96*w*h`. Temporary contention queues fairly, an oversized request fails
once, and a reservation remains held until worker files close. The independent
worker checks available filesystem space and actual I/O/flush failures. These
are logical per-application reservations, not OS-exclusive space or total
GIMP/GEGL/system memory/disk guarantees.

## Remaining work and non-claims

- Nine literal names have explicit compatibility executors: Edge, canonical
  Gaussian, four Gaussian aliases and three point operations. Other names retain
  definitions/cache and report unsupported execution. This is not generic PDB
  compatibility
- The common-store Filter editor now edits original Gaussian flag/two-radius
  forms and the point mappings. Unknown or unsupported shapes stay on Keep;
  choosing another procedure remains an explicit definition replacement
- The source/runtime inventory has 108 rows, with 99 execution routes still
  unaccepted. Source-eligible scripts require runtime/context review. The general
  plug-in process adapter, crash handling and unresponsive-procedure isolation
  remain outstanding
- The old reader's image-ID/GValue-pointer crash is a negative fixture; safely
  retaining those bytes does not turn it into a successful legacy round trip
- Normal Open/XCF persistence and creation/menu/editor UI are validated by their
  separate modules/evidence. This core slice supplies typed argument and
  generation primitives; these core tests alone are not save/reopen or GTK proof.
  A GimpProgress adapter remains separate
- Whole lower-stack parity (especially custom modes, masks, component visibility,
  group/passthrough behavior and high precision), sustained input,
  dense/complex spill-backed large rasters, measured high-contention multi-image
  workloads, maximum topology traversal cost and reference-machine latency percentiles remain broader compatibility
  gates. FIFO owner-thread fairness is tested; arbitrary worker/process workloads
  and exhaustive maximum-size/platform resource behavior remain outstanding
- The baseline `save-and-export` test failure is unrelated; this record does not
  claim the entire upstream app suite is green
