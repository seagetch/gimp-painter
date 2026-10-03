# Native Blinds context owner acceptance

This leaf is `16.003/native-blinds-context`. It integrates only the legacy
Blinds selection, component, own-alpha-lock and background phases. It does not
accept another procedure family, general PDB dispatch, global latency, arbitrary
sizes, GUI progress, disabled-swap execution or other platforms.

`acceptance.json` records exact source/binary identities, test cases, measured
limits and remaining gates. Large build outputs and duplicated source archives
are intentionally excluded. The committed compact old archives/manifests are
verified before every fixture run; no expected bytes come from this port.

The owner is part of the existing common BindingStore FilterSlot. GEGL reads
only the completed drawable cache. Input, start selection bounds/background and
final selection/components are separate phases. The private helper returns raw
shadow, including zero unwritten pixels outside the start ROI. Current final
coverage/components/own alpha lock are sealed once before bounded import.
Selection edits alone do not increment the Filter input generation. XCF's
replacement selection is handled by an immediate image invalidation signal and
identity checks at both gates. Cancellation, a replaced lower-input generation
or owner close discards unpublished captures/results.

The independent tests cover 240 native YA/RGBA merge records (every active mask,
four selections and own/synthetic ancestor locks),24 original live records and 6
expansion records. The actual Blinds process compares 60 complete genuine-old
results at zero/nonzero offsets; the offset cases also replace the selection
object in the same order as XCF. Additional 48 GrayA limiting-case scenes exercise
actual offset ROI and component/lock execution. These analytic checks remain
separate from the genuine-old oracle counts.

Lifecycle tests check selection changes during bounded preparation/final capture,
selection/component/lock changes after an exact known-empty seal and across
multiple import chunks, lower-input replacement, cancellation, image closure,
atomic publication, cached-only GEGL evaluation and oversized admission failure.
The complete-before-done spool race is tested using a generated observation copy
of the real spool; production code contains no test hook.

Reproduce using the restored build dependency environment, a task-local build,
and the same shared lock. Substitute paths for a different checkout:

```sh
source /workspace/scratch/5b5281e79681/gimp-build-restoration/env.sh
export GIMP_BUILD_PREFIX=/workspace/scratch/5b5281e79681/.prefix-installed-filter
source tools/linux-debian13-env.sh
export GIMP_PAINTER_BUILD_LOCK=/workspace/scratch/5b5281e79681/gimp-painter-build.lock
flock "$GIMP_PAINTER_BUILD_LOCK" ninja -C build-installed-filter
flock "$GIMP_PAINTER_BUILD_LOCK" meson test -C build-installed-filter --no-rebuild --print-errorlogs \
  gimp-filter-layer painter-filter-procedure painter-filter-wire painter-filter-process \
  painter-filter-scheduler painter-filter-spool painter-filter-context \
  painter-filter-owner-gates painter-filter-import-completion
python3 -B migration/tests/run_filter_process_sanitizers.py --build build-installed-filter \
  --output /tmp/blinds-context-sanitizers --lock "$GIMP_PAINTER_BUILD_LOCK" --run
python3 -B migration/tests/run_filter_owner_gate_sanitizers.py \
  --output /tmp/blinds-owner-gate-sanitizers --lock "$GIMP_PAINTER_BUILD_LOCK"
python3 -B migration/tests/test_filter_context_fixture_bundle.py
```

The native Meson target uses `run_filter_owner_context_fixture_test.py` to verify
and temporarily extract both corpora; direct native invocations must use that
wrapper too. The sanitizer runner instruments the owner, scalar merge,
scheduler, protocol, native adapter/helper/Blinds and tests. It records the
separate RTTI-only closure, source/dependency before/after seals and private
executable identities. Other GIMP/system libraries are ordinary; LSan is off.

The installed smoke stages a fresh prototype with `tools/package-linux-runtime.py`,
relocates it, and uses `tools/check_installed_filter.py` to observe the actual
installed helper/plugin. It requires complete 4096-square genuine-old pixels,
preserved serialized definition/arguments through save and reopen/resave, no
observed surviving processes or private profiles, and restored complete cache.
It is not a release/aggregate or full-editing gate.

The 2 ms FIFO dispatch interval, 32768-pixel maximum quantum and adaptive 2–4 ms read
budget targets remain unchanged. Recorded wall-clock/heartbeat/cancel/cleanup
measurements are observations on an uncontrolled host, not hard guarantees.
Existing greater-than-100 ms outliers in broader graph workloads remain open.
Owner cleanup timing does not claim to time detached-worker drain.

Owner input and possible coverage buffers add 5 logical bytes per RGBA pixel or 3
per GrayA pixel to both memory and spill admission. This conservative allowance
can exceed the existing 1 GiB pool even with GEGL swap enabled; sparse 13000-square
RGBA and 17000-square GrayA requests must fail before any worker starts while
retaining definition/cache. Existing image storage, GEGL tile metadata and other
host allocations remain outside this per-job estimate. Arbitrary-size and total
resource guarantees are explicitly unfinished.
