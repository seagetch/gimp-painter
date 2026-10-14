# Painter MyPaint native dockable help ID (08.018)

Restores the existing Painter MyPaint Brush Editor dialog action. The inner
editor worked, but its real native dockable wrapper rejected the NULL help ID.
Both factory and action now use the existing MyPaint edit help identifier.

## Reproduce

Use the established Debian13 dependencies, authenticated display, isolated
HOME/XDG profile, assertions enabled, and native or address,undefined builds.

    meson compile -C BUILD -j2 gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor painter-compact-options
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor painter-compact-options

Expect 93 source-registered cases in six successful processes per mode, under
unchanged 60/120-second registered bounds. The C boundary consumer explicitly
uses cc -std=c11. ASAN_OPTIONS is detect_leaks=0:halt_on_error=1:abort_on_error=1;
UBSAN_OPTIONS is halt_on_error=1:abort_on_error=1:print_stacktrace=1. LSan is off.

New editor cases /18-dialog-factory-dockable-lifetime and
/19-action-dockable-close-reopen each reach real wrapper construction before
checking help metadata. Against unchanged production at 98d5ea9708658e4319da28ba7eb828bd49987156,
both separately terminate with the fatal help_id != NULL critical (-5). Run
these controls with --test-args='-p /painter-editor/18-dialog-factory-dockable-lifetime'
and the corresponding /19 path. Baseline sources and failures are retained.
Candidate controls cover wrapper/child containment, native action reuse,
Close Tab weak destruction, closed binding, pending-preview cancellation,
reopening, draft persistence and source preservation. Action verification uses
non-creating find_widget, so a fallback creation cannot hide a broken callback.

The ordinary application before image shows the failed action leaves no editor
dock. The candidate shows the real right-side editor, pending name and completed
brush preview after visible Close Tab and reopening. Repeated activation reuses
the tab. Original source brush and genuine XCF bytes remain unchanged; app and
display exit cleanly. One focused-field shortcut attempt was unsuccessful and
is retained as such; the visible Help > Search and Run a Command route then
performed the verified activation. Full capture provenance/limits are in report.
The baseline app was built from the preceding preset publication; the only
intervening production change was independent generic icon-copy ownership.
The exact dialog/action/editor/factory source paths match the current baseline.

All 12 corrupted-report verifier controls reject bad evidence; unmodified input
passes. Adapter source scan and all 48 policy controls pass. No screenshot is
counted as a test case. External help page content, interactive multi-window
parity, the separate Layer Presets NULL-help issue and original ABI/handle
migration duties remain open. The 76 C entries and 31 core hooks remain separate;
08.018 stays open and migration count remains 141/661.
