# Independent legacy blending dynamics

The appended GIMP_DYNAMICS_OUTPUT_BLENDING has legacy value11 and nick
`blending`, without renumbering the existing eleven output types. GimpDynamics
owns the `blending-output` aggregate using the same native curve/config model as
its other outputs. It is independent of Flow. The native Dynamics editor lists
output types from that enum; dedicated Smudge tool integration is a later gate.

Five native and five-unit ASan/UBSan tests cover default identity, pressure
mapping without changing Flow, exact serialized curve/resource roundtrip,
independent copied output edits, the legacy property spelling, and editing an
empty curve. Comparing the whole GimpData object is not a valid roundtrip oracle
because unrelated per-instance identity differs; serialized properties and the
complete nested output model are compared instead.

Instrumentation exposed upstream GimpCurve passing null pointers to zero-length
memcpy/memcmp while adding/removing the first/last point or comparing empty
curves. These calls are now skipped when their lengths are zero. The empty-curve
case runs with the curve unit instrumented, rather than disabling the sanitizer.
Other GIMP/dependency units and physical GUI input are outside this test scope;
LeakSanitizer is disabled.
