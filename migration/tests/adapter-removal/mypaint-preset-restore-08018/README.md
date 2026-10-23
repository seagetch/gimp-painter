# MyPaint preset restoration (08.018)

Fixes a real startup loss: native tool reset discarded the selected preset's
settings, curves and stored icon before editing or Save. Explicit user Reset
still resets the ordinary controls while keeping draft-owned non-property data.
Save is unchanged; it now receives the complete intended working draft.

## Reproduce

Use the documented Debian13 dependency environment, an authenticated display,
isolated HOME/XDG profile directories and configured native/address,undefined
builds. Compile and run the ordinary registrations, with assertions enabled:

    meson compile -C BUILD -j2 gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor painter-compact-options painter-profile-native
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor painter-compact-options painter-profile-native

Expect 103 source-registered cases and zero exits in all seven processes, with
unchanged 60/120-second bounds. ASAN_OPTIONS is
detect_leaks=0:halt_on_error=1:abort_on_error=1; UBSAN_OPTIONS is
halt_on_error=1:abort_on_error=1:print_stacktrace=1. The actual-C boundary file
must compile with cc -std=c11. LSan is not enabled.

The baseline uses the strengthened existing native-config-reset-defaults case
with the public-base production files. It must fail the explicit draft-icon
preservation assertion after scalar defaults/source identity checks. The
preserved baseline source precedes a sparse-JSON expected-result correction;
that later assertion is not reached by the baseline failure.

For visual acceptance build app/gimp-3.0:executable, stage ordinary resources,
and start normal GIMP with a fresh isolated profile and shipped charcoal brush
plus sidecar. Activate Painter MyPaint, use tool-options Popup Editor and verify
initial opacity 0.4, radius 0.7 and hardness 0.2 with its textured preview. Edit only name
and opacity, switch resources, restore history and Save. Compare the entire
saved JSON to the original except those intended edits and embedded PNG field;
compare decoded PNG pixels to the original sidecar. Original source and image
bytes must remain unchanged. Do not substitute a blank test-host screenshot.

report.json records exact IDs/results, source/binary/evidence hashes, before/after
images shared before publication, full 43-setting/three-mapping Save readback,
12 corrupted-report controls, and open limits. source-disposition.md keeps the
legacy caller contract and remaining 76/31 obligations explicit.
