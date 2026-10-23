# Native scalar selection fixture repair (09.006 follow-up)

A later complete Surface run exposed a fixture-only precision assumption:
generic Y-float input written into a double native selection buffer physically
stored0.25000000903060116 rather than the independently requested0.25. The
Surface faithfully read that native value; its unchanged1e-12 oracle failed.
An isolated group stored exactly0.25, so a green isolated rerun was not accepted
as resolution. The full-suite diagnostic records the erroneous stored value
before any Surface read.

The fixture now writes scalar values in the buffer's exact native Y-u8,
Y-float or Y-double format, bypassing unintended color-format conversion.
U8 coverage is independently rounded to byte storage; floating quarter-values
remain exact. Every write is immediately read back in that same native format
and compared with the independently generated fixture. The downstream coverage
oracle and1e-12 tolerance are unchanged. Production Surface code is unchanged.

Verification includes the committed18-case Surface harness with only this
fixture corrected. Ten further full19-case runs (190 TAP cases) exercise the
pending snapshot test too; these extra cases do not claim09.008 completion.
The committed harness source hash is verified against its recorded git revision.
Selected result fields exclude raw Meson environment maps. The prior failure
and isolated/full diagnostic results are retained, not relabeled as passing.
