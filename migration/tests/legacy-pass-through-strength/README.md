# Original13.011: pass-through group strength

108 native contexts: nine historical byte modes, group opacity0/.5/1 and group
mask absent/0/128/255. A single Painter child (opacity.5/mask128) is inside a
pass-through group over an external backdrop. Every context matches all64 RGBA
pixels exactly. Child result bytes are genuine-old captured flat composites.
The independent group oracle converts backdrop/result bytes with Babl to linear
double RGBA, interpolates premultiplied color/alpha with group weight, then
converts to nonlinear bytes. Zero-alpha hidden RGB follows the current foundation
Replace/pass-through rule. This is explicitly current group semantics applied
to old child results, not a captured old grouped-artwork result.

Native and focused six-C-unit ASan/UBSan pass. Only the selected group test ran;
no UI/XCF aggregate or paused fault tests ran. The sanitizer instruments byte
operation, float operation/kernel, mode registry, group and C harness; all other
GIMP/GEGL/Babl/C++ dependencies remain uninstrumented. LSan is off. The additive
`--test-path` runner option records its actual selected command and leaves the
previous default full suite unchanged.

This is an owned-source snapshot of the primary build, not a clean whole-HEAD
receipt. Source hashes and actual selected test output are retained. No product
code changed and no screenshot is claimed for this numerical test. Original
criteria/dependencies remain unchanged;13.011 is not marked complete here.
