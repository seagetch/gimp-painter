# Genuine legacy Gaussian alias reference

Captured with the existing pinned gimp-painter executable corresponding to
`afa43fae3e920210146abed514f136fd49f671b5`. The report records the exact GIMP,
blur-gauss and file-png binary hashes, procedure source hash, invocation, and
script/log/input/output hashes. No source patch, rebuild, XCF reader, or ported
kernel was used to generate expected pixels.

`capture_gauss_aliases.py` prepared and executed 88 full-output PDB cases:

- `plug-in-gauss-iir` and `plug-in-gauss-rle`: one radius with independent integer
  horizontal/vertical flags, including noncanonical truthy values `2` and `-3`
- `plug-in-gauss-iir2` and `plug-in-gauss-rle2`: independent double radii
- Native opaque RGBA and Gray-alpha 9×8 inputs; 67×66 RGB tile-boundary cases
- Radii 0.25/0.75/1/2.5/3/5/7.25/25 where applicable, including disabled axes
  and the old IIR-to-RLE fallback when either axis radius is at most one
- Twelve positive-radius, both-flags-disabled calls succeed and preserve every
  input byte. This differs from the canonical/two-radius PDB calling-error rule
- Eight separate negative probes confirm calling errors: nonpositive single
  radius, or both independent radii nonpositive

All three inputs were independently loaded and re-exported by the old program,
and their channel-native bytes were unchanged. PNG decoding uses Pillow's
native `RGBA` or `LA` bytes, without RGB/Gray conversion. The `.rgba`/`.ya` files
contain tightly packed native channels. `fixtures.tsv` gives procedure, width,
height, channels, number of procedure-specific options, the three option slots
(the last is padding for two-option aliases), input, and expected output.

These are whole, unselected, opaque U8 PDB fixtures. They do not certify arbitrary
procedure execution, selected regions, mixed-alpha lower-stack projection,
modern precision, UI behavior, or XCF round trips. The separate
`legacy-gauss-negative` corpus resolves the region/shadow behavior of negative
disabled radii, including successful empty-region calls with an old diagnostic. The existing canonical Gaussian
corpus independently covers mixed/zero alpha and numerical kernel behavior.

The capture helper refuses to overwrite sealed evidence or retry an unsealed
capture with an existing log. Use a fresh output directory for an intentional
new capture after checking retained process outcomes.
