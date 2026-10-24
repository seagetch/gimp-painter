# Perspective incremental-damage oracle correction

## Finding
The reproducible damage-1 failure is not missing perspective ink or an extent error. At 37 degrees, anisotropic resolution, flip 2, third-point addition (step 2), pixel (390,796) differs by four red/green levels: clipped (66,66,23), full (62,62,23). It lies outside both perspective masks and inside the independently drawn image/layer border mask.

Hiding only the perspective overlay, forcing a full window draw, then replaying the captured damage clip reproduces BOTH RGB values exactly. A Cairo image-surface control did not reproduce this Xlib window compositing behavior. The old `delta == 3` border exception was an unjustified fixed limit.

## Correction
Only `app/tests/test-painter-perspective-damage.inc` changes. Production is untouched. Keep the two-level guide tolerance and zero unexplained-pixel assertions. A pre-existing border pixel above two levels is accepted only if both original frames match, channel-for-channel and in absolute RGB, a guide-free full/clipped control. Record the actual clip, require one original and one replay draw, and assert identical delivered clip regions. Drain the restored full draw before proceeding to the next mutation.

No case is removed; no channel threshold is increased. The UI aggregate's existing five skipped registrations are separately executed by the three damage scopes and geometry scope.

## Verification
- Original instrumented oracle: RED, unexplained=1, maximum=4.
- Fixed: all three angles, both resolution modes, four flips, seven mutations = 168 transitions.
- Geometry ink bounds and pinned old-path comparisons: 288 scenes each.
- Existing UI scope: 24 cases; total distinct Perspective cases: 29.
- Negative control blocking the perspective update signal: RED, guide=224, maximum=114.
- Negative control adding one red level to the disputed border pixel: RED, unexplained=1, maximum=5. Exact replay cannot classify a nearby manufactured mismatch as expected.
- Authenticated Xvfb 1080x1920x24, oncanvas UI asserted enabled. Genuine green GUI screenshot included, visually inspected. It uses the existing legacy test profile; native normal-profile/default-oncanvas acceptance is a separate task.

## Scope and reproduction
The committed test correction is based on baseline b35ed653e2f84f8912a5323c0745b7279d02ca13, rebuild `app/tests/painter-perspective-ui`, and run the existing `painter-perspective-ui`, `painter-perspective-ui-geometry`, and `painter-perspective-ui-damage-{0,1,2}` registrations under portrait Xvfb. For negatives, separately apply each patch in `negative-controls/` to the corrected source and run `-p /perspective-ui/damage-mutations/1`; both must fail.

Existing rootcause source/build and native-session sealed package were not edited. Only read-only libraries and compile/link commands were borrowed into an isolated test executable. No commits or pushes were performed. This is targeted Perspective acceptance, not a full-port/whole-repository green claim. Raw Meson logs/environment dumps are excluded; packaged logs contain allowlisted test results only.

Independent reviewer approved the final hardened source (SHA256 `18073df1ac23a3fc38480095a5941da308d349f529ace7a6a636ba814145ccad`) with no blocking issue. Patch apply-check and byte comparison passed against the unchanged baseline file.
