# Resumable native brush interpolation

The native GIMP 3 BrushCore interpolation algorithm now has a numerical
continuation object. The original paint-core vfunc is a synchronous adapter over
that same implementation; other tools keep their existing behavior. Fill's GUI
uses `gimp_fill_brush_motion_begin()` and advances it only through owner-thread
`step()` calls.

## Contract

- Admission copies one full GimpCoords event and its timestamp. It does not
  subdivide, clamp, smooth again, expand dabs, or publish pixels.
- The caller drains the previous event before admitting another. The Fill
  controller retains future raw events in input order and freezes its stroke
  options/resources before admission. Direct synchronous Fill callers retain the
  original API and its per-motion setting semantics.
- One Fill step either advances at most one native interpolation dab or searches
  at most the supplied candidate budget. Zero budget advances neither operation.
- Native stripe selection, exceptions A/B/C, exact-integer avoidance, spacing,
  dynamics, RNG draw order, pressure/axis formulae, distance and endpoint state
  remain shared with the synchronous path. There is no second interpolation
  implementation or alternative attached implementation store.
- A continuation contains numerical values only. Its caller retains and
  serializes the core, drawable and immutable options. Fill keeps a local
  continuation lease through callbacks and tests revision/cancellation before
  proceeding. Closing drops pending numerical state without expanding it.
- Candidate counts are signed 64-bit values. Nonfinite or unrepresentable count
  calculations fail before conversion. This is a numerical representation check,
  not an input/dab cap or silent geometry truncation. Stripe and cumulative
  distance arithmetic no longer relies on narrowing 32-bit casts.

## Evidence and remaining gates

`migration/fixtures/native-brush-interpolation` was captured from the pre-refactor
native executable before modifying BrushCore. Its 180 scenarios contain 8,883
records / 4,655,000 bytes. Synchronous and pause budgets 1, 2, 7, 31 and 4096 must
be byte-identical. This is a GIMP 3 numerical preservation oracle, not evidence
that GIMP 3's interpolation matches every old GIMP 2.8 input policy.

The independent old Fill fixture still compares all 12 stroke scenarios and all
36 finish/Undo/Redo snapshots, in synchronous per-motion, synchronous fully queued
and resumable modes. A separate test prepares 49,999,998,300 native candidates,
emits exactly one, then cancels; it never allocates that many dabs. Lifecycle
cases also run through resumable admission.

This structural bound does not establish a total latency or memory bound.
Projection flushing/snapshot creation, brush resource duplication, one mask's
creation, GEGL I/O/publication, frontier memory, raw input queue storage and
cleanup remain independently measured/admission-controlled work. Brush-pipe
state across cloned strokes and full old Shift/smoothing input equivalence also
remain separate compatibility gates. No arbitrary input cap or dropped event is
introduced here.
