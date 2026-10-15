# Private Filter swap teardown

This is a bounded shutdown fix for the one-request native Filter helper. It
does not change filter pixels, resource limits, process cancellation, the wire
protocol, or the accepted execution routes.

## Cause and supported observation

The Debian 13 native baseline uses GEGL 0.4.62, also this checkout's minimum
GEGL version. The public `gegl_stats()` API returns a borrowed `GeglStats`
object. Its installed header and GIR expose `swap-busy` as a boolean and
`swap-total` / `swap-queued-total` as unsigned 64-bit properties.

The version-pinned upstream sources establish the relevant distinction:

- [GEGL 0.4.62 swap backend](https://github.com/GNOME/gegl/blob/GEGL_0_4_62/gegl/buffer/gegl-tile-backend-swap.c):
  enqueue marks the writer busy. The writer clears that flag only when its
  current operation has completed and the queue is empty. Releasing a final
  buffer reference can enqueue an `OP_DESTROY`, including by converting an
  already queued write. Destroy operations can exist with zero queued bytes.
  Cleanup sets the exit flag, wakes and joins the writer, then checks the
  queue and free-space list. It does not first drain all queued operations.
- [GEGL 0.4.62 statistics](https://github.com/GNOME/gegl/blob/GEGL_0_4_62/gegl/gegl-stats.c):
  the three public properties read the corresponding swap-backend values.
- [GEGL 0.4.62 exit](https://github.com/GNOME/gegl/blob/GEGL_0_4_62/gegl/gegl-init.c):
  `gegl_exit()` calls swap-backend cleanup before tile-cache destruction.

Retrieved source SHA-256 values, respectively:

- `d78f04fd108b02d51fac2c63d7cf7fe86c0e7aab90bd19cb2a0d47ae27678db0`
- `5f502933cc70d4c1a27310fe3eeaeb54d098437645ac80cc1273c875fe2b40da`
- `68e14a09ecbb30ea8b593197bceca9eb5ee724813440697669667a17a4843e16`

The earlier aggregate observed the abandoned-queue and gap-list warnings
after an intentionally rejected Convolution request. A subsequent baseline
observed the same warning pairs during the successful procedure corpus.
This is a timing-dependent teardown issue, not evidence that only one route
or only rejected requests are affected.

## Ownership and drain order

The private runtime is constructed before all request-owned native objects,
so those objects are released before its destructor runs on success, early
return, or exception unwind. The destructor closes and reaps plug-ins,
finishes GIMP GEGL operations and parallel work, releases contexts, and
releases Gimp. Only then does it poll `swap-busy` until false, yielding to the
writer between observations. There is no fixed delay, swap disabling, cache
inflation, log suppression, or private GEGL API dependency.

The wait has a five-second monotonic deadline. Expiry is a fatal diagnostic,
not a continuation into potentially unsafe cleanup or a successful result.
Once idle, both `swap-total` and `swap-queued-total` must be zero. Nonzero
storage is a fatal ownership-invariant failure. `swap-file-size` is not
required to be zero: allocated free space can remain until GEGL cleanup.
Normal `gegl_exit()` still runs after a successful drain. The helper's
existing lifeline, parent cancellation and process-group cleanup remain in
force; no terminal success is sent until teardown has completed.

This relies on the private helper having no remaining producer after that
release order. The upstream getters are sampled statistics, not a documented
locking or synchronization API; the upstream source even notes possible
inaccurate 64-bit observations on 32-bit targets. This fix makes no global
GEGL concurrency guarantee and does not certify a shared runtime where
another thread can create or release buffers after the observation. Such a
producer would require additional ownership/synchronization work.

## Regression and acceptance boundary

`painter-filter-procedure` adds eight interleaved success/exception pairs with
258 by 198 Gray-alpha inputs crossing native tiles and transfer chunks,
alternating horizontal and vertical orientation. Both dimensions produce
three even-width segments: the legacy Blinds algorithm leaves the last
sample of an odd segment as background even at angle zero, so the original
257 by 193 regression input was not an identity and failed its pixel check.
Successful Blinds identity requests require exact pixels and merged
disposition. Negative requests reject a nontriplicated background after the
input GEGL buffer has been flushed; they require a pending disposition and
untouched output bytes. Every pair checks that the parent temporary root is
empty. `--teardown-only` runs these pairs without replaying the old corpora.

The driver captures and then reproduces all child stderr. It accepts only
the eight exact Gray-background rejection lines and, in the full corpus,
the four existing exact RGB-carrier rejection lines. Missing, additional or
different diagnostics fail the test, so an expected rejection cannot hide a
swap warning, fatal timeout, or unrelated crash. Original diagnostics remain
in the caller's log on both success and exception paths.

The installed acceptance runner requires the new result marker and those
same exact corpus stderr counts, with no stderr expected for Convolution.
It rejects warning, critical and error diagnostics in captured subprocess
output. It additionally checks nested installed-runtime logs for GEGL
diagnostics, retaining log name, line number and text in the report.
Unrelated installed-console startup-warning policy is unchanged; intentional
missing-runtime scenarios and their existing causal evidence are preserved.

`migration/tests/test_filter_acceptance_diagnostics.py` reproduces the
classifier controls without a native build or display: exact negative counts,
missing/additional negatives, warning/critical/error messages on either
stream, unexpected plain stderr, and the scoped nested-log policy.

## Scoped normal validation

The [validation receipt](../tests/filter-swap-teardown.json) records the exact
three tested source hashes and seven executable hashes over native source
base `8d8c06d32a6b05d0d5babaef5386e0604a2ad59d`. The native build and focused
eight-pair run passed. The complete registered `painter-filter-procedure`
and `painter-filter-convolution` tests then passed in 771.92 and 268.04 seconds,
respectively. Their output retains all required corpus markers and exactly
the twelve intended rejection diagnostics; there are no unexpected
diagnostics. Four pure-Python test methods cover fourteen diagnostic controls.
Source and executable seals remained unchanged across the native run.

The receipt preserves both failed regression attempts and the precise
odd-segment identity diagnosis. These passes establish this bounded normal
teardown/corpus checkpoint. They do not establish an ASan/UBSan refresh,
repeated full-corpus campaign, installed-runtime acceptance, full aggregate,
GUI acceptance, or release readiness. Timeout/invariant fault injection was
not performed, and installed-runtime acceptance must be refreshed separately.
