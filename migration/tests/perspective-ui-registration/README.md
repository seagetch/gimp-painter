# Registered Perspective UI scopes

Integration correction following the bounded original 26.005 checkpoint: the
new five cases were in the same executable as the original 24, but the default
Meson registration did not include the split-process filters used by the sealed
77-case proof. This package fixes ordinary registered execution, not production
geometry. The prior proof remains immutable.

The default `painter-perspective-ui` registration executes the original 24 only
using explicit prefix exclusions. Four additional registrations execute geometry
(two cases) and each damage angle (one case each), for a disjoint union of all 29.
Every registration retains 60 seconds. No timeout extension, case deletion, or
counting filtered cases as passing is permitted. Both configurations invoke all
five standard Meson names without command-line test-args overrides.

The source seal was captured during the first registered run, not before build.
No compilation was requested: tested ELF hashes match the preceding frozen
normal/ordinary ASan+UBSan proof. The only changed sealed input is Meson wiring.
The nine held providers remain exact prior frozen bytes; no held diagnostic is
run. Portrait Xvfb is authenticated, 1080x1920x24; the existing new cases assert
oncanvas enabled. No additional screenshot or old-runtime claim is made here.

Verified results: normal 24+2+1+1+1 PASS (maximum25.13s), ordinary sanitizer
24+2+1+1+1 PASS (maximum28.35s), zero skipped cases and zero timeouts.
Eight semantic negative controls reject omitted/duplicate scopes, invented counts,
failure, over-budget execution, removed filters/budget and wrong orientation.
Run `python3 migration/tests/perspective-ui-registration/check-registration.py`
and `python3 migration/tests/perspective-ui-registration/test-registration-evidence.py`.
