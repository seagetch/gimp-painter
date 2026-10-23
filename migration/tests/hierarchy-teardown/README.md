# Original WBS 06.027: hierarchy teardown

The original condition is that derived/base/store close and parent dispose do
not double-release state. Existing common ownership and native chaining already
implement that contract. This task strengthens exact lifetime assertions and
closes its one legacy source duty; it does not claim a new production defect.

## Actual native paths

The C fixture registers real Base and Child GTypes. Both dispose callbacks close
the same store and chain to their fixed immediate parent. Native instance init
registers base before child, so logical close visits child then base.

- `parent-dispose-reentry` now checks exactly two parent entries after one nested
  disposal, three after a second explicit disposal, and four after final unref.
  Each slot closes once, generation advances once, and neither Impl is destroyed
  during any parent dispose callback
- `shutdown-paths` covers base-only and child objects, each with and without an
  explicit store close before repeated native disposal. Per-tag close and
  destruction counts are one; all parent calls can still read closed state
- Test-only C finalize hooks observe the child/base/GObject finalize chain.
  Impls remain alive before native parent finalization and each is released by
  the store's qdata destruction. No order between Impl destructors is required
- Existing store close-transitions, implementation-ownership, call-leases and
  finalizing-close-read cases cover the complementary closing/closed guards,
  construction fallback and owner-reference loss

The store and production adapters are unchanged. Parent dispose may legitimately
run more than once; idempotence applies to slot close and memory release, not to
the required native parent chain.

## Legacy mapping

Only `legacy-1ff25c784d3e86f0c831`, hunk `01.002/000045`, is assigned to this task.
The old `app/base/glib-cxx-impl.hpp` blob is
`a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`; its already archived exact bytes are
in [inherited-properties/legacy-source](../inherited-properties/legacy-source/glib-cxx-impl.hpp).
Old NewGClass placement-constructs its private Impl and explicitly destroys it
before calling the saved parent finalize. It has no independent close/dispose
state machine. The modern store separates reverse logical close from one final
release. `source-mapping.json` identifies both mechanisms without imposing the
old private-memory layout or destructor ordering on the replacement.

## Reproduction and limits

```sh
python3 tools/test_painter_foundation.py --build-dir /tmp/painter-teardown --report /tmp/painter-teardown.json
python3 tools/test_painter_foundation.py --sanitize --build-dir /tmp/painter-teardown-asan --report /tmp/painter-teardown-asan.json
```

Native and ASan/UBSan reports each contain 64 passing cases and exact source
hashes. Meson separately recompiles the changed C/C++ fixture, links and passes
64 cases. LSan is disabled. This accepts the original common lifecycle contract;
it does not claim all-feature, whole-GIMP, platform or all asynchronous-job
acceptance. Historical evidence retains its original source seals. The current
row and old snapshot mismatches are distinguished in `validation.json`.
