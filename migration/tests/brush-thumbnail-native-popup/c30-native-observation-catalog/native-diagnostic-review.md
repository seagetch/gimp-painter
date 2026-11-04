# Run47 / C30 bounded native failure diagnostics

## Established observation
Run47 at 440378ae34082102a6d3907dd416ed61e1e80914 reaches native tests:
fixed focused OK/0; old focused EXPECTEDFAIL/-5; fixed full107 FAIL/-6.
Full107 status is observation_complete, categories warning/error/tap-failure/nonzero-native-exit.
Raw JSON hash 02d9dbbb5f19e8791b00031c7292bcda6b67f698faea4e5df425363a41adbd11.
Only five mask receipts exist in artifact. Neither native TAP nor assertion is in full job log.
No exact failing case or native cause can be established from run47 artifacts.

## Source comparison, not a causal finding
Both registrations compile the same full TU and tests.c with original group.
Mask registration additionally sets GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas.
Mask runner isolates HOME/XDG/GIMP temp and invokes fresh Xvfb/D-Bus per selection.
The TU contains expected-warning tests and subprocess session tests. Neither a timing
regression, a session config difference, nor an expected-trap issue is proven.

## Candidate
Only diagnostic helper and tests change. No C/C++ production, test registration,
runner, native expectation, provider validation, or aggregate acceptance changes.
Observation gains original107 allowlisted failed/last-passed case identities;
source-validated assertion locations (only TU and directly included fixed-name
.inc files), assertion macro kind, fixed subprocess/bailout flags, bounded counts,
stdout/stderr hashes. No arbitrary logged source path, function, message, assertion
value, environment, address, or raw log is exported. Diagnostic metadata does not
prove the cause: source location is only a hint validated against current source.
Malformed/missing allowlist/source retains fail-closed observation behavior.
The next hosted run is needed to obtain new detail; run47 cannot be enriched after
its private raw logs have disappeared.

## Validation
126 helper tests PASS, including six new diagnostic tests covering allowlisted
and unknown TAP leaves, unnumbered old-control failure, validated source macros,
included trap assertion, unknown/nonassert/out-of-range locations, missing source.
Existing raw-output redaction and all 120 C29 tests retained. py_compile PASS.
No native GIMP rerun, no external write. Root must validate published base byte
identity and update runtime seals before publishing. Raw fixture logs not included.
