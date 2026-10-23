# Opt-in float mode XCF boundary (13.012, parent remains open)

The preliminary plan to persist runtime enum73–81 in standard PROP_MODE is
superseded. The writer emits standard NORMAL_LEGACY and carries the optional
semantics in the existing GPXCF item namespace: framing remains1; the item
dictionary alone uses version2 with `(uint32 policy, string space, string name)`.
Policy1/nonlinear-rgb plus nine stable names is independent of enum numbering.
Old64–72 modes retain version1, raw mappings and byte arithmetic unchanged.

The transport matrix covers ordinary, Clone and group owners ×nine modes, each
through two saves/reopens. It checks actual standard property wire values,
float mode identity, absence of legacy-mode, exact mask storage, precision,
opacity, group/Clone references and native projected pixels. This is a transport
oracle, not the independent float arithmetic oracle in the separate operation
proof. The latter includes signed/HDR values and float32 computation policy.

Ten capsule schema scenes distinguish a known record with an unknown independent
field from nine unknown, malformed or conflicting envelopes. Ordinary resaves
retain opaque bytes. Editing an unsupported envelope into custom float semantics
must fail before replacing the destination sentinel. Explicit known float→legacy
→ordinary transitions remove the float field and restore version1 while retaining
unrelated unknown fields. This does not license editing unknown semantics.

The prior-port reader is an exact513bec9490 committed source with a test-only
harness overlay, not an old GIMP2.8 runtime. It opens27 actual new-writer files and
performs two unedited resaves each. Standard fallback, exact opaque extension
bytes and fallback-rendered pixels are retained. The older reader does not
understand/render the float operation equivalently; backward editing and arbitrary
upstream-reader preservation are not promised.

Final execution used committed a19f05f9c8 plus the four named XCF
candidate files, with all five platform-paused source paths verified byte-equal
to that baseline:115 normal cases and102 selected sanitizer cases passed. Preliminary primary-build receipts are diagnostics only; they
are not relabeled as clean evidence. A sanitizer rebuild on the primary was
stopped before test execution after its RTTI bridge source differed from HEAD.
The platform-held candidate task is not retried or closed.

The source metadata gate adds the float capsule field without redefining the
original185-field checkpoint. An explicit extension acceptance ledger records
policy/space/name/version/fallback and unknown-field coverage; parent13.012 and
its wider precision/integration dependencies remain open.

## Source receipt phases

The original selected-input archive predates the runtime-loader correction. It
is retained under that explicit name, not relabeled as containing the corrected
script. The four owned XCF files were separately copied before the clean build
(including the new include and uninstrumented image capability diagnostic). The
corrected runner is committed69c6d20815 and its exact bytes match the final
sanitizer source hashes captured before compilation. The validator checks this
union and unchanged-after-run receipts; it does not retroactively claim an
earlier archive contained later script text.
