# Perspective options identity and native registration ownership

Bounded child of open 08.018. The original source at
`afa43fae3e920210146abed514f136fd49f671b5` declares an empty
`GimpPerspectiveGuideOptions : GimpToolOptions` and the C function
`GType gimp_perspective_guide_options_get_type(void)`. That distinct identity
was absent from the native port: an actual GUI-registered tool returned generic
`GimpToolOptions`. `expected-red.json` proves the mismatch, not a compiler or
fixture failure. The earlier no-tool fixture attempt is explicitly excluded.

The original empty native subtype and C entry are restored, with no mutable
C++ implementation singleton, extra options properties, or second owner state.
The native registration callback selects that subtype. TypeTraits and owning
options/tool handles delegate to the C providers; borrowed registered options
are retained rather than consumed. The existing native ToolSlot remains the
sole behavior provider.

Five additional C/typed-owner tests cover registration identity, inherited
property identity, independent inherited configuration roundtrip, original C
registration signature and C++ delegation, and retain/adopt/copy/move/rejection/
last-owner lifetime. The model test fixture now uses actual GUI activation
rather than the headless fixture, because registration occurs during activation;
its original 34 model/factory tests remain present. The separate 24-case GUI
suite retains every test. Its two exact generic-options assertions have been
replaced by exact restored-subtype assertions, with explicit parent and instance
checks; failed intermediate oracle transitions are retained, not hidden.

The authentic screenshot is the application window on an authenticated Xvfb
1080x1920x24 display, with `painter-canvas-ui=TRUE`, visible perspective points,
oncanvas layer panel, color selector and bottom brush toolbar. It is not a mock.

## Deliberate differences / exclusions

* Source-level function signatures and empty options parent identity are restored;
  historical GObject/C++ instance/class binary layout and arbitrary old subclass
  ABI are NOT claimed.
* Old registration ignored its callback and directly constructed tool metadata
  using the paint-options context mask. Native GIMP3 honors the callback/userdata,
  uses context mask 0 and modern labels/GUI/visibility integration. Its canonical
  registration obligation remains PARTIAL, not silently marked equivalent.
* Old debug logging and dormant `#if 0` options are not reintroduced.
* Existing finite/index model safety, corrected angle behavior and changed
  notifications remain intentional native differences.
* Instrumentation is bounded to the units enumerated in sanitizer-build.json;
  RTTI-compatible-only units and other GIMP/dependencies are not sanitizer-covered.
  Leak detection is disabled; the original 60-second per-suite budget is retained.
* The earlier header-registration checkpoint is immutable. These changed headers
  require fresh header compilation and do not retroactively inherit that receipt.

## Verified checkpoint

Normal39+UI24: 2.04s/6.06s; bounded ASan/UBSan39+UI24: 3.92s/7.60s.
Exactly50 instrumented and41 RTTI-only sources. Registered public header
compilation and additive route controls both pass (0.14s/2.78s).
`validate.py --check-owned-source` verifies current owned sources; the immutable
111-input normal/sanitizer snapshots bind all tested checkpoint inputs.
`--check-source` additionally requires the whole checkpoint to match live files;
at packaging, three independently evolving files differ (listed explicitly in
checkpoint-differences.json), so that stronger check is NOT claimed to pass.
A future aggregate must test the integrated final tree, not rewrite these hashes.
