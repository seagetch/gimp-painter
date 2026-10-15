# Pixel-format correspondence (09.001)

This mapping describes the old representation and its explicit GIMP 3 counterpart;
it does not certify byte-identical rendering, parallel processing, conversion,
or native high-precision feature support. Source reference is
`afa43fae3e920210146abed514f136fd49f671b5`. The adjacent JSON pins each of the 13
assigned source obligations, its exact blob/range/hunk identity, whole-blob SHA256,
individual interpretation and executable check. No source duty is removed.

## Legacy byte representations

| Legacy image/storage | Channels, byte layout | GIMP 3 counterpart |
| --- | --- | --- |
| RGB / 3-byte TempBuf | R, G, B unsigned bytes | R'G'B' u8 in the image's layer space |
| RGBA / 4-byte TempBuf | R, G, B, A unsigned bytes | R'G'B'A u8, separate unassociated alpha |
| Gray / 1-byte TempBuf | Y unsigned byte | Y' u8 in the image's layer space |
| Gray-alpha / 2-byte TempBuf | Y, A unsigned bytes | Y'A u8, separate alpha |
| Indexed | palette index unsigned byte | per-image babl palette format, 1 byte |
| Indexed-alpha | palette index, A unsigned bytes | per-image alpha palette format, 2 bytes |

Bytes alone are insufficient: a one-byte Gray value is not a palette index.
TempBuf has no color profile, transfer-curve or format field; its 1/2/3/4-byte
interpretation comes from the caller. The nonlinear names above are the legacy
image-byte correspondence, not information encoded in every arbitrary TempBuf.
Legacy image type names come from libgimpbase/gimpbaseenums.h. PixelRegion has
independent bytes and rowstride fields. PixelProcessor's five region parameters
are five buffers, not a five-channel pixel format. Its serial and parallel p5
dispatch, descriptor copies and region registration do not convert formats.

Pixel::data_t is guchar and has 8-bit depth; internal_t is guint32 and real is
float. Those arithmetic temporaries do not turn the storage into u32 or float.
from_f clamps normalized values, multiplies by 255 and rounds with +0.5;
to_f divides by 255. All 256 byte values round-trip through the real old methods.

## Native precision, transfer curves and space

The JSON enumerates all 72 RGB/Gray × alpha × six component types × three
transfer-curve combinations, checked against the actual extracted
`gimp_babl_format`, `gimp_babl_component_type`, `gimp_babl_trc`,
`gimp_babl_precision` and `gimp_babl_is_valid` functions.

| Component | Bytes/component | Storage range |
| --- | --- | --- |
| u8 | 1 | bounded unsigned |
| u16 | 2 | bounded unsigned |
| u32 | 4 | bounded unsigned |
| half | 2 | floating-point |
| float | 4 | floating-point |
| double | 8 | floating-point |

Linear models are RGB/YA/etc.; nonlinear models use apostrophes on color
components; perceptual models use tildes. Alpha has no transfer-curve marker.
Bytes per pixel equals component count × bytes per component. Tests execute all
72 choices in both sRGB and Rec2020, asserting encoding, alpha, byte size, exact
space identity and precision decomposition/recomposition. This checks format
construction, not transformed sample colors or ICC rendering accuracy.

Indexed is valid only at U8_NON_LINEAR. gimp_babl_format deliberately does not
construct indexed formats. `gimp_image_colormap_update_formats` obtains the
image layer space and calls `babl_new_palette_with_space`; its paired formats
are checked using the same API in both spaces. Palette contents are required to
interpret an index; casting index bytes to Y' or RGB is invalid. All other 17
precision/TRC combinations are rejected for indexed formats.

## Layout, conversion and ownership distinctions

Legacy TempBuf allocates width × height × bytes, filled in channel order.
Subwindows alias their parent's storage and retain the parent's rowstride,
including when their own width is smaller. The compiled old allocation and
subwindow functions are tested for all four byte widths with nonzero origins.
The old temp_buf_free returns immediately for a subwindow, leaking its wrapper
unless separately released. The harness explicitly releases that wrapper after
observing the old behavior. This documents an existing defect, not a recommended
modern ownership implementation or leak-freedom claim.

Old temp_buf_copy converts only RGB↔Gray and RGBA↔Gray-alpha when widths differ;
it is not a general conversion dispatcher. Its actual gray conversion multiplies
encoded bytes by 0.2126/0.7152/0.0722 and rounds; it copies alpha unchanged.
The compiled source tests both directions and both alpha choices, and equal-byte
copy. Color-managed babl/GEGL conversion must not be presumed pixel-identical to
that encoded-byte formula. Native extensions require explicit formats and space,
not replacement of data_t with another scalar.

The new rowstride declaration and parent field are specifically accounted for.
Old dump uses packed data size and a contiguous write; it is not evidence of a
row-aware subwindow exporter. Threading, subwindow copy correctness, old wrapper
repair, mask meaning, premultiplied working formats and behavioral migration are
separate follow-on tasks. This table does not close those obligations.

## Reproduction

Source the available relocated dependency environment, then run:

    python3 -B tools/check_pixel_format_mapping.py --report migration/tests/pixel-format-mapping/native.json
    python3 -B tools/check_pixel_format_mapping.py --sanitize --report migration/tests/pixel-format-mapping/sanitizers.json
    python3 -B tools/tests/test_pixel_format_mapping.py

Requires the pinned Git commit, C/C++ compilers, pkg-config, GLib and babl-0.1.
The checker compiles extracted source in a temporary directory, with real enum
values and real GLib/babl libraries. Sanitizers instrument the harness/extracted
code; dependency libraries are not instrumented and LSan is disabled. Negative
controls reject missing/duplicate duties or formats, changed source identity,
wrong byte layouts/encoding, nonboolean alpha and invalid indexed precision.
No full-GIMP or cross-platform pass is implied.
