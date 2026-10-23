# Perspective model C operations and failed-construction admission

Task08.018/perspective-model-entries. Verified bounded scope: normal34/34 + UI24/24 and focused
ASan/UBSan/float-cast-overflow34/34 + UI24/24 pass under original60-second
runtime budgets.47 source units instrumented;41 additional C++ units have RTTI
compatibility only, other GIMP/dependencies are uninstrumented, LSan disabled.

Original7 C signatures are unchanged, independently compiled from original
`afa43fae3e920210146abed514f136fd49f671b5` and current headers with C11,
Wall/Wextra/Werror. Owning facade uses current names count/add_point/get_point/
set_point/remove_point; those names do not duplicate the provider implementation.

Preserved intentional differences: negative/invalid indices and nonfinite points
are rejected safely; the old angle property incorrectly used id accessors, while
native angle is independent; native changed signal updates image/explicit canvas
observers. No historical class/instance ABI or exact erroneous behavior claim.
Original valid model operation data remains checked by the existing extracted
source oracle and runtime-capture fixture in the unchanged perspective suite.

Failure coverage targets early store creation and completed activation, each
through raw C construction and the original checked C factory with four C++
exception categories. Constructor failure must prevent subsequent property
access to absent/closed storage and return a null factory result without escaping
exceptions or fatal property diagnostics. Failed raw instances are safely inert.

Expected-red is the real early-store fault: fatal property diagnostic before
factory completion. Three production admission checks now prevent property
access and checked operation access to failed instances. No assertions, diagnostic
fatality, sanitizer setting or time budget was weakened. Setup-only failures
(duplicate header route inventory; C11 nonstandard M_PI name) are excluded from
behavior evidence. The test wrapper maps M_PI to the identical GLib G_PI constant;
the immutable legacy oracle is unchanged.

The actual app PNG was captured on authenticated1080x1920x24 Xvfb, app1040x1800,
with oncanvas UI enabled. The full24-case UI aggregate also covers repaired
retained tool groups. Original model fixtures, tested-source archives and hashes
are recorded; Meson receipts exclude environment dumps.

Run `python3 migration/tests/adapter-removal/perspective-model-entries-08018/validate.py`
for historical verification, adding `--check-source` for current owned-source
binding. Parent08.018 remains open; no historical binary-layout claim follows.
