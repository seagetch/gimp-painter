# Fixed-size channel-property length recovery

Bounded checkpoint for **12.015/field-owner-checkpoint**, still DOING. This is a
compatibility test, not a parser hardening change or a finding of exploitability.
The existing native loader consumes its fixed known payload size regardless of
the declared size for OPACITY, FLOAT_OPACITY, COLOR and FLOAT_COLOR. The initial
40-case observation established that all supplied mismatched lengths were
accepted with one intact channel. The test deliberately preserves that policy.

The strengthened test uses the public native writer, mutates only one declared
length, and enters the registered native Open route. Four properties times ten
lengths (0,1,2,3,4,5,11,12,13,UINT32_MAX) retain complete actual payloads and all
original object offsets. It asserts successful Open, exact channel identity and
geometry, RGB within 2e-7 and float alpha within 1e-12; subsequent native Save must
emit all four canonical lengths again. This does not test genuinely truncated
payloads, every owner/property or arbitrary recursive graphs. It does not assert
that unknown records may ignore their declared lengths.

No production source changes are part of this checkpoint. The previous valid
property-order regression and all original field tests remain active. Evidence
reports identify final native and focused sanitizer counts, instrumented source
scope and snapshot limits. No UI interaction claim is made.
