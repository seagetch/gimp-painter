# Widget exception-boundary controls (08.018)

Two shared noexcept helpers previously caught only std::exception. The accepted
05.013 contract also requires unknown C++ exceptions to remain inside the native
boundary. This correction adds one catch-all per helper, retaining diagnostics
and existing safe facade fallbacks.

## Reproduce

Use the documented Debian 13 dependency environment and configured native and
address,undefined build directories. Reconfigure after adding the Meson target.
Compile painter-widget-exceptions, painter-canvas-ui and painter-compact-options.
Run under an authenticated display with isolated HOME/XDG fixture directories:

    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs \
      painter-widget-exceptions painter-canvas-ui painter-right-docks-ui \
      painter-dock-session-ui painter-dock-input-ui painter-dock-layout-ui \
      painter-compact-options

For the sanitizer build, use ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
and UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1. Normal registered
timeouts remain unchanged. Linux GCC/Clang and linker --wrap support are required
for this dedicated executable; ordinary builds have no injection mechanism.

For the causal baseline, retain the tests/registration but restore only the two
production helpers to the manifest's public-base blobs and rebuild the dedicated
target. Run the /painter-widget-exceptions/standard group with --test-args '-p PATH'.
Run each unknown case separately using its exact path from baseline.json: each
must terminate with SIGABRT and the specified integer-exception diagnostic.
Restore the candidate production sources, rebuild and run all six controls.

The C test checks unchanged visibility, FALSE with unchanged parent, and NULL
with preserved live widget/image association, followed by successful retries and
weak-pointer destruction checks. These are pre-operation faults, not proof of
arbitrary rollback. report.json contains all 138 executed case IDs for each mode;
source-manifest.json fixes the precise code inputs. No screenshot is appropriate
for this internal exception-containment correction. The original 76 C-entry rows
and separate 31 core hooks remain open for full obligation reconciliation.
