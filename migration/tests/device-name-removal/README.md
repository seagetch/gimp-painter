# Superseded device removal — bounded 30.007/device-name-identity successor

A real native failure is corrected: on Linux the documented duplicate-exposure
policy replaces old GdkDevice A with same-name B in the existing persisted
GimpDeviceInfo. A later removal notification for A previously looked up the name
and detached B, losing the currently attached device. The only product change
is the removal-handler identity guard: detach/fallback only if the info still
holds the exact removed GdkDevice. Add/replacement policy, unique-names=TRUE,
serialized names, config schema and GimpDeviceInfo code remain unchanged.

## Evidence

* Exact committed host 503ade3466, private fixture and five-line manager diff.
  Initial normal expected-red fails the attachment assertion after A removal.
* Final normal two groups and focused ASan/UBSan two groups pass. Only manager
  and fixture C units are instrumented; other application/dependency units are
  not. Leak detection is off. The actual original manager compiled against the
  final fixture fails the same attachment assertion; not an invented mutant.
* TRUE versus FALSE GimpList deserialization proves the original unique-name
  change: repeated saved name reuses one profile and applies the later values
  with TRUE, whereas FALSE creates a second profile.
* Current device, opacity and named profile remain associated after superseded
  removal; pressure curve survives serialization/reload and repeated deserialize.
  Repeated/unknown-keyboard removal cannot detach the current exposure. Actual
  attached-device removal detaches it and selects the real core-pointer fallback;
  reconnecting A or B reuses the same named profile and opacity.
* Two **real retained Xvfb GDK mouse devices** are given the same synthetic name
  by one linker seam wrapping only gdk_device_get_name for those two objects.
  Real seat signals call the real manager handlers. No Gimp function is mocked,
  no real X server device is removed, and this is not physical tablet input.
* 1,323 inputs were byte-sealed at the baseline link. Before concurrent Colorify
  work, all 254 core objects were preserved in a regular (not thin) archive;
  every member hash is verified. Final shared link inputs match that baseline.
  The only added linked provider is the private manager object, and the core
  regular archive contains the original bytes. Source/link commands are retained.

Native tests use authenticated Xvfb1080x1920 and explicitly enable oncanvas UI
on an actual shell. The PNGs show that oncanvas main window only: the input
settings dialog remained obscured. They are preserved as visual diagnostics,
**not device-dialog visual acceptance or physical-device proof**. Backend
acceptance comes from the causal assertions, not a fabricated screenshot.

The initial relink checker incorrectly included its existing output executable
among input hashes; the sole changed path is preserved as a setup diagnostic.
It was excluded from inputs and the final full matrix rerun successfully. The
first final-mutant launch hit transient ENOSPC before application execution;
that receipt is retained. After storage recovered, the actual mutant ran and
failed at the causal assertion. Existing missing-icon/session-bus warnings are
retained; no clean app shutdown or theme completeness claim is made here.

## Unresolved original scope

Distinct physical devices can share vendor/product/name data; those identifiers
are not a unique hardware serial. This patch does not invent suffix identities,
change on-disk profile keys or replace platform policy. Windows deliberately
rejects duplicate replacement; its runtime has not been tested. Full original
30.007/device-name-identity and its dependencies remain open. Private fixture
uses isolated device events, not tablet/hotplug hardware evidence.

`python3 -B migration/tests/device-name-removal/validate.py` checks immutable
receipts and exact current provider/fixture. Meson registration is supplied in
a separate HEAD-derived patch for admission; this private proof does not claim
that the new registered target or whole latest HEAD was rebuilt.
