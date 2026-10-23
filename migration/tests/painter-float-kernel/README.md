# Original 13.012: isolated opt-in float kernel checkpoint

This new, currently **unregistered** kernel implements the continuous Painter
nonlinear float policy specified in `migration/contracts/painter-float-mode-policy.md`.
It changes no public enum, menu, XCF reader, cached dispatch or existing byte mode.
Private kernel enum indices0..8 are not layer-mode IDs or old XCF raw values.

Independent Python double reference computes premultiplied weighted sums then
unassociates, whereas production computes normalized straight-color weights.
326,196 vectors cover all nine modes, the full product of nine alpha/opacity/mask
boundary values (including NaN, infinities, negatives and >1), four signed/HDR/
nonfinite RGB patterns, and90,000 seeded continuous vectors. Every case checks
both input/output alias configurations:652,392 alias checks. Additional probes
assert invalid mode leaves output unchanged and non-byte-grid RGB, negative RGB,
and HDR values survive. Comparison tolerance is2e-6*max(1,abs(expected)); this is
not a historical exact-byte oracle. Optimized and ASan/UBSan both pass; LSan off.

Production uses double intermediates, sanitizes nonfinite components, clamps
coverage to[0,1], retains finite signed/HDR nonlinear RGB and saturates only at
float representability limits. This is not linear-light/physical HDR correctness.
No user-visible high-precision path exists until native registration/integration
is complete, so original13.012 remains DOING. Existing byte proofs are unchanged.

Run `python3 migration/tests/painter-float-kernel/run.py --report PATH`, optionally
`--sanitize`. Generated case vectors and executable live only in a temporary
directory; reports do not include inherited environment.

The test-only GEGL adapter also passes45 graphs x64 pixels at negative-origin
ROI with connected nonlinear-float input/aux and Y-float masks, against the same
independent oracle (normal and focused ASan/UBSan). This verifies real GEGL
buffer/pad plumbing for the isolated kernel, not native application registration.
GEGL/Babl dependencies remain uninstrumented.
