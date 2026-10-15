# Parent staging instructions

Only prepared new fixture directory:
legacy-rotation-operations-20261015/deliverables/migration/fixtures/legacy-rotation-operations

Copy that directory to maintained-repo/migration/fixtures/legacy-rotation-operations.
No maintained file was edited, no commit or publication was attempted.

Run the standalone tests after copying:
python3 migration/fixtures/legacy-rotation-operations/test_rotation_capture.py

For aggregate unittest discovery, add a thin loader in migration/tests using the
repository's existing fixture-test conventions, or integrate these 16 tests there.
The tests are currently standalone and have no external Python package dependencies.
Do not mark GUI process execution successful: both captures have command_exit=-11
after the explicit 370-row completion marker and before gimp_exit returns.

Independent review should check full-file pin hashes, extracted-function hashes,
unchanged display archive identity, GUI/source tables, literal half-step values,
key-event timing, primary/repeat equality, raw diagnostics and the shutdown caveat.
Check package manifest hashes. Confirm normal original binary hash is unchanged.
Only the parent should decide acceptance wording and ledger advancement.

RECONSTRUCTION.md and reconstruction/ are part of the exported fixture. They
contain the dependency/build recipe and exact required external inputs.
run_gui.sh is sealed provenance; new runs must use run_gui_fresh.sh with a new
output name. Guard checks refuse existing output, invalid names, and missing names.
