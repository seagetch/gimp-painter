# BucketFillBrush original C operation names

The exact original two C function signatures now reach the sole native
FillBrush provider. Actual runtime name remains GimpFillBrushTool. No second
subtype/controller/store or historical class-layout equivalence is claimed.
See migration/contracts/bucket-fill-tool-entries.md and the explicit successor
08.018/bucket-fill-original-registration for retained old registration duties.

The runnable native expected-red uses weak declarations of the exact original
C signatures and fails because get_type is absent (1.54s SIGABRT), before any
provider is invented. Final tests instead include the real header and strongly
link the real C symbols. They cover stable native identity/parent, actual GUI
registry options/tool identity and lifetime, exact opaque callback forwarding,
recursive registration, inert NULL callback, nonfloating typed ownership and
wrong-type rejection. A link wrap proves the C++ registration method calls the
original C provider exactly once. Four exception classes are contained through
both original and native registration spellings.

The eighth test creates a real image, selects the registered FillBrush in the
actual tool manager, asserts oncanvas TRUE, and captures the rendered window.
The status bar identifies the fill-with-brush tool. The application window is
1040×1800 on authenticated Xvfb1080×1920×24. This is registration/selection UI
proof, not a new full paint-kernel pixel equivalence claim.

A first green build was blocked before compilation/tests by the independently
registered preset-factory-gui.h missing from the detached source. Its diagnostic
is retained; the parent synchronized that committed header under the build lock.
No header gate was bypassed. Subsequent own synchronization overlays only the
paired BucketFillBrush header route, rather than copying an evolving manifest.

C11 and C++14 first/double includes compile with -Werror (C++ uses the target's
no-RTTI flags). Public registered header closure compilation is recorded
separately; unrelated reviewed routes in that aggregate are not attributed to
this child. Selected sanitizer units and RTTI-only units are enumerated, and
other GIMP/dependencies remain uninstrumented. Leak detection is disabled.

## Verified checkpoint

Native8 PASS2.70s; bounded ASan/UBSan/float-cast-overflow8 PASS4.12s,
original60-second focused budget. Exactly43 source units are instrumented and41
C++ units are RTTI-compatible only. The11-input normal direct-source snapshot
and92-input sanitizer snapshot agree on all common inputs; this is not a claim
of instrumenting every application dependency. Both rendered screenshots were
visually inspected with the fill-with-brush status and oncanvas controls shown.

The detached registered closure compiled203 C +224 C++ probes (427 total); both
registered header checks passed. Its263 exact source inputs are preserved in
header-tested-sources.tar.gz. This closure includes independently owned float
operation/kernel routes; their implementation is not credited to this task.
It is an immutable detached checkpoint, not the full evolving main checkout or
a final-current repository aggregate. No historical header proof is resealed.

Private Meson environment values are removed from packaged JSON, retaining only
explicit test/sanitizer/path keys. Publication remains parent-controlled.
