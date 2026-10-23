# Original WBS 07.005: construction properties and callback admission

The former lifecycle fixture measured construct → property → activate but did
not connect a callback. The new independent C GType fixture closes that test
gap; no production defect or behavior change is claimed.

Two G_PARAM_CONSTRUCT integer properties are set through actual native C vfuncs
into the registered C++ slot. Their setter uses `initialize` while constructing.
A deliberately premature test-only probe is connected after slot registration,
and each setter emits a real custom GObject signal. Its C-compatible trampoline
calls the normal `with` path through the exception boundary. Both attempts must
return INVALID_STATE with zero entries into the Impl callback body.

The native constructed hook verifies both properties were set, no Impl callback
ran and the normal connection is still absent. It disconnects the premature
probe, activates the store, then installs the normal owned Connection. An actual
emission must now observe both initialized values. The test covers default and
explicit construction values, reads both through native getters, and checks a
later property update reaches the active callback. Repeated dispose disconnects
the observer; a later signal causes no attempt and final unref destroys once.

Exact attempt/run counts also reject vacuous success: missing the early probe,
forgetting to remove it, failing to connect the normal callback, or admitting an
uninitialized Impl all fail the assertions. The probe intentionally exercises an
invalid early callback attempt; normal callbacks follow the documented setup
order. This custom signal does not assert GObject's queued notify timing.

`native.json`, `sanitizers.json` and `meson.json` each contain 66 passing common
cases with current source seals. Native fixtures remain C11 and implementation
adapters C++14. ASan/UBSan cover this mixed component, not system GLib; LSan,
whole-application and cross-platform results are not claimed.

The one old duty, `legacy-ffd397217ccde9b8df7a`, maps exact glib-cxx-impl.hpp
blob `a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`. Its property dispatcher calls
registered setters (185–206), while instance_init placement-constructs Impl
(407–410). The current adapter retains construction-time setting and makes
callback admission explicit through store state. The old implementation is not
claimed to have the new activation gate. Other feature-specific initialization
and callback requirements remain separate. `validation.json` distinguishes this
current task from historical evidence whose test-source seals have changed.
