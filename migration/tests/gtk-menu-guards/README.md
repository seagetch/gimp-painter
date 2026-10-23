# GTK guard verification

Exact original acceptance: reproducible dependency patch for Debian offscreen
NULL GdkWindow and upstream accessible-ancestor type checks; strict fixed mode
zero; independently validate offscreen, ordinary input and AT-SPI; retain the
3.24.52 investigation and update source/license/runtime provenance.

The unchanged standalone reproducer from `../gtk-dialog-diagnostics/` produces
both original diagnostics with exact Debian 3.24.49-3 source-built GTK. Its strict
mode exits1 before the patch and0 after it. Direct GTK activation remains clean.
Independent controls prove offscreen grabs are still rejected, mapped normal
widgets acquire/release grabs, and unrealized menus can acquire/release grabs.

A separate process uses libatspi to discover the hidden menu and perform its
action over real D-Bus, then generates a native X11 pointer click on the ordinary
button. Exactly one dialog action and one click reach the GTK process. Warnings
are fatal. No accessibility feature or warning is suppressed. The constrained
workspace forbids Unix sockets: a private nonce-authenticated loopback bus with
DBUS_COOKIE_SHA1 and supported ATSPI_DISABLE_P2P=1 carries all accessibility
traffic instead. No anonymous/public listening address is used. Nonce files,
bus addresses and profiles are deliberately not in this evidence package.
The registry is extracted from pinned at-spi2-core2.56.2-1+deb13u2, not the host.

`run-controls.sh OUTPUT PINNED_REGISTRY` compiles the real C controls with strict
warnings and runs under an existing display. Use `tools/run-painter-xvfb.py` to
provide authenticated1080×1920×24. Source the pinned build environment and
explicitly select the repaired GTK library directory. Source build instructions
and all source hashes are in `migration/patches/gtk-3.24.49-menu-guards/`.

A separate real GIMP native tool fixture renders applied/disabled layer masks
with oncanvas controls at1080×1920. Its unchanged executable hashes and live
`/proc` GTK-only mapping prove which repaired GTK it loaded. Raw PNGs were
visually inspected; masks are changed programmatically, not by physical pen.
No whole-app sanitizer, full GTK test suite, Wayland or physical tablet claim.
The earlier source-built checks and new dependency receipts do not replace
historic baseline receipts or certify a final release bundle.

`validate.py` checks the evidence; `test-validator.py` rejects seven altered
receipts. Setup-only failures are described in report.json, not counted as
behavioral regressions or successful tests. No credentials, profiles, nonce
material or full environment dumps are included.

The final build script also completed a second clean1,029-step source build,
private install and repeated strict/grab/external AT-SPI controls successfully.
Its separate runtime hashes are recorded; different build paths/prefixes mean
no bit-for-bit reproducibility claim. The GIMP screenshots use the first patched
prefix, whose source identity and live loaded library are independently sealed.
