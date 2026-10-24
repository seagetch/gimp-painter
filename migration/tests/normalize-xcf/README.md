# Normalize XCF: exact recorded contexts, not lost-intent reconstruction

The pinned old writer/type functions emit exactly58 bytes for Normalize: a
length word plus18 NUL-terminated name bytes, INT32 run-mode12, omitted image8,
omitted drawable8 and END8. There are no user parameters. Eight signed run-mode
values, including both INT32 limits, are checked against an independent Python
big-endian serializer under normal and ASan/UBSan hosts. Two writer mutations
are rejected by that independent serializer. This executes only extracted
writer/type code, never the old application or undefined Normalize math.

The decoder recognizes only that exact original name/framing/tag/kind/size/
payload-offset/terminator layout and signed run bits. The old writer did not
store image/drawable identities; current executable placeholders are zero,
while original raw bytes are retained unchanged. Malformed and unknown data
remain opaque. A separately stored native typed model takes precedence and is
not silently replaced by the old capsule, including an explicit unsupported
extra-argument model.

Native tests cover eight signed-context admissions, four defined-domain image
roundtrips with independent shared-channel integer arithmetic, twelve malformed
frames, native-model precedence, and all-transparent domain refusal/cache
preservation followed by valid recovery. No old undefined-domain output is
invented. Current typed data and original raw/cached pixels are separate
preservation channels. Save/Open does not itself promise restoration of an
unserialized transient worker error state; cached/model/error behavior is
checked explicitly in the fixture.

Runtime provenance is a private link over exact55bf baseline, committed Normalize
native core and7e63 wrappers from the prior sealed proof, a pinned f5398 Colorify
fixture, and this owned XCF overlay. No shared clean source/build writes or Video
candidate are used. This is not a latest-HEAD aggregate. The selected XCF
sanitizer scope is documented by actual compile commands; native Filter/kernel
and dependencies remain uninstrumented here, covered by separate predecessor
proofs. Portrait/editor and indexed/Gray/highprecision support remain open;
original16.022 is not completed by this bounded package.
