# Original 14.013: source connection replacement

The new native model case checks 104 real signal-handler cardinalities across
three A→B→same-B→A replacement cycles and final detach. Attached Clone deltas
are one update, one name-changed, one disconnect and two notify handlers
(frozen/image). Each live update forwards once; retired and detached sources
forward no update. Production code is unchanged.

Selected clean native and focused ASan/UBSan each pass five Clone cases: this
connection case and four committed Clone history/permission/ownership cases.
The latter are a cross-check against the frozen explicit publication-outcome
successor, not a rerun of the predecessor's entire 69-case Clone suite.
The matching prepared fixture passes 18 native cases.

The clean checkpoint is baseline 81bc24545d plus explicitly named overlays in
source-contract.json, not latest whole HEAD. Sanitizer instruments 10 units,
links 59 RTTI-only compatibility units and leaves other dependencies
uninstrumented; LSan is disabled. Receipts retain executable/object/archive
hashes and linked DSO resolution. Full environment fields were removed from
native receipts. This is model evidence, not a UI or old-runtime execution.
Original criterion is unchanged; prerequisite 14.012 remains open.
