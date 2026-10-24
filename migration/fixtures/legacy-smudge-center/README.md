# Source-only Smudge center oracle

Pinned `afa43fae3e920210146abed514f136fd49f671b5` functions `blend_pixels`,
`replace_pixels`, and `combine_inten_and_inten_a_pixels` are extracted unchanged;
complete containing files and exact utility macros are retained with Git-blob and
SHA256 identities. `verify-source.py` checks them. Native fixture wraps these in a
namespace, forwarding every actual current native paint callback unchanged.

The adapter tracks one fixed accumulator center cell, initial successful footprint
overlap, clamped initial sample, and no zero-opacity updates. It observes the
current drawable sample before each actual dab, then evaluates old blend and
composition independently. Solid bitmap masks and constant paper make center
coverage known independently. Whole source raster/partial-alpha/interpolation
parity is NOT inferred from this bounded center oracle. Existing immutable old
48-scene and1024-vector traces remain separate regressions, not newly executed old
runtime. All new alpha-format scenes start opaque.

Old `gimp_brush_core_solidify_mask` in retained `old-brushcore.c.source` allocates
one byte per pixel and writes0/255. The current native helper returns `Y float`
0/1, and paper preserves the incoming format. Native fixture asserts actual
returned dimensions and float center values. Treating that allocation as bytes
is not a valid implementation of old coverage. Final Smudge conversion preserves
the byte fastpath and converts float coverage after paper.
