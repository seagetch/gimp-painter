# MyPaint unsaved draft names (08.018)

The shared editor can name an unsaved brush variant without renaming its saved
source. History restores its settings and working name. Save creates a separately
named resource; explicit Rename retains its existing saved-resource role.
Canceled, invalid, colliding and failed saves preserve the draft and source.
Newer reentrant edits are not replaced by completion of an older save.

## Reproduce

Use the documented Debian 13 dependency environment and configured native and
address,undefined builds, with an authenticated display and isolated HOME/XDG
profile directories. Build these unchanged registered targets:

    meson compile -C BUILD -j2 gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor painter-compact-options painter-profile-native
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor painter-compact-options painter-profile-native

Do not extend the registered 60/120-second timeouts. For sanitizers, set
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1.
Expect 102 source-registered TAP cases, all seven process exits zero, no skips,
and no unexpected warning/error/sanitizer diagnostics. The C boundary test must
compile with cc -std=c11. Test assertions/checks remain enabled.

For the normal-app workflow, compile app/gimp-3.0:executable, stage ordinary
shipped resources, use an isolated writable brush folder and the shipped charcoal
brush, and open a genuine saved image. Activate Painter MyPaint and use its
working tool-options Popup Editor. Edit opacity and Draft name, switch to another
brush, restore Draft history, and Save. The original resource must remain while
the new named resource is selected. Compare the baseline read-only name field.
Use raw screenshots of the real app; do not substitute empty GUI fixtures.

report.json records exact case IDs, terminal results, source/binary/evidence
hashes, the initial teardown failure and correction, and reviewed before/after
visual evidence shared before publication. source-disposition.md reconciles the
original callers and keeps overwrite, icon-copy, exact API and remaining 76/31
obligations open. No migration completion count changes here.
