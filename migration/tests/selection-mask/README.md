# 09.006 native selection integration

Three native groups are added to `gimp-painter-surface`; the existing 12 groups
are retained. See `../../contracts/selection-mask.md` for exact oracles and limits.
The tests execute actual current GimpPaintCore/PaintCore/Surface code and actual
selection channels; there is no copied renderer or replacement native core.

Native result files are produced only after execution, contain selected Meson
result fields, and exclude inherited environment maps. Display verification uses
the authenticated Xvfb runner at portrait 1080×1920×24. These are pixel-buffer
acceptance tests, not a visual UI acceptance or full-app sanitizer claim.

The first runtime attempt passed the original 12 and the two integration groups,
but the new scalar oracle incorrectly assumed an U8 image selection retained
0.25 rather than quantized 64/255. The corrected independent oracle explicitly
rounds U8 values and additionally tests float/double image precision. This is a
fixture correction, not a production defect or reduced tolerance. The sanitized
failed result is retained under `excluded-fixture-attempts/`.

The corrected baseline passed all 15 native TAP groups in 1.60s at the unchanged
60s target budget. The first pre-runtime attempt encountered an unrelated shared
Meson registration whose dispatch source had not yet been synchronized. The
owning task supplied that source; selection changes never copied whole Meson.

Both actual production mutations compiled and failed at the intended assertion:
zeroing the PaintCore mask offset broke translated-scene equality; bypassing
Surface's native coverage multiplication broke the exact alpha oracle. Each
source was restored in a finally block and rebuilt. The final restored aggregate
passed Surface15 + TempBuf6 + coordinate/write7 (28 TAP), with no skips/timeouts,
on authenticated portrait Xvfb. `run-mutations.py` requires the caller to hold the
shared build lock and leaves production sources restored even on a failed check.
