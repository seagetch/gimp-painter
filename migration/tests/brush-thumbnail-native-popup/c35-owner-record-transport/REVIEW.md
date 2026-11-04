# C35: owner diagnostic record transport, not native acceptance

C34 public `fd7280ec8a9c63deef08fe3e3cda2dccfd166ed8`, tree
`4fbd3fbcca0d87df0686a7ba7146f0b2e4ec414c`, completed run52 with five
successful and three failed jobs. See the retained terminal observation summary.
The fresh separate owner diagnostic compiled, verified 1,592 identity files and
launched; primary identity remained unchanged and owned child/debugger cleanup was
confirmed. Eight stack samples were retained, but owner records were empty.
This establishes the separate-build feasibility for that run, not acceptance.

## Reproduced transport defect

The preserved main invokes `g_test_init`, whose GLib print handler prefixes g_print
output with TAP comments (`# OWNER_DIAG ...`). The strict record reader expects
`OWNER_DIAG ...`. Existing synthetic signal tests omitted g_test_init, so did not
reproduce the production test-harness transport. A normal linked-GLib executable
and exact synthetic probe with real g_test_init reproduce old-emitter RED/new-emitter
GREEN: old output is decorated and no rows are accepted; printf emits raw rows.
Only the diagnostic probe's five output calls change to printf; final stdout flush,
probe lifetime cleanup and done breakpoint are unchanged. Global handlers unchanged.

GDB quoted target stream strings can split logical lines. The reader now accumulates
only decoded @ target chunks to newline, with a 4,096-character remainder limit.
Every split position and every truncation of the fixed valid stream are tested,
including one-character chunks, multiple lines, incomplete completion, malformed
encoding and overlong chunks. Console/log strings are never joined or accepted;
mixing raw and target-stream owner records fails closed. Numeric grammar, record
shape/completeness/index bounds and accepted values are unchanged. No fragment is
exported. Existing controller wire/line/time limits remain unchanged.

The report adds only `last_observation_stop`: a finite reason enum and a boolean
for matching the installed finish breakpoint. This is the last observed stop, not
necessarily a final stop (records can arrive during a command). No raw MI, breakpoint
address, arbitrary reason, signal string or path is exported. End-to-end mocked
controller test verifies the same-child finish after eight samples and its identity.

## Limits and preserved gates

Run52 exported no raw MI and did not retain finish stop identity. Thus the transport
defect is proven and sufficient to explain empty rows if finish was reached, but its
exclusive causality and the exact Run52 final stop cannot be recovered. A future
separately authorized hosted run must verify C35's actual record capture. No local
GIMP/native build, debugger/ptrace or babl execution, acceptance or leak verdict.

No primary/production sources, protected 26 common files, workflow slot, 20-minute
outer budget, 15-minute preparation budget, 180-second controller budget, eight
sample limit, original 3-second readiness deadline or assertion are changed. No
additional retry/child, inferior expression/call or permission is introduced.
Run52's native readiness and remaining aggregate leak failures remain unresolved;
matched comparative diagnostics remain inconclusive. Task 308 remains in progress.
