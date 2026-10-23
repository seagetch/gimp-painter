# Original14.019: retained Clone operation corpus checkpoint

`extract.py` reads only the Clone capture log, preserving30 ordered stages and
238 object-state rows (attached/parent/index/geometry/reference) from the genuine
old-core capture. The268 raw pixel records include all30 image projections.
Original inputs and capture source are SHA-256 inventoried; no old runtime runs.

The new native include replays only `capture_clone()` lines1151–1252 with current
APIs: source/clone translation, source reorder, clone/group duplication, three
independent source edits, source removal/restoration/edit, clone/group removal,
and the original ordinary Undo/Redo calls. It checks old stage models, every
watched raw layer buffer (including retained detached history objects) and final
projection. Old normal maps explicitly to Painter normal; old byte fill inputs
are retained. The observer interval is the original Clone100ms, not the Filter
interval. Capture IDs are observer labels, not persistent XCF IDs.

Important: these are old **live pre-save** captures. The five independent old
XCF smoke opens never established old-reader render or reference equivalence.
This replay therefore does not infer reader correctness from saved end states.
No file load/save, sibling Filter/old Edge runtime, UI input or screenshot is
part of the test. Selected native replay passes all30 stages,238 model rows and
268 raw pixel records in4.42s. The earlier primary probe is retained separately. Clean baselinea19f05f9c8 plus
parent XCF/runtime-loader and owned test overlays also passes selected2/2 native
and2/2 focused ASan/UBSan (corpus plus earlier projection/ROI/detach test). Eight
source units are instrumented;59 additional C++ units only receive RTTI
compatibility, not sanitizer instrumentation. Other GIMP/dependencies remain
uninstrumented;LSan is off. Built libgimp DSOs are explicitly selected and the
runner verifies their identical native/sanitizer resolution. This is not an
unqualified whole-current-HEAD or UI receipt. Original
14.019 dependency chain remains open and this checkpoint is DOING.

The initial adapter used byte76 for original double0.3 based on fixture README
prose. The raw retained captures actually contain77. `seed-bytes.json` checks all
pixels of the three uniform old seed states and records the corrected byte
inputs. The first failed adapter receipt is retained, explicitly not labeled a
production regression. No expectation or original raw capture was rewritten.
