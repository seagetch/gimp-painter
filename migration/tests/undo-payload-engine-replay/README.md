# Committed Engine compatibility replay

After the immutable09.017 checkpoint (27de31a8bd), rerun the same native Surface24 cases against committed Engine ac7af04669. Exact tested archive includes the concurrent prepared-Undo helper candidate in `app/core/gimpimage-undo.c`; this is not a whole-HEAD or released-build assertion.

Normal24 and selected11-unit ASan/UBSan24 pass under authenticated 1080×1920 Xvfb. Both runs include32 native Undo payload precision cells and three Undo/Redo cycles per cell, plus all prior Surface registration/lifecycle/selection/layer-mask/snapshot/dirty tests. The original09.017 evidence is unchanged. No additional task, whole original source duty, rendered UI, paired engine/stroke Undo, whole-app instrumentation, vptr sanitizer, or leak-detection claim.

`verify.py` replays the immutable receipt and binds Engine to the committed Git checkpoint; `--check-source` additionally requires all exact tested current sources. Subsequent helper/controller work must retain this receipt rather than reseal it.
