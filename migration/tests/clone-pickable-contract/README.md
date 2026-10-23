# Original14.003/clone-pickable-opacity

Selected native model test (one case) passes with both an ordinary source and a
group source containing a referenced child. It checks648 point opacity queries,
including outside coordinates, all exactly0.0 under the current gdouble slot.
Six final projection pixels prove that this picking contract does not hide the
reference cache or its child updates: red source, blue update, then green base
when clone visibility is disabled. Pixel picking and cycling skip the Clone,
while bounds picking includes it (six assertions).

This retains old integer-zero picking semantics without changing rendering or
pretending Clone is itself a group. No production code changed. This is a
selected primary-build model-only receipt, not GUI/clean-whole-build/sanitizer
proof. The primary workspace has unrelated sources outside this narrow test;
no paused popup/canvas/MyPaint runtime was invoked. Clean proof remains queued.

## Clean follow-up

`clean-native.json` and `clean-sanitizer.json` pass the same single case on the
parent-prepared81bc24545d clean checkpoint, plus four parent-owned metadata
files and only the owned Clone harness/include and runner overlays. This is not
latest whole HEAD. Exact owned source hashes, unchanged archive hashes, linked
DSO resolution and executable hashes are retained. Sanitizer instruments8 units
with59 RTTI-only C++ units; remaining dependencies are uninstrumented, LSan off.
No GUI interactions are claimed. Dependency14.002 remains open.
