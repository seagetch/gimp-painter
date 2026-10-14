# Filter typed-handle ownership (08.018)

FilterLayerRef::create must own its factory result before image insertion can
consume the native GimpItem floating reference. It now uses sink, matching the
existing CloneLayerRef pattern. The native C factory remains unchanged.

The existing /gimp-filter-layer/cpp_header_layout case now checks non-floating
ownership before insertion, unattached release, and both independent image/handle
release orders. It disables Undo for insertion and performs no main-loop work,
so unrelated retained references do not hide the ownership contract.

## Reproduce

Use the documented Debian 13 dependency environment and existing configured
native and address,undefined builds. Compile the gimp-filter-layer target.
Run the ordinary registration with isolated HOME/XDG directories:

    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs gimp-filter-layer

Its existing corpus-fixture wrapper and 600-second timeout must remain intact.
For sanitizers, use ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
and UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1.

For the deterministic baseline, retain the strengthened test but restore only
the handle header to the public-base blob in source-manifest.json, rebuild and
add --test-args '-p /gimp-filter-layer/cpp_header_layout'. The first non-floating
assertion must fail before the added insertion paths run. The corpus-fixture
wrapper forwards the child failure and Meson reports exit 250 with the exact
assertion recorded in report.json. This is not an insertion-time UAF demonstration.
Restore the candidate header and rebuild: the same focused case and the full
147-case registration must pass.

No ordinary-UI typed-create caller was found. This is an internal typed-handle
ownership correction, so no screenshot is appropriate. Reports preserve exact
case IDs, source/binary seals and limitations without claiming all 08.018 work
complete.
