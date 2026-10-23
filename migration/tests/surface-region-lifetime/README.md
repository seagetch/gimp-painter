# 09.003/region-lifetime — scoped buffer release

Original criterion: `draw_dab と get_color の全 return・失敗経路で read/write/snapshot/mask/texture の buffer を一度だけ解放する`.

No production change was required. This seals the existing RAII implementation with observable internal-buffer finalization, in addition to the existing 11 lifetime tests. The dedicated GNU ELF test wraps real `gegl_buffer_new/dup` return values solely to install weak-finalization observers. It never fabricates GLib allocation failure or changes application linkage.

## Ownership/exit review

- `Impl::target`, `initial`, `floating`, `selection` are `ObjectRef<GeglBuffer>`; `Surface` owns `Impl` via `unique_ptr`. Borrowed target/mask/snapshot are retained; replacing an identical selection retains before release. Session end releases snapshot/floating; active destruction releases all remaining owned buffers. Repeated end/cancel is harmless.
- Source, write and original/result byte regions, transformed brush coverage and selected/wide-selected masks are local `std::vector` objects. All normal returns and C++ exceptions destroy them; there is no manual `g_free` path in the replacement. `coverage` retains a local provider and returned `shared_ptr<const Texture>`, so replacement and exceptional/early returns cannot strand texture ownership. Dirty notification is copied before mutation and remains owned through self-replacement.
- Reviewed draw exits: inactive/nonfinite rejection; zero opacity; coverage failure/empty target or selection intersection; wide no-accumulation; wide/byte success; provider, copied observer and invoked observer exceptions. Reviewed sampling exits: null outputs; coverage errors/empty intersection; wide success, including no alpha/weight; byte success. Nested pixel/read/vector operations also unwind through the same lexical owners, not separate manual cleanup.
- Tests use byte and double formats, both accumulation modes, success/end/cancel/destructor, rejected/duplicate session snapshots, shape/texture invalid/throwing providers, zero opacity/radius/outside target/selection, NaN input, null sample outputs, no-op wide paint, provider replacement, observer copy failure, callback throw/recovery and repeated sessions. Finalization observers count exactly once for real target/mask/snapshot/floating buffers. Temporary texture deleters and provider captures are independently counted. Vectors are verified by lexical ownership review and ASan/UBSan, not an invented per-allocation counter.

`ownership-map.json` preserves all 14 original sites verbatim at pinned `afa43fae`: 12 free sites and two factory returns. The image factory ownership role maps to PaintCore's `shared_ptr<GeglSurface>` (not unique_ptr); old preview Surface factory semantics are **not** certified. Preview/API migration remains under 08.018 and 31.017. Every inherited whole-header duty remains TODO in the three original manifests, with snapshot, work allocation, Undo, commit/cancel and other successors unchanged.

## Evidence and limits

`results.json`: independent standalone normal14 and ASan/UBSan/float-cast14, original60-second budgets, source closure hashes and binary identities. Only compiled Surface/test translation units are instrumented; GLib/GEGL/Babl are not. LSan disabled; no whole-application leak claim, forced OOM, thread-race or exhaustive pixel-format claim.

`native.json` and `native.log`: registered original11 plus dedicated14, authenticated portrait Xvfb1080×1920×24. This is non-UI ownership testing, not a new screenshot claim. Native logging originally omitted an absent optional stderr field; the first extraction failure did not change passing results. Final corrected extraction uses an empty default. `excluded-missing-test-include.log` records a test-only missing `<limits>` compile setup failure before correction, not a passing run.

Run `python3 migration/tests/surface-region-lifetime/validate.py --check-source` and `python3 migration/tests/surface-region-lifetime/test_validate.py`. Runtime reproducer requires pinned environment: `source /workspace/shared/gimp-native-baseline/env.sh; python3 migration/tests/surface-region-lifetime/run-standalone.py --output OUT`. Exact tested local headers and sources are archived; no inherited environment dictionaries are packaged.
