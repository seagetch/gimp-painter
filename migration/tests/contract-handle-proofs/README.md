# Proof-aware aggregate handle-contract validation

The original 05.001 contract-coverage checker incorrectly rejected every
non-TODO handle implementation, including reviewed bounded JSON and ActionGroup
routes. Its historical declaration/inventory and one-C-entry-per-operation
coverage checks remain unchanged: 90 handles and 76 C entries.

Two reviewed handle rows are allowed only with exact row and receipt identities.
The JSON route checks its native19/selected-sanitizer19 evidence and current
owner sources. ActionGroup retains all immutable original no-op proof pins and
replays its archive validator without requiring superseded headers to remain
current, then verifies the dialog successor against current owner sources.
Unknown claims, changed scope, missing/tampered old **or** new proof, untested
current source changes and rollback of reviewed claims are rejected. The other
88 handle rows remain TODO. No full legacy feature/layout completion is claimed.

The aggregate gate and eight test methods passed at the exact committed source
checkpoint recorded in `report.json`, with only the checker/test overlay.
`tests.log` includes isolated immutable-archive fixtures, receipt mutations,
current-source mutations, row-field mutations, unknown claims, rollback and
historical inventory coverage. The CLI is the separate current-source gate;
unit inventory coverage intentionally isolates proof validation.

This maintenance change does not close another WBS task or reseal prior runtime
receipts. Future source evolution requires another explicit immutable-proof /
current-proof successor, not rehashing an old tested archive as new evidence.
No native tests or screenshots were rerun by this Python checker repair.
