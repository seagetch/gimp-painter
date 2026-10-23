# 15.006/filter-pickable-opacity

325 exact double-zero opacity queries coexist with actual visible cache and
native Gaussian refresh: three image projection pixels, three pixel/cycle/bounds
picking controls. The loaded green cache renders despite being unpickable;
hiding it reveals blue source, then supported native Gaussian refresh follows
red source. No Edge or old runtime executes. The first fixture used unsupported
generic Gaussian radius0; its setup failure is retained separately, not claimed
as a production defect. Radius1 on uniform color is the verified fixture.
Selected native1 plus this case within combined Filter3 sanitizer pass.

This is checkpoint-scoped model evidence, not UI, all-artwork, whole-HEAD or
old-runtime proof. Sanitizer scope/counts and exact source/object/executable
hashes are retained. Remaining dependencies are uninstrumented; LSan disabled.
Original15.005 prerequisite and criterion remain unchanged.
