# Original 13.012: source-only/zero-weight float regression

The independent endpoint test first failed: DST IN at opacity0 in a source-only
ROI returned0 instead of source RGB0.234567001. The generic parent bounding box
removed source coverage, and its empty-input shortcut could return before the
swapped-DST equation. This also conflicted with specified bottom initialization
retaining hidden source RGB at zero coverage. The causal red receipt and exact
initial test are retained; it stopped at the first mismatch, not a full matrix.

Fix is confined to the **new opt-in float subclass**: its bounding box retains
raw input/source union regardless of opacity/mask; its layer parent_process
virtual keeps the inherited effective-mask/opacity wrapper but uses the actual
point-composer path without invalid generic early exits. Missing input/aux gets
the same empty-buffer preparation as the base path. Shared generic layer-mode
and historical byte operator files remain byte-identical to the prior checkpoint.
No ordinary-mode shortcut or old Painter behavior was changed.

162 independent endpoint graphs now cover all nine float modes, source-only ROI
with a nonempty backdrop elsewhere versus true bottom initialization, opacity
0/.37/1, and absent/in-ROI/out-of-ROI masks. Expected endpoints derive directly
from the documented equations with A=(0,0,0,0), not the production kernel.
All12 native suites and focused six-C-unit ASan/UBSan pass. This includes prior
byte oracles, float numerical/storage/group/order tests. Other GIMP/XCF/C++/
GEGL dependencies are uninstrumented; LSan off. Primary build is an owned-source
snapshot, not a clean-whole-HEAD or crossdomain UI receipt. No screenshots are
claimed for this numerical regression. Original criteria/dependencies remain.
