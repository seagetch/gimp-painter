# 08.017 first subset: native editor layout and common stores

This is an interim verified fix based on `f28e0260d39ed282250e004d6a8330a036505b4b`. Original 08.017 and all 93
source duties remain TODO; no task count or ledger was advanced.

The original active native editor constructor requested non-homogeneous buttons.
The current path still forced equal widths. A focused baseline test failed at
that exact condition; the minimal FALSE correction passes real unequal-width
allocation in LTR/RTL and style refresh. No store production code changed.

New controls check unchanged store identity across presentation/context reuse,
independent owners, explicit close and rejected reopening, inert retained editor
controls, and MyPaint popup notebook-page retention through show/popdown and
layout/restore cycles. The first page test confused weak disposal notification
with finalization; its failed run is disclosed in baseline-red.json. Corrected
qdata-destruction counters pass on the unchanged baseline.

The first expanded sanitizer run exposed a pre-existing empty-array memcpy
contract violation in libgimp procedure configuration. The separate prerequisite
commit guards the two procedure copies and the downstream generic-config copy.
Its registered five-case regression and exact red/green evidence are retained in
migration/tests/procedure-config-zero-count. The results here are fresh complete
reruns with that prerequisite applied; the failed run is not represented as a pass.

The standard native and ASan/UBSan builds each pass 205 cases across five selected
registrations: layer UI 63, canvas 44, compact options 14, MyPaint editor 14 and
foundation 70. Sanitizers retain RTTI/vptr; LSan is disabled and third-party
dependencies are not instrumented. Reports seal final sources and executables.
This is neither the whole aggregate nor cross-platform/tablet acceptance.

source-contract.json preserves the exact original duty and states the remaining
source/API integration work. No old feature is declared equivalent merely because
its source file or API name is absent. Existing type, feature and WBS obligations
remain separate.
