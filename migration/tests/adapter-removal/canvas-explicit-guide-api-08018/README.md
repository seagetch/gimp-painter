# Restore the explicit-guide C operation (08.018)

This is separate from the preceding checked native factory child. Fixed legacy
source is `afa43fae3e920210146abed514f136fd49f671b5`.

The old operation returns `GimpCanvasPerspectiveGuide *` and accepts `(shell,
guide)`. Its read/write construct `guide` property retains the explicitly
supplied model, including NULL, and its getter exposes that model's identity.
The temporary native one-argument provider had instead followed the image and
omitted this property. The original and restored headers compile the same C11
function-pointer probe; the preceding one-argument header fails that probe.
Setup-only missing-prelude failures are retained separately from this result.

The restored operation and `CanvasPerspectiveGuideRef::create(shell, guide)`
use the one existing OverlaySlot/native GType. The native auto-follow extension
is explicitly named `gimp_canvas_perspective_guide_new_for_image` and
`CanvasPerspectiveGuideRef::create_for_image`. The shell-item caller uses that
extension; its existing dynamic UI behavior remains covered by the full
perspective regression. Setting the guide property explicitly retires image
following. The shell is still borrowed; the item and model are nonfloating
owned references.

Replacement acquires the next model, signal connection and state before
publication; it keeps the outgoing model alive through the item update and
signal disconnection. It does not write stale state after an update observer
or outgoing model finalizer reenters. C tests cover getter ownership, explicit
identity independent of image models, replacement/clear, old-model events,
pixel/extents output, finalizer/update/property-notify reentry and close,
auto-follow override, plus early/store/connection/activation failure cleanup.
The C++ facade calls the same C provider and does not own an independent Impl.

Scope limits: this restores source-level operation/signature/property ownership,
not historical GObject instance/class binary layout or whole historical pixel
parity. The old extents implementation repeatedly queried point0 for multi-point
models and used an uninitialized rectangle for a non-NULL zero-point model.
Those defects are not restored; safe native transformed/finite bounds remain.
NULL guide remains inert. Valid shell remains a precondition with a checked
NULL result for invalid calls. Private C++ first/double inclusion is checked in
C++14 without RTTI and with Wall/Wextra/Werror.

Acceptance: normal 61/61 real-C/facade/fault cases and full perspective 24/24
pass; ASan/UBSan/float-cast-overflow repeats 61/61 + 24/24 under the original
60-second budgets. Instrumentation covers 47 source units; 41 additional C++
units are RTTI-compatible only, other GIMP/dependencies are uninstrumented,
and LSan is disabled. Source hashes, immutable archives, selected Meson output
(no environment dump) and build commands bind this bounded checkpoint.

Both PNGs are actual rendered app windows (1040x1800) on authenticated
1080x1920x24 Xvfb with oncanvas controls enabled: the explicit scene shows one
supplied-model marker, the native auto-follow scene three image-guide markers.
No broad parent08.018, historical layout or full pixel-parity completion follows.
Run `python3 migration/tests/adapter-removal/canvas-explicit-guide-api-08018/validate.py`
for historical receipt validation; `--check-source` additionally requires the
current owned sources to match this checkpoint.
