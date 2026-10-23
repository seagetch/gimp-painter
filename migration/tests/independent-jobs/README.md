# Original WBS 06.028: independent asynchronous job state

The original condition is that asynchronous work owns its input, result and
cancellation state without capturing the owner's Impl. The current vector Job
and spill State already implement this. This task adds deterministic observations
after owner destruction, checks real GIMP owner shutdown through actual worker
completion, and refreshes the focused sanitizer runner. No lifetime defect in
production execution was reproduced.

## Ownership and executable assertions

The vector thread captures shared Job and FilterLifetime only. Job owns its
input/output vectors, process closure, cancellation/completion atomics and lease.
The spill thread owns shared State and FilterLifetime; State owns queues, process,
geometry/path and cancellation, while its run scope owns the input/result files.
The spill factory's `this` refers to this independent State. Owner-side read,
import, commit and context gates remain ephemeral synchronous step arguments.

`independent_state_after_owner_destruction` gates both real worker routes before
the owner is destroyed. After the entire scheduler harness is gone, the worker:

- observes cancellation and the original input, despite a changed source value
- writes and reads its own result; the spill path also reads the original
  raster again and verifies the final output raster after owner loss
- deliberately returns success, which cannot publish to the vanished owner

The test waits for the independent lifetime count to reach zero, then checks
that its captured processor was destroyed once and that job, byte and spill
reservations were released. The spill completion flag is set only after its
last raster operation, so a caught worker exception cannot masquerade as success.
Separate vector and spill import-close cases continue reading correct result
bytes after close from the import callback and forbid commit.

The common Process callable is a trusted adapter boundary, not a type-system
proof against arbitrary captures. The current production captures are audited
in `source-mapping.json`: value options/dimensions and owned descriptor, process
options, outcome and progress channels. No owner Impl is captured by those jobs.

## Native integration and evidence

The actual GIMP `image_close_during_worker` case observes image/source/filter
finalization and then waits for worker resource completion. The retained-handle
case covers pre-run, vector and spill sizes, CLOSED state and unchanged completed
pixels/cache/run count. It both drains workers and preserves a separate 50 ms
main-context observation for late owner callbacks. Native RUNNING polling alone
does not guarantee worker overlap; the deterministic gate test above does.

`native-scheduler.json` and `sanitizers.json` record 39 passing scheduler cases
with current source hashes. The sanitizer runner now links FilterLifetime,
records relevant headers, and rejects source drift after compilation/run.
Request's progress member explicitly defaults empty, matching its existing
behavior and permitting current strict aggregate-initialization builds.
`native-layer.json` separately records the two real GIMP cases and source seals.
Only the independent scheduler/raster/spool/lifetime closure is ASan/UBSan
instrumented; GIMP integration is an ordinary native build and LSan is off.

```sh
ninja -C BUILD app/painter/painter-filter-scheduler app/tests/gimp-filter-layer
BUILD/app/painter/painter-filter-scheduler
python3 migration/tests/run_filter_scheduler_sanitizers.py BUILD --report /tmp/scheduler-asan.json
python3 migration/tests/run_filter_native_checks.py BUILD --mode foundation --report /tmp/filter-owner-close.json
```

There are zero exact 06.028 assignments in the hunk/asset/source-duty inventories;
the source ledger remains byte-identical. This closes the original common-state
condition, not generic procedure coverage, all feature jobs, platform acceptance
or the separate 07.012 completion-race task. Historical reports keep their hashes;
`validation.json` separates current acceptance from old snapshot mismatches.
