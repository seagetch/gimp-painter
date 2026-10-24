# Original23.002 initialization-before-opacity checkpoint

Keep the old first-successful-overlap seed even if that contact is transparent.
Fully outside contacts do not initialize. Once initialized, zero-opacity dabs
must not update the accumulator or drawable. Later visible dabs expose the seed
with unchanged old byte arithmetic; lifetime/admission checks remain intact.

Verify exact same corrected native fixture against original and candidate code,
including actual positive dab counts, changed bytes, clipped/negative/outside
entry, repeat zero motions and Undo/Redo. Re-run all captured48-scene and1024kernel
regressions normally and under explicitly scoped sanitizers. All52 source duties
and original dependencies remain tracked/open; no blending/size/platform closure
is inferred from this bounded fix.
