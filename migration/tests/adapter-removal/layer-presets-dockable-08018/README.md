# Layer Presets native dockable (08.018)

Restores native opening, closing and reopening of Layer Presets. Both factory
and action use the existing generic dock help identifier. The view advertises
icon-only tabs because Layer Presets selection has no GimpContext property;
context, menu, buttons and auxiliary-info behavior remain inherited.

## Reproduce

Use the established Debian13 native or address,undefined build, assertions
enabled, authenticated display and isolated HOME/XDG paths.

    meson compile -C BUILD -j2 layer-presets-ui gimp-layer-presets painter-mypaint-editor
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs layer-presets-ui gimp-layer-presets painter-mypaint-editor

Expect 36 cases across 3 successful processes per mode: UI 6, backend 11 and
neighboring MyPaint editor 19. Registered bounds remain 120, 90 and 120 seconds.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1;
UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1.
LeakSanitizer is disabled. Both C consumers use cc with the project-default
language standard, not a C++ language override.

The new /layer-presets-ui/dialog-factory-dockable-lifetime and
/layer-presets-ui/action-dockable-close-reopen controls reach actual wrapper
construction before checking metadata. Against unchanged production at the
recorded public base, each separately fails help_id != NULL (-5). The two-help
only candidate then fails at the inherited NULL-property tab preview. Both
stages are retained. Final controls create a real display and preserve source,
image/history and context through native opening, re-raise, Close Tab and
reopening; they also construct all three preview-style tabs and check icon
style normalization. A bounded natural-map wait repairs the retained test
allocation-timing failure without forcing visibility or weakening assertions.

## Visual evidence and limits

Before/after ordinary-app images were shared before commit on 2026-10-14.
The baseline action leaves the dock absent; candidate opens the real preset
list, closes and reopens it, repeats activation, closes and quits cleanly.
The original XCF/resource bytes remain unchanged. Baseline quit instead
exited SIGSEGV; its cause is unresolved, and no causal crash-fix claim is made.

A separate native row-click defect remains: one click created two clones.
The explicit Apply button produced one clone and one Undo restored it, but
this does not establish general Apply correctness. That event-path defect
is the next bounded correction; these images only establish dock integration.
Programmatic selection controls do not substitute for real pointer events.

Original 76 C entries and separate 31 hooks remain open at their exact mapped
boundaries. No whole entry is closed, and no blocked perspective changes are
included. See source-disposition.md and report.json for exact limits, retained
failures, source/binary/evidence hashes and sanitizer coverage.
