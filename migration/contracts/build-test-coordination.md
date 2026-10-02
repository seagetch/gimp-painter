# Build/test coordination in the cloud migration workspace

The shell executor and native cloud desktop share the checkout and
`/workspace/shared`, but not their `/tmp` namespace. A lock file under `/tmp`
therefore cannot be assumed to serialize operations across both surfaces.
Explicit GUI profile handoffs were used before this was corrected; individual
historical reports retain their exact source/executable hashes and do not imply
an entirely serialized aggregate build.

Subsequent phases in this environment use
`/workspace/shared/gimp-painter-build.lock`. Native runners honor
`GIMP_PAINTER_BUILD_LOCK`. Build, native-test, headless-test and focused sanitizer
phases take the same shared lock separately, so a long chain cannot monopolize
it. Manual GUI demonstrations retain ownership until the process exits. This
path is test-environment-specific, not an installed application dependency.

On 2026-10-02 08:24 UTC, an executor held a separate probe lock under the same
shared directory. The native desktop's `flock -n` refused that held lock with
status 1, establishing actual cross-surface contention. The operation only
opened the probe lock and a report; it did not start GIMP or change a profile.
Subsequent aggregate evidence must use this shared lock and still distinguish
focused instrumentation from a complete application/dependency sanitizer run.
