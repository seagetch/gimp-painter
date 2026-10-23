# Bounded drawable/projection invalidation coordinates

Status: DONE for this bounded unit, 2026-10-16.
Task: `09.002/projection-update-bounds`.
Receipt: `migration/tests/projection-update-bounds.json`.

## Scope

This unit makes the translated drawable/stack update and projection invalidation
queue/paint notification path use defined arithmetic at extreme signed integer
coordinates. It preserves ordinary positive-area update coverage, including the
existing grid padding before intersection. It does not claim that every GIMP
coordinate API accepts arbitrarily placed or sized images.

The prior path could add a drawable-local origin to its item offset in `gint`,
add update width/height before converting to floating point, narrow an expanded
grid endpoint outside `gint`, or subtract projectable offsets in `gint`. An
otherwise separate FilterLayer completed-buffer swap can reach this path when
it emits its normal drawable update. The fix keeps those normal notifications;
it does not suppress them in tests.

## Defined interval contract

- Translation computes both endpoints in `gint64`, then clips to the endpoint
  range representable by the existing signed-int update signal.
- Nonpositive extents remain empty. Stack callers retain their notification
  behavior with empty representable geometry. Unlike the previous rounding
  path, off-grid nonpositive projection intervals cannot create phantom dirty
  cells; this is an intentional safe definition for invalid/empty geometry.
- Grid floor/ceil use mathematical rounding for negative as well as positive
  coordinates, with checked wide intermediates.
- Positive update intervals are clipped against the bounds' covering grid
  cells before alignment. The aligned result is then intersected with bounds
  and the native endpoint range. This preserves the old align-before-clip
  padding even for an update just outside a non-grid-aligned image edge.
- Native coordinates and widths are narrowed only after representability is
  established. Projection queue and paint intersection use the same bounded
  interval rules; offset subtraction and notification translation stay wide.

## Accepted validation

`update-geometry` passes five cases covering independent ordinary-domain equivalence, translated
and aligned boundary intervals, negative rounding, nonpositive sizes, and wide
inputs. The equivalence case checks 4,767,100 positive intervals against an
independent expression of the old grid expansion at grids 1 and 32. The internal
header is included twice and registered for C/C++ header checks.

`projection-update-bounds` passes all three paths using real top-level drawable, layer stack and image
projection notifications. It covers grid padding, extreme offsets, partial
local updates, active changes, backdrop/range invalidation and cleanup without
blocking update handlers. A synthetic GimpProjectable provider also exercises
real projection offset subtraction and paint notification at extreme
coordinates, using small native buffers. Neither establishes arbitrary
group/show-all or huge-buffer allocation safety. These tests have no
dependency on Sobel runtime fixtures.

The default native build and all 12 selected registrations passed: 313 native
TAP cases, 27 Python controls and 403 header compile probes (192 C, 211 C++).
The targeted run includes the complete existing FilterLayer, Clone, XCF Open
and XCF roundtrip regressions, plus core, layer-mode and ancestry checks. There
were no skips, runtime crashes or unexpected diagnostics. The XCF roundtrip
suite retained its 295 expected malformed/retained-metadata warnings; this was
not a warning-free run and no new diagnostic exemption was introduced.

The focused ASan/UBSan run passes the same five pure and three native paths.
Exactly five C translation units were instrumented:

- `app/core/gimpdrawablestack.c`
- `app/core/gimplayerstack.c`
- `app/core/gimpprojection.c`
- `app/tests/test-update-geometry.c`
- `app/tests/test-projection-update-bounds.c`

The inline helper is instrumented through those callers. Other GIMP code,
dependencies and C++ remain uninstrumented, and leak detection is disabled.
All 921 selected source/dependency inputs, 43 ordinary build inputs and two
private executables remained sealed unchanged. Every linked alias of a replaced
object used the private archive, and ordinary build outputs were not replaced.
The separate Python-control source is sealed by the normal run, not included
among the sanitizer compile dependencies. Its ten controls cover exact TAP and
diagnostic rejection, lock ownership, private output/dependency rewriting,
option preservation, failure/timeout receipts and changed-input rejection.

The normal run sealed eleven code/test/registration files and the then-current
contract draft, plus three named ordinary executables. This is a bounded
snapshot, not a whole-tree source/output seal. Only this contract, the public
receipt and task/progress documentation changed after execution. The eleven
tested code/test/registration hashes remain identical to the accepted overlay.

## Relationship to frozen aggregate acceptance

The prior strict 160-target Linux aggregate remains tied to exact source commit
`7463d5dc65fa74832ab7f8219aae71dc83d6b8c7`, tree
`735b6abdd2b0cb0ddade6148fe46d9dd18c63b3e`, as recorded in
`migration/tests/frozen-linux-7463d5.json`. It does not cover this later geometry
change. This unit was tested as an eleven-file overlay on
`6055380c28cf2aab5f2003b9220b26700c01136d`, which records that earlier aggregate;
its acceptance is the targeted normal and focused sanitizer evidence above.

## Boundaries

The real application cases cover top-level image projections. Helper arithmetic
checks do not by themselves establish extreme group/show-all geometry support.
Priority rectangle changes, general pixel queries, projection bounds-change
allocation/copy paths, and unrelated image geometry operations are outside this
unit. The separate host-coordinate input fix and Sobel owner/XCF changes in
`gimpfilterlayer.cpp` are not included in this overlay or its acceptance.
Full-program sanitizers, leak checking, installed-runtime acceptance,
Windows/macOS execution and the full legacy UI contract remain separate gates.
