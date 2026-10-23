# 09.015 — high precision and explicit unsupported handling

Original acceptance: unsupported 時も情報を黙って欠落させない.

No production policy change is made. Existing native scalar codecs preserve
native component precision rather than round-tripping pixels through an evaluator
float format. The full 72-format backend suite checks u32/double LSB distinctions,
half boundaries, HDR/negative values, alpha, sampling, composition, rollback and
profile/TRC transforms; the actual native 144-cell matrix covers image format and
profile identity, history and rollback. These suites are rerun, not inferred from
format names or earlier commits.

New tests refuse unsupported premultiplied/Lab/XYZ/CMYK models and noncanonical
component order before writes, with useful errors and unchanged native bytes.
Actual native RGB and Gray images reject wrong-model, Lab and CMYK device-link
profiles repeatedly without changing pixels, precision, profile, buffer identity
or existing history. A subsequent atomic stroke is usable and rolls back exactly.
The fixture deliberately uses the batch API for whole-operation rollback: default
non-batch brushes may already commit a segment after leaving the selection.

The inherited 358-line Pixel feature duties from both read/write ancestry remain
whole-hunk TODO with explicit 31.017 ownership. Exact original identities and
acceptance texts are retained; no downstream consumer is silently closed. Native
byte Gray's intentional red-component policy and the old193-record oracle remain
unchanged. Unsupported native profile assignment is explicit, not a fallback or
an arbitrary new profile-name whitelist.

Evidence is model/native pixel validation, not rendered UI. Twelve selected native
units plus standalone Surface are instrumented; libgimpcolor/LCMS dependencies are
not instrumented, native vptr and all leak detection are off. Earlier09.014 evidence
replays immutably; this successor supplies the new current precision harness hash.
