# Original constant MyPaint mode entry

Verified: missing-symbol link red, normal6, bounded4-unit sanitizer6 and
42 header routes/88 C/C++ probes pass. Four mutated evidence controls reject.

The pinned original validates GIMP_IS_MYPAINT_OPTIONS then returns NORMAL,
regardless of stored brush-mode. See the public-operation contract for the exact
signature, enum values, old diagnostics and remaining unmatched operations.
Tests cover all four property values without mutation/notifications, disposed
valid instance behavior, invalid NULL/wrong-type diagnostics and an actual C11
consumer. Typed get_brush_mode delegates directly to the same C entry.

The bounded sanitizer covers options.cpp, binding-store.cpp and two fixtures;
no whole-app/dependency/RTTI/vptr/LSan coverage is claimed. Portrait authenticated
Xvfb1080x1920 plus oncanvas preference assertion is model-only validation, not
rendered UI evidence. This pure getter changes no UI; GUI screenshots remain the
separately committed fa08b201e8 evidence.

Two initial setup failures (missing C++ widgets-types and C GLib prerequisites)
are preserved under /workspace/shared/options-mode-entry/fixture-*-failure.
They are not causal semantic red and do not count as successful tests. Final selected sources are an explicit mode overlay on exact committed20b; all
eight held providers and four Clone providers match20b. The separate root
aggregate21targets238cases plus Filter4 and graphUI3 passed before this overlay,
not evidence that this candidate is a whole-HEAD aggregate. No held candidate
is copied. Post-test input hashes are unchanged. Publication remains paused.
