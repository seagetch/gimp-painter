# Original23.002: hard-mask format boundary and center update matrix

The independently forwarded native center fixture found a real format boundary:
old byte hard masks became native `Y float`, while Smudge still indexed their raw
storage as one byte per pixel. On a solid17px mask, old accumulator center130 was
correctly computed, but raw float bytes supplied coverage0, leaving drawable220.
Private diagnostic tracing established those separate stages and was restored;
tracing is not the acceptance implementation. The final expected-red uses original
committed production without instrumentation and the exact final fixture.

The11-line fix normalizes non-byte masks into a Smudge-owned byte buffer after
paper. Paper explicitly accepts byte/float and preserves its input format; hard
binary0/1 with constant paper128 gives the exact old byte coverage128. Existing
byte/subsample paths do not convert. No BrushCore API, old accumulation/blending
formula, source geometry or transaction policy is changed.

## Native oracle scope

192 scenes: Y/YA/RGB/RGBA × interior/edge/outside/negative first contacts ×
rate0/50/100 × solid17/18px masks × paper disabled/128. The observed paint vfunc
forwards every actual call unchanged; all first zero-opacity dabs preserve the
whole drawable, and first successful overlap seeds once. Fully outside first
contact explicitly records no seed, then entry succeeds. Pressure mapping is
turned off before visible motions so each observed update has genuine opacity1;
there is no interpolation-pressure tolerance. The fixture asserts native hard
mask `Y float`, exact dimensions and center values.1008 real visible centers are
compared to unchanged pinned old blend/replace/opaque-compose functions; full
Undo/Redo bytes are also compared. No center is silently skipped when out of range.

Alpha-format initial pixels are opaque. The old alpha replacement coverage is
255²/65536, not a mathematically exact overwrite; the old function computes it.
This is not full partial-alpha, textured-raster or device-interpolation parity.
The previously captured old48-scene routes and1024-vector fixture remain separate
regressions; no new old runtime was run. Original52 duties remain open.

## Evidence boundaries

Final normal/focused results and all exact source hashes are sealed in `final/`.
The10 held providers are committed private baseline bytes, including Options
blob12fd73dc406128bc94167ee3444caa05cbdd9a9d at ce40ac4cff, not latest main Options.
No held acquisition/lifetime/extreme diagnostic ran. This is a selective private
build, not an exact current-HEAD aggregate. Focused sanitizers retain ordinary
ASan/UBSan/float-cast-overflow; remaining host/dependencies are uninstrumented and
LSan is disabled. No raw environment JSON is packaged.

Diagnostic compile errors (missing standard includes), preliminary observer
failures and restored private production traces are preserved separately. The
initial observed hard-mask red was not attributed to accumulation: the corrected
old-composition oracle and final identical-fixture causal red/green are normative.
No UI widgets or input bindings changed, and no new UI screenshot is claimed.

## Executed final results

Normal registered25 native cases (1.407s), old nonpaper48-scene raw/owned routes
(1.329/1.271s),1024-vector kernels (0.054s) all PASS. Normal paper raw/owned
48-scene routes pass exact2,952,800 bytes/193records (1.225/1.166s). Focused
18-unit ASan/UBSan/float-cast-overflow passes the25 cases and all four48-scene
routes; separately instrumented1024 vectors match891,487 bytes. All192 matrix
scenes/1008 observed centers pass both native executions.26 new evidence controls,
4 exact successor-admission controls and22 predecessor controls reject mutations.

The71-input archive was sealed before final builds. Expected-red `source.json` is
a retrospective reconstruction from that archive with only production changed to
original21e9163fd1; the same-fixture identity is checked, not falsely described as
a separately captured pre-build manifest. Diagnostic6/7 source manifests describe
the pre-instrumentation baseline; exact tracing edits are in retained scripts.
Neither trace is used as final source/binary acceptance proof. Final ordinary
production has no tracing. Native test display is authenticated1080×1920x24.

## Exact alpha-center successor admission (2026-10-21)

The separate `smudge-alpha-centers` checkpoint adds partial/transparent alpha
center scenes without production changes. The exact source admission now pins
that package's current entry source and checker; all historical runtime/archive
bytes remain unchanged. Original25/24-case results are not relabeled26-case runs.
Arbitrary future edits are rejected. Original52 duties remain open.
