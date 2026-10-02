# Genuine legacy mode projection and merge captures

The pinned old GIMP-Painter application executed 56 valid 8×8 RGB scenes through
ordinary PDB layer creation, opacity/mask editing and image procedures. All
scenes completed with process exit 0. Each contains two different RGBA8 color
patterns; each alpha axis independently takes 0, 1, 64, 127, 128, 192, 254, 255.

Modes 23–29 are measured at eight opacity/mask combinations: 100%, 50%, 0% without
mask; 100% and 50% with mask 128;100% with mask 0; 1% with mask 254; 99% with mask 1.
Every scene produces two distinct reference outputs:

- `*-projection`: `gimp-layer-new-from-visible` copies the actual image projection
- unsuffixed: `gimp-image-merge-visible-layers` merges the original two layers

The two routes differ in hidden RGB when the bottom alpha is zero. Merge starts
with a cleared destination and combines the bottom layer; direct projection
uses initial-region copying. Both outputs are retained; tests must identify
which route they compare rather than treating these references interchangeably.

`capture.scm`, `capture.log`, `exit-code.txt`, the authored input PNG/raw bytes,
all 112 output PNG/raw buffers, and `capture-report.json` record the actual run.
The report includes pinned source identities and verifies relevant compositor,
layer and merge files are byte-for-byte the unmodified reference source. The
executable hash and all output hashes are recorded. The previously documented
build-only overlays and diagnostic logging remain in that executable.

`generate-capture.py` records the exact input/script generation. Its absolute
paths describe this execution environment. PNG export uses `file-png-save2`,
transparent RGB preservation, compression 9, and metadata flags disabled. Decoding
uses Pillow RGBA/tobytes without numerical transforms. The manifest seals all
package files except itself. No GUI/display, higher precision, indexed/gray,
offset/group or cross-platform parity is claimed from these 56 RGB scenes.
