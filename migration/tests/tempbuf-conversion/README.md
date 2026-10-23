# 09.005 actual TempBuf conversion evidence

`test-painter-tempbuf.cpp` directly calls the current production C TempBuf and
pixbuf bridge APIs and actual C++ Surface write producer. Six groups verify:

1. 144 native format/precision/TRC/space copy-and-borrow configurations.
2. Exact same-model u8↔u16 converted READ, WRITE and READWRITE locks.
3. Padded/subpixbuf import and independently owned padded-row export.
4. Explicit Gray16/RGB(A) precision/alpha conversion with exact endpoint oracles.
5. Sixteen nested GEGL aliases backed by real TempBuf, clipped Surface writes,
   parent-halo guards and commit/cancel after original-owner release.
6. Format-preserving owned scale copies, including unchanged dimensions.

Actual TempBuf allocation counters return to their starting value after every
case. This does not assert all dependency libraries are leak-free. No old leaking
TempBuf wrapper is recreated and no production patch was necessary.

`run-standalone.py --build <configured-native-build>` compiles actual
`app/core/gimptempbuf.c` and `libgimpcolor/gimppixbuf.c` as C, Surface and tests as
C++. Normal and ASan/UBSan/float-cast-overflow runs each pass six groups. Dependencies
are uninstrumented and LeakSanitizer is disabled; this is not a full-app sanitizer
claim. Each executable is bounded to 60 seconds. The configured native build's
config.h digest is recorded, not its environment.

The initial manual setup omitted the libgimpcolor compilation macro and failed
to compile the internal header; no test executed and no application defect is
claimed. The reproducible runner records the same `GIMP_COLOR_COMPILATION` macro
used by libgimpcolor's native build. Registered native integration is a separate
artifact, not inferred from standalone success.

The original criterion is stride/format/ownership conversion, not wholesale
legacy pixel infrastructure migration. All **34** source duties and their
relationships to 09.003/09.004 survive in `../../contracts/tempbuf-conversion.json`.
Full generic periodic region/dispatch/scalar replacement remains explicitly
owned by existing 31.017/09.015 tasks. Raw Meson environment maps are excluded.

The interrupted first native registration attempt used a target before Meson
regeneration and reported target-not-found. No native test ran in that attempt;
`excluded-fixture-attempts/initial-target-lookup.log` preserves this setup failure.
The recovery explicitly reconfigures the existing pinned build before compiling
the named targets. No assertion or runtime behavior was changed for recovery.

The recovery's first reconfigure was stopped by the independently reviewed
header-scope guard for the new native action header. No test ran. After the
owning task synchronized its exact header route, the unchanged TempBuf target
compiled and all three registered executables passed: TempBuf 6, real app
Surface 12, coordinate/write boundaries 7 TAP groups (25 total), with no skips.
The authenticated Xvfb runner verified 1080×1920×24 and rejected empty authority.
This is a buffer conversion test, not a visual UI acceptance claim.
`run-native.sh SOURCE BUILD` reproduces the commands under the caller's pinned
build environment and shared build lock. `registered-native.json` retains only
selected result fields; raw Meson environment maps are not included.
