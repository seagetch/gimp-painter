# 09.015 native precision and explicit refusal

This checkpoint verifies the existing implementation, without changing production
or historical byte color policies. Six noncanonical models are repeatedly refused
with useful exceptions and unchanged raw patterned storage; a BGR-order format
with an otherwise canonical RGB model is also refused. No fallback conversion is
performed. Actual native RGB/Gray double images repeatedly reject wrong-model,
Lab and CMYK device-link ICC profiles, preserving pixels, native buffer/format,
profile, precision, seeded Undo/Redo and preview state, then successfully execute
and atomically roll back a fresh stroke. Twelve ICC attempts are counted.

The unchanged full native precision144-cell suite and all eight backend groups
(72 formats, exact u32/double LSBs, half, HDR, alpha, native selection, background,
composition and independent analytic/LCMS profile comparisons) are rerun. Normal
registered tests total16 TAP groups. Standalone precision8 + refusal2 groups pass
ASan/UBSan, and the new native ICC group passes selected12-unit instrumentation.
The native image profile owner is instrumented; libgimpcolor, LCMS and other
shared dependencies are not. Native vptr and all LSan are explicitly off.

Three actual production mutants are rejected: accepting noncanonical component
order, narrowing u32 values through float, and falsely reporting successful
unsupported native profile assignment. The final restored native test passes.
The initial mutation selector mistakenly selected zero cases; that excluded log
is preserved and is not counted. The corrected exact selector runs one actual
case and fails at the intended assignment-result assertion.

The first recovery fixture used a non-batch default brush. After it crossed the
selection boundary the engine had already committed a segment: diagnostic active=0,
Undo depth1→2,470 byte differences. Canceling an inactive core cannot undo history.
Both failure receipts are retained. The final fixture explicitly requests atomic
batch semantics, asserts actual painting with unchanged history, then checks exact
rollback. No production behavior was altered to satisfy the mistaken fixture.
An initial selected sanitizer builder tried to locate a shared-library unit in
static link objects; its truthful missing-unit assertion is preserved, then the
correct12-unit scope was rebuilt and run. No shared-library instrumentation claim.

The original full358-line scalar duties inherited from write and TempBuf proofs
remain TODO under31.017.09.016/017 boundary and Undo tasks are not closed here.
Earlier09.014 evidence is replayed immutably; this successor supplies current
harness sources. Native runs use verified portrait Xvfb, but these are model tests,
not visible UI evidence. Sources, sanitized receipts and hashes are bound by
verify.py; --check-source additionally compares current owned code.
