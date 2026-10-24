# Normalize and Exchange: actual portrait rendering

Two actual GUI cases display the unchanged source with the Filter hidden, then
the committed native result with it visible. Definitions and visibility use
**direct core APIs**, not parameter-editor gestures. Each 400x300 full source,
cache and projection buffer is compared with independent byte arithmetic. The
Normalize image spans shared extrema40..180; Exchange changes only an exact
warm patch to pink and leaves the cool control/background unchanged.

Authenticated loopback-only Xvfb is verified1080x1920x24 with invalid authority
rejected. Both cases assert the actual root dimensions, actual1040x1800 portrait
application window and visible oncanvas controls. Four original PNG captures are
included, not generated, composited or edited. All four were visually inspected.
An independent PNG check finds eight mathematically expected displayed patch
colors (tolerance3/channel), each covering over1000 pixels. No black-window or
model-only evidence can satisfy this check. The before/after image stays at200%;
Exchange's initial thumbnail layout settles by12 vertical pixels, so no fixed
canvas-origin claim is made. Existing missing-icon warnings remain visible in
the receipt and are not silently claimed fixed.

Only the mode-ui test fixture and new include override exact committed
0a775df4135052a2c851c9a5dfa37cea25887268. One private C object is linked against
unchanged committed libraries; all source and linked bytes and runtime asset
files are checked before/after. Profile/output directories are private. The
clean source/build is not written. This is a test-only successor to the exact
aggregate, not an additional whole-application build or UI sanitizer run.

`validate.py --check-source` checks only the owned fixture sources, then rechecks
actual PNG content and recorded native assertions. XCF/native/old-oracle proofs
are separately pinned predecessors. Parameter-editor activation, broader
precision/owner domains and original16.022 acceptance remain open.
