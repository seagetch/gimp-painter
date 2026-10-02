# Atomic generic extended MyPaint strokes

The three native entrypoints in `gimppaintcore-stroke.c` now dispatch Painter
MyPaint to its existing extended Session. Direct coordinates, interpolated path
subpaths and sorted boundary loops share a small immutable C segment descriptor.
Standard cores keep their prior execution path. Future owned Fill/Smudge
backends can reuse this preparation rather than creating another renderer.

`GimpPainterPaintGate` is now a C++ GObject implementation with exactly one typed
BindingStore slot and a named `PainterPaintGateRef`. Its operation owns the
existing `GimpPainterSession`, target and image. There is no outer PaintCore
transaction or second extended engine. Direct low-level starts still fail in
`check_start`, before native allocations/coordinate mutation. Generic operation
admission rejects reentry; pending-paint queries remain true until completion or
actual rollback, including when a callback explicitly disposes the adapter.

The controller's opt-in batch mode holds one native snapshot/freeze across the
whole operation. Evaluator splits and disconnected subpaths retire independent
Surface segments, so nonincremental composition uses each segment's starting
pixels while native rollback retains the initial operation snapshot. Success
commits once with the requested Undo policy/name; failure cancels the initial
snapshot. No Undo/Redo stack surgery is used. Interactive callers retain their
existing logical split/Undo behavior. Settings changes abort an immutable batch,
then the Session reconciles the current valid model for a later operation.

Prepared coordinates remain image-space until the adapter subtracts the target
offset once. All device axes are copied. Each disconnected subpath resets the
extended evaluator. Timing follows the current GIMP 3 generic MyPaint policy:
zero-pressure initialization, 15ms first draw and 0.5ms per pixel thereafter.
This synthetic timing policy is distinct from old interactive input parity.
It does not delegate rendering to upstream libmypaint.

Path preparation retains the target/image/core/options/path and an owned snapshot of
its stroke list while virtual interpolation runs. Coordinate arrays transfer
ownership into the prepared list. A real native test caught the difference
between `g_array_ref()` and `g_array_free()`: the latter clears the segment even
when another wrapper reference exists. The corrected transfer preserves data
until dispatch. Failure restores the adapter's public coordinate bookkeeping.
Invalid/nonfinite/unrepresentable core input is rejected before native start
converts coordinates into integer extents.

## Verified scope

Thirteen synthetic native GIMP groups cover:

- Separate logical segments under one native freeze/snapshot/Undo, with offset
  selection and nonincremental pixels equal to independent controller segments
- Later input failure preserving preexisting Undo and Redo and their real pixels
- `push_undo=false`, zero-opacity no-op and the pinned locked-alpha floating no-op
- Callback cancel that cannot subsequently commit, plus successful reuse
- Settings-change abort and later valid operation recovery
- Actual raw stroke API dispatch, input immutability, Undo name and final coords
- Actual raw path and boundary dispatch, multiple subpaths and real Undo/Redo
- A second-subpath settings failure rolling back the first subpath as well
- Nested raw dispatch rejection, pending-paint preflight and explicit disposal
- Raw no-Undo behavior with existing history, and empty-path errors with NULL
  error storage
- Invalid first input without any native preview freeze or pixel/Undo mutation

Two added groups drop the caller’s last Options reference from real virtual
path interpolation (success and empty-path failure) and compare every public
start/current/last coordinate axis against genuine native fresh/reused cores.
Native `start_coords` intentionally remembers the previous endpoint for Undo.

The same thirteen groups passed focused ASan/UBSan/float-cast-overflow with vptr
enabled: 28 instrumented sources plus 24 RTTI-only compatibility sources. LeakSanitizer is disabled. The interactive Session8/Hover6 suites and
independent old RGBA129/RGB385 record comparisons still pass. The recorded scope
is native API testing, not a physical device or separately launched PDB client.
The common stroke entrypoints are used by PDB/Stroke Path callers; transport and
GUI aggregate acceptance remain separate verification.

## Reproduction and instrumentation

The model, tool and editor builders use `painter_sanitizer_scope.py` and replace
every private archive alias. It now conservatively includes all production app
C++ units explicitly built with `-fno-rtti`, because polymorphic Surface/support
code also supplies RTTI to instrumented callers. Extra units get `-frtti` only,
are listed separately, and are not claimed as sanitizer-instrumented. The initial eleven-group report retains its earlier nine-unit RTTI subset.
The final thirteen-group run uses the expanded C++ suffix-aware discovery.
A separate refresh rebuilt only a concurrently changed RTTI-only BrushCore
loops unit, recreated private archive indexes, relinked, and repeated all
thirteen cases. It required all instrumented sources and recorded ABI headers
to remain identical; the rejected predecessor report is retained. No vptr
checks are disabled and no production object is replaced. Exact unit lists,
source/executable hashes and runtime diagnostics accompany the reports.
The tested-source archive seals all compiled units plus three native ABI
headers, including dependencies being developed in parallel. An inherited
shutdown data-folder diagnostic is retained verbatim in runtime reports.

All future build/test commands use `/workspace/shared/gimp-painter-build.lock`;
the earlier `/tmp` lock was not shared with the native desktop namespace. Native
profile handoffs remain necessary in addition to that lock.

Gray/Gray+alpha, higher precision policy, arbitrary ICC, full active brush-pipe
parity and physical tablet/platform acceptance remain separate renderer gates.

## Owned generic entrypoints follow-on

Raw Stroke, native Path and Boundary now use the same existing Fill/Smudge owned
transactions across all subpaths. See `owned-generic-stroking.md` for the scoped
implementation, independent old entrypoint fixtures and exact checkpoint gates.
