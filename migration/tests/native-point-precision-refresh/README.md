# Native point/precision checkpoint refresh

This is bounded evidence for `16.003/native-point-precision`, not completion of
parent `16.003`, the 108-procedure selector, whole-program sanitizers, all
platforms, or latency acceptance. No production algorithm, image dimensions,
assertions, cache limit, or sanitizer check was reduced to obtain a pass.

The existing sparse **8193 × 8193** native FilterLayer test runs unchanged. It
checks completion, one execution, cache generation, near/far pixels, original
procedure name and heartbeat activity. It uses the native current Edge route;
this is **not** execution of the user-skipped old Edge pilot/oracle.

The prior combined/standalone ASan SIGKILL artifacts remain in
`filter-native-large-sanitizers.json` and related historical receipts. They did
not contain a sanitizer diagnostic or confirmed OOM event. Fresh successful
runs do not retroactively turn those attempts into passes or establish a
specific historical cause. Source inspection found count-bounded preparation
and import staging, not an unbounded full-raster vector in the spool path.

## Resource interpretation

The application retains its default GEGL tile-cache policy (half reported RAM,
about 4.86 GiB on this host), which is separate from Filter spool admission.
Private temporary data and the existing `build/filter-spill-cache` are on the
disk-backed workspace overlay; `/tmp` is tmpfs but is not selected for these
runs. System swap is absent. No private user assets or profile input are used.

Normal peak target RSS was about 1.33 GiB; direct ASan repeat about 1.74 GiB.
RSS excludes filesystem cache and is not a universal application memory cap.
The build-plus-run monitor could not see nested subprocesses through this
execution environment's `/proc` child listing: its sampled 36 MiB Python value
is **not** the tested application's RSS. Use the separately captured direct
ASan target sample and `rusage_children_maxrss_kib`, not that wrapper sample.
Compilation and native runs were serialized with the shared build lock.

## Reproduction

Load the repository's locked build environment and hold the shared build/test
lock. Build the ordinary native targets. Set `GIMP_POINT_SOURCE`,
`GIMP_POINT_BUILD`, and `GIMP_POINT_EVIDENCE` to the source, configured build,
and a private output directory. Run `capture_resources.py MODE` through
`tools/run-painter-xvfb.py` for `normal`, `asan` (build + giant test),
`large-asan-direct`, `core-normal`, and `core-asan`. The wrapper authenticates
Xvfb at **1080 × 1920 × 24**, rejects empty authority and uses loopback only.

The independent kernel and scheduler sanitizer helpers remain
`migration/tests/run_filter_native_sanitizers.py` and
`migration/tests/run_filter_scheduler_sanitizers.py`. Their ordinary targets
are `painter-filter-native-kernels` and `painter-filter-scheduler`.

`verify_inventory.py` executes the actual generator row function and verifies
five receipt-backed bounded dispositions, unchanged unique 108-name inventory,
and explicit remaining obligations. Its remaining-route count is a literal
inventory label count, not independently established generic completeness.

## Reproducer repair

The refreshed independent-kernel sanitizer compilation initially failed under
`-Werror=attributes`: its test-local `Memory` class had default visibility while
its `FilterRaster` base now has hidden visibility. The helper now explicitly
uses `-fvisibility=hidden`, as the existing scheduler sanitizer helper does.
No warning, sanitizer, assertion or test is suppressed. The failed compiler log
is retained, and rebuilt results are recorded separately; this is a build-helper repair,
not a change to filter arithmetic or a claimed fix for the historical SIGKILL.

## Final source-bound acceptance

`final/` retains the post-initializer rebuild receipts: unchanged 8193-square
ASan pass (plus direct repeat), nine point/native/spill/alias cases, six kernel
and 39 scheduler cases. The core build instruments 20 units; its 50 RTTI-only
units are explicitly not sanitizer coverage. The three point/alias/reentry GTK
paths pass in the current 70-case normal and ASan layer suite, bound by
`ui-bound-acceptance.json` to the initializer package (73 instrumented and 37
RTTI-only units). LSan is disabled in these established helpers.

`original-checkpoint.json` preserves the exact prior WIP claim. New evidence
closes only the bounded child, not the parent or generic remaining obligations.
`validate.py --check-source` verifies actual TAP paths, exit statuses, no skips,
source seals, giant log hashes and the sibling UI receipt hashes.
`verify_inventory.py` checks the five bounded inventory dispositions.
