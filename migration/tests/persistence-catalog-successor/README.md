# Native preset catalog: exact persistence-source successor

Admits only `app/core/gimp.c` from catalog commit
893c4e0d3d14f7fb61065ad2e0b5cac613f31312 against the original family pin.
The four exact providers (Gimp plus descriptor C/public/private headers) and
archived old Gimp are bound. Gimp class initialization, property setter/getter
and config loading bodies are normalized-byte identical. Descriptor path names
are validated against existing readable/writable directory-list schema; the
same native LayerPreset JSON loader remains selected. Private lifecycle and
metadata are transient, not a new saved field or alternate resource backend.

The exact catalog runtime receipts remain immutable: 26 model + 5 actual GUI +
6 legacy metadata cases, selected eleven-unit sanitizer37, two implementation
mutants, causal old Preferences autosave failure, unchanged link/source seals,
and six native regression targets. This checker reads those receipts; it does
not execute native callbacks or claim whole-app instrumentation/LSan/full legacy
ABI equivalence. Actual UI evidence remains in the original catalog package.

All eight family/inventory/TSV inputs remain exact. Full metadata gate passes
1,379 records / 873 candidates / 16 routes from a selected frozen-source snapshot
plus explicit admitted successors, not the dirty main worktree. Only aggregate
source hashes, successor provenance and checker hashes change. The earlier
main-worktree family mismatch and derived-manifest mismatch are preserved as
setup diagnostics, not source regressions; neither was accepted as final proof.

Three new source-only groups reject seven metadata/schema mutations, four
provider drifts, twelve receipt drifts, two proof-pin drifts, missing archive and
incorrect transition endpoints. Existing readiness3, Undo3 and enum6 controls
also pass. No held preview/provider callback diagnostic was rerun.

Use PYTHONDONTWRITEBYTECODE=1. `test_successor.py` requires a frozen admitted
metadata checkout (main contains separately held candidate family edits).
`verify.py` checks this package's immutable metadata reports without runtime.
