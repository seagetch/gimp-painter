# Original WBS 06.024: operation-scoped resolution

The original condition is that pixel loops use already-resolved borrowed state.
The current adapters already satisfy this granularity; no per-pixel lookup defect
or performance improvement is claimed. This acceptance fixes the contract and
adds executable regression checks at the actual store and slot resolution sites.
There are no historical source-ledger duties assigned directly to 06.024, and no
source-duty assignments or statuses are changed for this task.

## Coverage and results

- `common.json`: six standalone cases, all pass
- `sanitizers.json`: the same six cases under ASan/UBSan, all pass; LSan is off
- `native.json`: two real MyPaint/GEGL cases, six Meson observation cases and the
  unchanged 59-case foundation suite, all exit zero
- `BOUNDARIES.md`: source review of MyPaint, Fill, Smudge, Clone, Filter and Painter
  GEGL mode entry points and reachable pixel kernels; 50 exact source hashes in
  `scope-files.json`. The complementary production textual census has 29 files
  and 327 matching lines. A textual occurrence is not a runtime call count

The observer is generated from the unchanged production `binding-store.cpp`.
It counts `find()` invocations and actual `entry()` searches separately. Exact
single-occurrence anchors fail if the implementation moves. Only test executables
link the observation copy; production gains no instrumentation, counter API or
per-pixel overhead. Native compile and link commands are recorded in `native.json`.

The common cases vary 1, 257 and 1,048,576 pixels over one or seven regions. Both
counts equal the region count, with exact work checksums. Deliberately bad
controls yield (64,64) for repeated store/slot resolution and (1,64) for a cached
store with per-pixel slot searches. Additional cases cover nested operations,
loss of the caller's last owner reference, close/generation invalidation and
wrong-thread rejection. The sanitizer result covers these common mechanisms,
not the full native application.

Real `gimp_painter_session_stroke_to()` is measured after pressure-zero warmup,
with a separate pixel baseline for the measured inputs. One input touches 182
pixels in the small fixture and 15,494 (u8) / 15,536 (double) in the large fixture;
both use two store finds and three slot searches. Eight inputs use 16 finds and
24 searches while touching 332 versus 16,863 / 16,909 pixels. Tests assert these
counts, increasing touched area, visible output and exact native Undo restoration.
Thus the actual adapter scales with input operations rather than their pixels.

Real `GeglSurface::draw_dab/get_color` executes eight dabs and samples over 256
and 65,536 pixels in nonlinear RGBA-u8 and linear RGBA-double. Both counters stay
zero. Exact written bytes grow from 8,192 to 2,097,152 (u8) and 65,536 to 16,777,216
(double); read volume is twice that. These copy-volume measurements are separate
from lookup counts. Output changes and cancellation restores the original bytes.

## Reproduction

For the common observer, with GObject development headers and C++14 available:

```sh
python3 tools/test_painter_qdata.py --build-dir /tmp/painter-qdata --report /tmp/painter-qdata.json
python3 tools/test_painter_qdata.py --sanitize --build-dir /tmp/painter-qdata-asan --report /tmp/painter-qdata-asan.json
```

In a configured GIMP build, compile the `app/painter/painter-qdata-operations` and
`app/tests/gimp-painter-qdata-operations` targets, then run the matching Meson tests.
The native target's Meson environment points to the synthetic app test profile;
no user image, personal brush library or desktop is used. The captured Linux
build uses GCC 14.2.0, GLib 2.84.4, GTK 3.24.49, GEGL 0.4.62 and babl 0.1.114.
No GUI/display is required for these tests.

## Limits

The native fixture exits zero but emits the existing test-profile writable data
folder diagnostic during teardown. This result does not certify application
packaging or profile-save configuration. It also does not certify Windows/macOS,
tablet behavior, all-feature numerical parity, full-app sanitizers, or unrelated
upstream GObject data keys. Fill/Smudge dabs and steps, Filter dependency checks
and callback reentry intentionally admit separate operations; hoisting raw Impl
borrows across them would weaken lifecycle checks. Existing unrelated historical
foundation report hashes remain unchanged and are not current PASS evidence.
