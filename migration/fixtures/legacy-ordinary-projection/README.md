# Direct old ordinary projection capture

This supplements the sealed legacy-runtime fixture; it does not replace it.
The real pinned old application reopened ordinary-layers.xcf, then copied the
image projection through the ordinary gimp-layer-new-from-visible PDB procedure
and exported that layer with transparent RGB preserved. Capture exited0.

All30,720 decoded RGBA8 channels equal both earlier merged PNGs. The observed
modern projection difference (910 of7,680 pixels, at most1 RGB unit, unchanged
alpha) therefore cannot be dismissed as a merge-versus-projection comparison.
The exact ordinary-compositing gate stays open pending a real arithmetic fix.

The script/log, input and executable hashes, decoded bytes and sealed manifest
record the measurement. No paint, composite, XCF or projection source code was
changed for this capture. Binary build compatibility overlays are documented in
migration/baseline/legacy. No screenshot/UI or cross-platform result is implied.
