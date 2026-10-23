# Layer-mask owner representation absence

Supporting evidence for 12.015/field-owner-checkpoint, still DOING. The same
native channel-property reader also constructs layer masks, but with a distinct
constructor and owning-layer attachment. Sixteen presence subsets of byte/float
color and opacity now run through that real mask route, then native Save/Open.
Each round checks the owning layer backlink, name, all 7,680 white mask pixels,
RGB within the unchanged 2e-7 and alpha within 1e-12 bounds. Defaults remain
black/opaque. Tags become same-size unknown records; payloads/offsets remain
intact. This is absence coverage, not malformed truncation.

The prior channel case and new mask case pass in separate exact-path normal and
focused ASan/UBSan processes, mandatory one TAP case each. Existing channel
expectations were not weakened by the shared helper. No production change was
needed after the preceding independent-alpha loader fix. The initial runner
stopped at a wrong include pathname before compilation; its correction is
recorded and not counted as a test.

The selected sanitizer builder retains its existing instrumentation and
RTTI-only scope recorded in sanitizer-build.json, with LSan disabled. Immutable
compiler inputs are archived; independently evolving source outside these two
owned test files means this is not a final integrated-tree claim. Previous
full-suite receipts stay unchanged. Current full aggregate, recursive graphs,
truncation, all remaining field owners and platforms remain open. No UI or
physical-input test is claimed.
