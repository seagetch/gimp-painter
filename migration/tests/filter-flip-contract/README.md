# Original15.005 Filter flip-only legacy contract

Pinned Filter flip only reset/invalidated; neighboring scale/resize/rotate/
transform delegated to the parent. Causal native red moved x=4 to20. The fix
registers only the current flip virtual and invokes the sole FilterSlot native
invalidation path inside the existing C++ exception boundary, without a parent
pixel/geometry flip. No other transform virtual is replaced.

Four H/V direct-item and whole-image scenarios pass. Filter offset/extent stay
fixed; generations and executions advance. Ordinary lower layers still flip,
and final Filter pixels follow them through whole-image Undo/Redo. Four
neighboring virtual pointers remain identical to the native parent. Final
normal4 and selected20-unit ASan/UBSan4 include the three previously committed
pickability/definition/history probes;52 linked RTTI-only units and remaining
uninstrumented dependencies are explicit, with LSan disabled.

This clean81bc-derived checkpoint includes named prior metadata/history/resource/
pixel-preparation overlays and the Options post-construction successor. Exact
relevant source hashes are recorded; all eight held providers match81bc. It is
not latest whole HEAD, old-runtime execution, arbitrary geometry parity or UI
proof. Actual portrait before/after rendering is a separate follow-up. Original
15.004 prerequisite remains. The earlier audit map intentionally retains its
pre-fix committedbf547 provider snapshot rather than rewriting history.
