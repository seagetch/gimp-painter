# 25.009 inherited fractional-origin bridge

## Original acceptance remains represented

- `25.009`: 現行表示行列へ接続する — canvas と overlay が同じ角度を使用.
- `25.009/drag-zoom-scroll`: 修飾キー付きdrag zoomと変換座標panを維持する — Shift回転・Ctrl scaling・通常scrollの開始/drag/終了、zoom距離300、device→image変換と原点を旧操作fixtureで比較する.

Neither row, criterion nor dependency is removed or weakened by this checkpoint. Historical common-angle, transformed-drag and fractional-difference contracts/receipts stay byte-for-byte historical evidence, not fresh results for this source. In particular the historical fractional-difference case genuinely ran as the default policy; it was not secretly custom. Its current integer-device expectations now run under an **explicit custom binding**, while default pan is checked independently against unchanged old source. The original entire display/overlay acceptance and full old-operation zoom/pan acceptance require their own final review; this checkpoint is not a physical-device, old-runtime, all-renderer or other-platform proof. User-skipped old Edge/string execution is untouched.

## Implemented correction

Pinned old source uses double offsets and narrows only the inverse-transformed press to `gint`. Native GIMP's integer offset fields remain integer and in their original positions. Four appended doubles store device-origin fraction and old press anchors. The shared rotate/inverse matrix includes the fraction, including angle zero. Default middle pan computes exact old pre-rotation clamp and converts the same visible image origin into native integer-plus-fraction coordinates. Fractional motion invalidates the raster cache fully. Explicit custom/integer operations retain native policy; explicit origin/grid reset paths clear the fraction, while pan release preserves the displayed origin and clears anchors.

Finite/range checks precede native pointer-coordinate conversion and tool-focus updates. A raw device point inside gint range can still inverse-transform beyond the canvas-item center property's range; that previously produced a fatal GLib critical in the new negative test. Such input is now ignored without losing a held grab; a valid initiating-button release still cleans up. Pan begin reports inverse-range rejection before grabbing. These checks do not reinterpret real pointer coordinates or quantize valid fractional events.

## Verified evidence

Evidence directory: `migration/tests/navigation-legacy-pan/`. Its README documents source representation, initial failures, explicit exclusions and reproduction environment.

- Genuine unchanged-production expected red: old visible center `(371.00602758221567,233.98001204063144)`, native `(370,233)`, SIGABRT.
- Five unchanged old bodies/suffixes pinned to `afa43fae3e920210146abed514f136fd49f671b5`; real Cairo source-only oracle, no old executable.
- **35/35 ordinary native tests**, 24.5182 s; all original31 registrations retained plus4 new cases.
- **35/35 ordinary Meson ASan+UBSan tests**, 32.7714 s; assertions enabled, no hand-substituted objects. LSan off for platform ptrace incompatibility; pinned dependencies and the gimp-data build-time colorsvg2png generator are not instrumented. This is not an all-target build claim.
- Related **4/4 native UI and 6/6 pure navigation** tests pass.
- **24 scenes, 1,920 real GTK motions, 5,760 independent point/inverse pairs**: 0/37/90°, all H/V flips, isotropic/anisotropic resolution, off-center and fractional press/motion, repeated subpixels, return, both clamp directions and reverse. Equivalent legal old H/angle states represent vertical/both flips without changing old code. Shift/Ctrl changes preserve the historically latched inherited pan.
- Separate oncanvas-enabled raster ROI, actual item paint CTM, upright child stored anchor and normal GTK allocation tests. Original whole-canvas trail detector remains unchanged in its original UI environment; toolbar color pixels are not silently ignored there.
- Explicit custom pan preserves its integer delta policy after a fractional default pan. Same-integer set-offset, zoom, rotation and dot-for-dot changes have independent reset tests. Space remains the separate native dynamic-modifier policy, tested but not relabeled old-compatible. Escape does not invent a historical rollback policy.
- Actual authenticated **1080×1920×24 Xvfb** root screenshots, `portrait-oncanvas-pan-m0.png` and `...-m1.png`; visible oncanvas controls and real raster. No montage or image editing.
- Source hashes, selected receipts (without raw environment dictionary), screenshot dimensions and exact old Git bodies validated by `check-evidence.py`; **10 negative + 1 positive controls** in `test-evidence.py`.

The build is an isolated exact committed base `ce40ac4cffa5b6dba48cd9100303c3c94653a5bc` plus the manifest-listed task changes. Unrelated uncommitted popup/brush/perspective work was not copied. This is an exact task-source proof, not a claim that all current combined worktree/branch sources were rebuilt. Original whole-task and aggregate/current-branch acceptance remains a separate owner responsibility.
