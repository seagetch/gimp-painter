# Original14.007 indexed source projection

One native model test checks24 scenes: indexed/indexed-alpha sources ×three
opacities ×no mask/applied mask/disabled mask/shown mask. All144 pixels compare
palette RGB and binary alpha exactly. The independently pinned source oracle is
old initial_indexed_pixels/initial_indexed_a_pixels: nonalpha ignores opacity
and applied mask, while alpha uses rounded INT_MULT3 and strict >127 threshold.
Shown-mask gray uses a palette entry, avoiding accidental palette-quantization
ambiguity. Six alpha values include both127 and128 threshold neighbors.

This is source-derived arithmetic/dispatch evidence, not newly executed old
runtime, arbitrary artwork parity, non-RGB cross-image semantics or GUI proof.
No production change was needed. Native1 and focused sanitizer1 pass; sanitizer
instruments8 units with59 RTTI-only compatibility units, other dependencies
uninstrumented and LSan disabled. Its source checkpoint is the parent clean
81bc baseline plus previously named metadata/history/resource/pixel-preparation
overlays and only this Clone test overlay, not latest whole HEAD. All eight
paused-provider paths were checked byte-identical to81bc before this run; their
hashes are retained with exact provider/test hashes. Full environment fields
are omitted from the native receipt. Original14.006 prerequisite remains.
