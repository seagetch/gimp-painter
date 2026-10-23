# Original v1 curve validation and roundtrip: bounded proof

No decoder production change. The existing native parser already removes the
old unconditional skip. New tests cover all405 canonical setting/input pairs,
three transformed old aliases (hue, saturation, value), negative/zero/positive
x coordinates,14 malformed lists, and encode/reopen preserving every point.
Current Mapping intentionally permits equal-x discontinuities; a duplicate-x
case pins that current policy rather than pretending the broken old parser
provided runnable strict-order semantics. Descending/nonfinite/single/odd/
oversized/malformed point lists are rejected.

10 normal groups and10 selected3-unit ASan/UBSan groups pass, including the177
unchanged shipped v2/v3 resource roundtrip. Resource, Engine and test translation
units are instrumented; dependencies/libapppainter are not, vptr and LSan off.
Three causal compiled mutants fail the new v1 matrix: skip points, omit y
transform, permit descending x. Their diagnostic outputs are retained.

The pinned old parser source has `if (!(x_val > prev_x));` and unconditionally
skips point assignment; this is source analysis, not new old runtime execution.
Tested production is exacta84cd with only this pure test replaced; all hashed
sources are archived and unchanged after build. No held provider is imported.
These are model tests, no new UI claim. The original prerequisites19.005 and
19.014 remain open, so both original task rows remain DOING, not DONE. Broader
loader/stream/icon/old ABI obligations remain separately tracked.
