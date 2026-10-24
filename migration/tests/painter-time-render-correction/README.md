# Erratum and separate timestamp render proof

## The earlier visual claim was invalid

Commit `b804577710` retained `painter-time-sequences/timestamp-wrap-oncanvas-portrait.png` with SHA256 `01fce03ca824e5c172f0a5e17e40a34c8d425377799a2004db0acece22976dbd`. It shows a mostly black desktop and small blank canvas, **not the red stroke/oncanvas presentation claimed in that package README**. That image is not accepted visual evidence.

The worker viewed an earlier red capture, then final normal/sanitizer runs reused the same filename and overwrote it. The final copied image was not viewed before the claim. The final sanitizer was the last recorded capture writer; landscape control receipts contain no capture. The exact reason that later view was blank is not established by this correction. A logical visibility flag alone did not prove the target window was active/mapped or that the recorded screen showed the stroke.

The original package, bad image, receipts and source fixture remain byte-for-byte unchanged. Original seven normal + seven focused sanitizer timing/alpha/raster/Undo assertions remain historical raw-buffer/input evidence, **not validated visual proof**. Original23.009 and dependency scope remain open.

## New independent render cases: one normal + one focused sanitizer

A separate private fixture reuses the same wrap input and normalized-reference assertions, then performs presentation-only operations after timing/alpha assertions: selects and raises the target image window, sets5×zoom, evaluates projection, and waits for presentation. No product or shared capture helper changed.

Each run has a new exclusive directory and empty GIMP3_DIRECTORY/HOME/XDG profile; reuse is rejected. The screenshot is hashed immediately in that run's receipt. It asserts:
- actual target shell is active; canvas and target window are mapped;
- root and target window both1080×1920, oncanvas enabled;
- the mapped MyPaint Editor button is fully onscreen at108,1813;
- actual root screenshot at412,851 is RGB218,55,55, a red stroke pixel.

Both cases pass and independently produce identical PNG SHA256 `919b96d41d682f94b7038fe52e58489fa437fa371366ce8aa5659f564aededcf`. `normal-render.png` and `sanitizer-render.png` are final copies from their distinct run directories and were individually viewed after copying. They show actual red stroke and oncanvas controls, not an edited/generated image.

This remains direct registered-tool input, not queued GDK/physical tablet/Windows proof or automatic projection-notification proof. The extra sanitizer case instruments only the fixture and the native MyPaint tool provider; dependencies/evaluator are uninstrumented, LSan disabled. Missing icon/theme/session-bus warnings remain. No old evaluator behavior is replaced.

Exact host remains released `0a775df4135052a2c851c9a5dfa37cea25887268`, with shared read lock through build/run and source/runtime hash checks. A fixture compile attempt encountered the known C-only image-window header member named `private`; the corrected C++ fixture uses its exact opaque C declarations, separately compiled against the real C header without conflict. This is not a product header fix.

`validate.py` verifies run-time screenshot hashes, asserted pixel/mapping evidence, source/build receipts, and byte-exact preservation of the earlier package. This successor supersedes only the invalid visual claim; it does not erase or silently rewrite historical evidence.
