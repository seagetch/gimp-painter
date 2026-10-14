# MyPaint generic-copy pixel independence (08.018)

Fixes shared mutable icon storage between a Painter MyPaint resource and its
native generic Copy/Duplicate. The clone now snapshots live pixels before
publishing target settings/icon. Full resource JSON is retained independently.
Native naming, explicit icon serialization, and self-copy contracts are unchanged.

## Reproduce

Use the established Debian13 dependency environment, authenticated display,
isolated HOME/XDG profiles, assertions enabled, and native or address,undefined
builds. Run ordinary serial registrations without extending their timeouts:

    meson compile -C BUILD -j2 gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-painter-mybrush gimp-painter-mybrush-factory gimp-painter-options gimp-painter-session painter-options-session-c painter-mypaint-tool painter-mypaint-editor

Expect 78 source-registered cases and seven zero process exits per mode, under
unchanged 60/120-second registered bounds. The resource/factory tests compile as actual C using cc and its default C
standard; the boundary consumer explicitly uses cc -std=c11. ASAN_OPTIONS is
`detect_leaks=0:halt_on_error=1:abort_on_error=1`; UBSAN_OPTIONS is
`halt_on_error=1:abort_on_error=1:print_stacktrace=1`. LeakSanitizer is disabled.

The retained baseline uses these same new tests against unchanged production at
0b8137a8deab4618e445f2edd9bd124e72cc10d4. Run each separately with
`--test-args='-p /painter-mybrush/duplicate-icon-pixel-independence'` and
`--test-args='-p /painter-mybrush/copy-icon-pixel-independence'`.
Both baseline processes fail at the source-pixel comparison after target mutation,
after initial live-pixel and full-JSON checks pass. These are regression-control
assertions, not claims of an ordinary UI crash.

The eight resource cases also load/copy all 177 shipped Painter brushes, roundtrip
a stored icon, retain closed-owner/finalizer-reentry controls, exercise RGB padded
rows and RGBA alpha, copy live pixels newer than PNG, check two-way write isolation,
clear an old target icon, and preserve native self-copy behavior. Explicit set_icon
serialization is separate from direct mutation through ref_icon.

Strict terminal/TAP verification rejects missing/duplicate/skipped cases, sanitizer
or teardown diagnostics, changed source seals, and timeout extensions. All 12
corrupted-report controls are rejected and the unmodified report is accepted.
The adapter scanner and all 48 policy controls pass; these are source gates,
not a substitute for runtime acceptance or the remaining migration obligations.

Allocation-failure handling is source-reviewed, not fault-injected. The native
void copy boundary cannot report errors, and generic duplicate can return its
new default object after a contained copy failure. No broader API redesign is
claimed. The original 76 C-entry rows, separate 31 core hooks, overwrite and
colorpicker semantics, dockable help-ID gap, and blocked perspective work remain
open as documented. Migration count remains 141/661.
