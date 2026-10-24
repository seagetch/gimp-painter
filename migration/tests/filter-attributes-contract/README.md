# Actual Gaussian cache ordinary attributes — original 15.006

**Native1 + selected ASan/UBSan1 PASS.** 27 scenes cover Normal legacy, Multiply legacy and Painter Replace × opacity 0/0.5/1 × mask 0/128/255. Each scene executes a fresh Gaussian, confirms blur across the color boundary, and compares two projection samples with a separate ordinary layer seeded from that completed cache. All54 comparisons match exactly. Eighteen Normal samples also match independently calculated nonlinear source-over color within one u8 rounding unit. Projection reads never start another runner or change cache bytes.

No provider defect or production change. This is model/projection evidence, not a new UI test or independent Gaussian numerical oracle. Other modes, precisions, spatial mask patterns and groups are not claimed. Old afa43fae parent-projection delegation is read-only evidence; default Edge and held diagnostics were not executed.

Snapshot: exact2ef base plus previously sealed readiness, Filter property and dialog-help overlays, then these two test-only files. `predecessor-excerpt.json` identifies the full predecessor manifest hash and relevant source hashes.144 archived input files, pre/post source equality, owned-file hashes and20 instrumented+53 RTTI-only sanitizer units bind the result. Other dependency units remain uninstrumented; LSan disabled. Not whole-current-HEAD proof. Portable `python3 migration/tests/filter-attributes-contract/validate.py` checks artifacts, source archive, test counts and three result negative controls.

Original15.006 acceptance and dependencies are unchanged; status is DOING, not closure of preceding15.005 or surrounding migration gates. Prior pickable-opacity and editability evidence remains separate.
