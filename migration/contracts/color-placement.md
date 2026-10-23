# 09.014 color-conversion placement

Original acceptance: 採色と合成で意図しない二重変換がない.

The already implemented domain boundaries are now verified by registered actual
Surface and native PaintCore tests. No production policy is changed. Foreground
and background enter drawable evaluator space; wide composition converts once to
native working space, and alpha-weighted native sampling converts once out.
Historical nonlinear-byte Gray deliberately retains red-component composition and
replicated sampling, including custom ICC profiles. It is not silently redefined
as colorimetric luminance. The existing 193-record oracle is untouched.

There are no direct inventory rows assigned to 09.014. This checkpoint claims no
whole legacy feature completion and removes no historical obligation. Exact
09.015 precision/refusal, 09.016 boundary and 09.017 Undo criteria remain separately
owned and reverse-linked. Existing source-duty manifests and their successor
links are unchanged. JSON pins both established color policies and original
acceptance; runtime evidence explains independent oracles and tolerances.

Validation: tools/check_color_placement.py, eleven ledger controls, and
migration/tests/color-placement/verify.py --check-source. Tests are model/native
pixel tests, not rendered UI claims. Selected instrumentation excludes vptr,
leak detection and uninstrumented dependencies.
