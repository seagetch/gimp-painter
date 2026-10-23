# Immutable evaluator checkpoint — prerequisite, not stroke Undo completion

The original core deletes its Stroke at split without pushing the proposed Undo.
Original Stroke.start leaves state/settings capture TODO; stop accumulates an
uninitialized timing member; render uses the borrowed live Brush. The original
Undo has an empty pop and declares a pointer using g_param_spec_object. Reusing
those behaviors or aliasing the native generic pixel Undo cannot meet the task's
stronger requirement that pixels AND stroke state round-trip with one destruction.
The paired native Undo/controller work remains open.

This child adds Engine::Checkpoint, an immutable shared snapshot whose contents
are deep value copies of the evaluator. state-audit.json enumerates every field:
all states (including random seed), mappings/points and cached current values,
speed cache, reset flag, timing/debug values, GRand owner, and Engine opacity/
nonincremental properties. A copied checkpoint shares only frozen data. Restore
clones before swap, so allocation failure leaves the live engine unchanged. The
source engine may be destroyed; no Surface, live Resource or controller is held.
The initial settings-value cache is zero-initialized to avoid reading indeterminate
floats when capturing before the first sample. This does not change the pinned
numerical trace. Current evaluator re-seeds GRand from the state per input sample;
both owner and seed are copied, and the lost-seed control is behaviorally tested.

Capture/restore is only valid between complete input samples. Recursive capture,
restore and stroke evaluation from a Surface callback are rejected, not interpreted
as usable half-executed snapshots. The transient processing flag is never captured
as an active state. Empty snapshots are rejected.

Nine native groups and the same nine under selected three-unit ASan/UBSan prove
initial/midstroke random replay, changed settings, pending reset, source-owner
release, independent copies, empty rejection, callback reentry and allocation
failure at three clone/copy points. Allocation controls explicitly wrap operator
new in this test executable, not production allocation policy. Lost random seed
and lost settings native replacement-object mutants fail the original byte-trace
comparison. A fixture first expected only the equal-byte assertion rather than
the earlier equal-length assertion; its failure is retained separately.

Original resource9 groups pass. Exact historical engine trace: 5,637,911 bytes,
33,768 records, 177 brushes plus synthetic, 27,392 dabs and 3,172 samples. Exact
decoder trace: 1,125,707 bytes and 81,420 records. Initial decoder execution failed
because its binary was not built; that receipt is retained and the built rerun
passes. No fixture, numeric policy, reference trace or precision expectation was
changed. C++14/noRTTI first/double Engine header and current423 general header
probes pass. General probes do not themselves cover Engine; the dedicated compile
receipt does.

No UI behavior is changed and no new UI image is claimed. Dependencies are not
fully instrumented; vptr and LSan are disabled. The clean historical private build
contains committed owned overlays, not concurrent blocked candidate sources or a
whole-branch integration proof. Original09.011/mypaint-undo-stroke,09.017 and the
original GimpMypaintCoreUndo type/property/ownership work stay open. No raw old
Stroke pointer is accepted and no binary ABI restoration is claimed.

Run `python3 migration/tests/engine-checkpoint-09011/validate.py --check-source`.
