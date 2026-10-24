# Original23.002: current distinct dynamics/neutral mapping cross-pin

No production or test source changed. Existing registered `painter-dynamics`
five cases cover default/enum, independent Flow/Blending, aggregate serialize/copy,
legacy property key and empty-curve lifecycle. They PASS normally0.144s and with
five C units instrumented by ASan/UBSan (LSan off). Existing registered
`/painter/geometry/default-dynamics` passes normally1.068s and using the same
instrumented model/output/curve/enum replacement archives. It evaluates1028 native
input values against256 immutable old samples, toggling legacy mapping then back
to unchanged modern behavior. Geometry C++ driver and remaining host/dependencies
are uninstrumented. The historical helper's `instrumented_cpp_rtti` field is not
used to claim C++ coverage: its actual five source/compile entries are all C.

The stale output-provider delta was committedd018198984's legacy neutral smooth
curve mapping, not just an unrelated field. A private causal control bypasses only
that mapping and the actual registered case fails exact equality; source is then
restored and the same case passes. No product defect is inferred from that mutant.

## Exact source relationship

26 inputs were sealed before building, including all10 held provider baselines.
Current main and private model/output/curve and both test drivers are whole-file
identical. Relevant Blending enum registration and geometry-options getter bodies
are byte-identical. Main core-enums differs only by appended MyPaint Undo entries;
those are not the tested Blending registration and are not claimed executed here.
`committed-source-proof.json` and snapshots retrospectively verify the exact
recorded main commit, not a fabricated pre-build main-source seal. Private compiled
source archive remains the normative executed input. This is not whole-current-
HEAD or held-diagnostic execution. Prior Options baseline remains pinned.

## Original duty review

All52 original rows, acceptance and shared owners remain unchanged/open. The11
Blending enum/model source roles now have fresh native/source evidence rather than
historical full-unit assumptions. `source-duty-review.json` records that progress
without closing/reassigning original rows or declaring future23.003/004/005 done.
Original allocation/shade/crop/ownership role review remains separate. No old
runtime was rerun; existing default samples are immutable prior evidence.

Authenticated1080×1920x24 Xvfb was used. These are model/curve checks, not new UI
behavior; no screenshot or physical-device claim is made. Raw environment JSON is
not packaged; focused command/source hashes and selected test fields are retained.

24 semantic evidence controls reject altered source/commit/function claims,
missing real neutral path, disabled sanitizer, missing held provider, accepted
causal mutant and failed restoration. No historical stale receipt is rewritten.

## Exact ledger successor
The OPEN audit and runtime receipts above remain immutable historical evidence.
The checker admits only the exact reviewed23.002 ledger successor in
`../smudge-accumulator-closure/rows.json`; it does not relabel this historical
execution as a new run, admit future source changes, or close cross-owned tasks.
