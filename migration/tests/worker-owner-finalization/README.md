# Original WBS 07.012: worker completion and native owner finalization

The original acceptance requires that a worker not perform the UI object's final
unref. Existing native image-close tests counted eventual destruction but did not
assert its thread. A private, unique qdata key now has a destroy callback that first requires the
exact owner GThread, then atomically increments a per-object counter. The key is
never replaced or cleared, so destruction observes native finalization. Weak
notification alone was rejected as proof because explicit dispose can invoke it
before final unref. The initial weak-observer runs are not final acceptance. Real image,
source layer and FilterLayer must each finalize once on that owner thread.
No production lifetime defect was reproduced or production code changed. The
[GLib qdata contract](https://docs.gtk.org/gobject/method.Object.set_qdata_full.html)
specifies destroy notification at finalization or replacement; this test excludes
replacement. All native parent finalizers chain synchronously.

Two existing native tests are strengthened:

- image_close_during_worker observes RUNNING, releases the actual image, requires
  image/source/filter finalization on the owner thread, and drains actual worker
  lifetimes to zero. Close retains its original nonblocking bound.
- retained_handle_after_image_close covers pre-run, vector and spill sizes. Image
  and source finalize on the owner thread while the explicitly retained filter
  remains alive and CLOSED. Worker drain plus late main-context observation must
  not change updates, cached generation, run count or completed pixels. Releasing
  the retained filter then triggers its sole finalization on the owner thread.

Native RUNNING polling does not guarantee a worker remains executing at the
instant of image release. These tests do not claim deterministic native overlap.
Complementary, unchanged scheduler tests gate actual vector and spill workers
across destruction of a plain C++ Harness, verify owned input/result/cancel state
after owner loss, and drain lifetime/admission reservations. Their Processor is
worker-owned; its worker-side destruction is not UI-finalization evidence.
The production-capture audit in independent-jobs/source-mapping.json identifies
only independent value/descriptor/progress state in worker closures, with native
owner callbacks remaining synchronous on the owner side.

Acceptance therefore combines deterministic common worker ownership, actual
native final-unref thread affinity and the current production capture mapping.
It does not substitute a scheduler Harness for a real GimpImage.

The new native-layer.json and native-sanitizers.json each execute the two actual
GIMP tests against current sealed sources. The existing sanitizer builder now
accepts repeatable --test-path so this bounded acceptance does not rerun unrelated
large-image cases; the default remains the full suite. Original build objects
are not overwritten. Its report lists exactly which C/C++ sources are
instrumented and which are RTTI-only; other GIMP/system dependencies remain
ordinary, and LSan is disabled. The unchanged independent-jobs native-scheduler
and sanitizers reports each have39 passing cases and12 current source hashes;
they are reused without another execution. Report hashes/scopes and fresh native
seals are recorded in validation.json. Historical native hashes are retained.

Reproduce using the existing normal app/tests/gimp-filter-layer target and
run_filter_native_checks.py --mode foundation. Run the existing
run_filter_layer_sanitizers.py with --test-path for each of
/gimp-filter-layer/image_close_during_worker and
/gimp-filter-layer/retained_handle_after_image_close under the shared build lock.

There are zero exact07.012 assignments in hunk, asset and source-duty inventories.
The source ledger remains byte-identical; no neighboring duty is reassigned.
All-feature job auditing, forced native interleaving, platform tests, full GIMP
instrumentation and LSan remain separate limits.
