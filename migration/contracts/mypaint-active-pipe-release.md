# Active MyPaint pipes and release tails

## Observed old behavior

The independent capture links the pinned `afa43fae3e920210146abed514f136fd49f671b5`
application archives. It uses real GimpMypaintCore, native GimpBrushPipe selection,
full nonlinear-u8 RGBA drawable pixels and the actual Undo/Redo stack. No port
renderer or evaluator is linked into the old capture. Thirty-two synthetic scenes
cover all eight selector modes, with and without shape-aware smudge sampling, incremental and nonincremental.
The fixture contains 416 complete 64×48 RGBA images, 416 native state records,
1,920 selector observations, 32 Undo results and 32 subsequent global RNG values.

A class-vfunc observer forwards unmodified arguments to the original native
selector and returns its result. It does not change indices, masks or random
state. Each new old Surface is warmed using a constant selector; the observer
never reads its undefined first last_coords. The old core has static storage so
its otherwise uninitialized initial options pointer is zero-initialized. The
per-scene seed is external test setup, never production segment setup. The first attempted nonincremental drawable switch reproduced the known old
cold drawable-feature crash; its separate failed capture is preserved. Successful
scenes warm each new drawable incrementally before enabling nonincremental. Old
build logs, behavior-source and archive hashes, link command, runtime diagnostics
and complete pixels are preserved beside the fixture.

Explicit finish, repeated strokes, mode/input-axis changes, resource A→B→A,
a defined warmed drawable switch and Undo/Redo retain native pipe index/current
child and RNG state. The built-in old and current selectors ignore last_coords:
angular/velocity/pressure/tilt use current input axes; incremental uses the native
index; random consumes the process-global stream. The old Surface carries its
previous coordinates across logical boundaries, whereas transient port resource
providers reset them. That difference is retained in the raw trace and explicitly
reported by the comparator. It does not alter these built-in selector results.
No speculative coordinate owner or new BindingStore route was added.

## Demonstrated defect and correction

The first old/port replay agreed through explicit finish/restart, then diverged
at the first hover following a press. The evaluator interpolates from preceding
positive pressure to the incoming zero pressure. In this stimulus it emitted
13 positive-pressure tail dabs during that sample. The port's unconditional
sampling-only draw rejection discarded those dabs and their native selections,
ended its logical stroke earlier, and changed subsequent incremental/random
selection and pixels. The before-fix run is retained in
`migration/tests/mypaint-pipe-before-fix.*`.

`HoverSurface` now forwards a dab only while an existing native paint segment
is active and the evaluator's interpolated STATE_PRESSURE is positive. It uses
the existing Surface, resource owner and transaction. Sampling-only transient
hover cannot create a transaction. At zero pressure every dab remains suppressed,
including for a constant-opacity brush. Finish/cancel retires the Surface, so a
later hover cannot resurrect residual evaluator pressure. All incoming hover
coordinates, including the ones passed to the native selector, still have zero
pressure; tilt/direction/velocity are retained. This matches the old selector's
actual inputs during the positive-pressure interpolated tail.

There is no pipe cloning, index restoration, per-segment reseeding or extra
Undo snapshot. Preview keeps its private deep copy and private RNG. Generic
atomic operations retain the existing Session and snapshot implementation.

## Acceptance and limits

The comparator checks exact full pixels, selector counts, current axes, native
index/current-child, Undo/Redo and RNG records. Its only exclusion is the seven
previous-coordinate values and their validity flag in selector observations;
all excluded differences are counted. Raw old observations remain available.
An active built-in pipe therefore has a stronger oracle than a new-only direct
selector test. The undefined old cold coordinate bytes are never an oracle.

The native hover tests separately cover true cold/settled hover, default mouse
pressure, explicit constant opacity, cumulative and nonincremental release tails,
finish/cancel, no empty Undo, resource callback reentry and image release. The
registered GTK tool test covers real release dispatch, pending-tail XCF Save,
reload of exact native bytes, retained pipe state across Save and Undo/Redo,
and settled hover. Native 8-bit RGBA/RGB/Gray oracle and safe Gray-extension,
preview isolation, Session and generic atomic-batch suites remain regression
requirements.

This does not establish custom brush subclass semantics that inspect previous
coordinates, old undefined cold state, arbitrary ICC/high precision, physical
tablet events, all resource geometries or other platforms. The old capture uses
native core boundaries; actual XCF Save is exercised by the port GTK test, not
claimed as an old GUI capture. Constant-opacity pure-hover painting in the old
code is deliberately not reproduced. Test reports record exact source hashes,
normal/sanitizer results and the focused instrumentation scope.

## Frozen acceptance checkpoint

`migration/tests/mypaint-pipe-checkpoint.json` and `mypaint-pipe-allowlist.json`
identify the bounded change and authoritative evidence. Normal headless acceptance
passes all nine suites: the new 2,817-record pipe oracle, original RGBA129/RGB385/
Gray193 oracles, Gray safe extension, Session, seven hover groups and five preview
groups. Thirteen standalone fixture/provenance checks pass.

Focused ASan/UBSan/float-cast-overflow with RTTI/vptr passes the pipe comparison
(27 instrumented +32 RTTI-only sources), hover (26+32), Clipboard/preview (30+32),
and native GTK tool (38+30). LSan is disabled and unlisted dependencies remain
uninstrumented. Final frozen native and sanitized GTK executables each pass all
15 groups, including actual release, image replacement, Save/reload and Undo/Redo.
Native HALT/COMMIT can synthesize a timed release; final assertions deliberately
use the committed pixels, with retained/nondecreasing prior coverage, because a
stationary tail may or may not alter an already saturated pixel.

The tool test was recompiled once its obsolete pre-release pixel expectation was
corrected; retained production objects are unchanged and tied to the source
archive. `mypaint-pipe-tested-sources.tar.gz` contains the exact 101 tested source/
header inputs, including a hash-verified prior ordinary-geometry RTTI dependency
that another worker edited after compilation. Later unrelated working-tree code
is not silently substituted into that archive.

### Separate unresolved native empty-display observation

One canonical normal GUI attempt failed at registration-case teardown, reporting
`gimp_image_get_resolution: GIMP_IS_IMAGE (image)` while closing the last image.
No stack was captured on that first failure. A diagnostic binary with a critical-
log backtrace handler then passed all 15 groups without reproducing it, as did the
final unchanged canonical and ASan binaries. The failure and diagnostic source/log
are preserved. This is an unresolved, intermittent empty-display lifecycle
observation, not a diagnosed renderer defect or a reason to suppress warnings.
The tests retain `gimp_display_close()` and empty-shell reuse; their lifecycle
coverage was not replaced with display deletion. A dedicated native empty-shell /
borrowed-options regression should investigate it independently.
