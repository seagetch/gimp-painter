# Corner CENTER admission — bounded25.009 checkpoint

The CENTER enum is the GObject property default and factory-admissible, but neither
old nor native corner code defines center geometry: its switch branch leaves x/y
unassigned. Old wrapper documentation says “which corner” and describes a corner
of an existing rectangle. Eight resize directions map directional anchors. Native
rectangle center control uses a separate CROSS handle. Complete pinned caller,
enum, property and factory sources are retained in corner-contract-source.

Policy: CENTER deterministically produces no ink and no extents, rather than
inventing a centered rectangle. All eight directional transform/draw/extents
branches remain byte-identical to the pre-fix native module. The three changed
blocks initialize the CENTER transform outputs and suppress CENTER draw/extents.
No valid directional geometry or dimension policy is changed.

The initial prototype wrongly required nonempty CENTER ink; that diagnostic is
preserved and is not claimed as a memory-sanitizer failure. After documenting the
contract, the unchanged pre-fix native source fails the actual no-extents default
criterion. This separate expected-red and its exact source/test snapshots are kept.

Five native registrations per normal/ordinary Meson ASan+UBSan run:
*default: direct GObject default and both factory outside settings => NULL extents,
 zero recording-surface ink;
*directional:192 conservative ink-bound scenes, eight anchors xoutside/inside x
 0/37/90degrees xboth H/V flips;
*three actual-window cases:192 directional-to-CENTER transitions total. Initial
 visible frames receive full exposure; return to CENTER uses real begin/end-change
 invalidation only. A470x470 image ROI must return to baseline with zero pixels
 differing by more than2 channel levels. No forced full redraw masks stale ink.

Authenticated1080x1920 Xvfb, oncanvas enabled, actual37degree root-window screenshots.
Each scoped invocation retains60seconds; O2/assertions on/debug symbols off,
ordinary ASan+UBSan, external dependencies uninstrumented, LSan unavailable.
The generic projection/capture helpers are source-bound from the existing grid
fixture. Prior handle18+18 proof is replayed immutably, not a fresh aggregate.

Original25.009 and all196 original work rows remain open. No old application
runtime, old-native pixel identity beyond unchanged directional code, all-item,
physical-device/platform or whole-branch completion claim. Old runtime is
USER-SKIPPED. Corner invalid internal-view behavior outside CENTER is not broadened
by this narrow admission fix.
