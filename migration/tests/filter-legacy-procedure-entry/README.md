# Checked Blinds procedure preparation: component acceptance

Accepted original child `08.018/blinds-procedure-preparation`, tested on
`53884c05ef787ffc4eda6976009fed6313570213`, tree
`cb7366d607983955e2acb29656b12d978b8898b8`, with pinned gimp-data
`a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`.

## Result

- Normal: all eight selected registrations passed. This includes 437 TAP cases
  (entry 35, Filter 223, Clone 62, LayerUI 68, XCF 49), two compile/header controls,
  and the complete non-TAP procedure registration.
- Fresh ordinary Meson ASan/UBSan: all four selected registrations passed,
  388 TAP cases (entry 35, Filter 223, Clone 62, LayerUI 68). Actual measured command
  times were 9.60, 510.83, 6.53, 150.77 seconds, inside the original 120/600/60/180
  budgets. No timeout multiplier, native failure or skipped case was accepted.
- Build inspection verifies 1511 runtime objects, 33 archives, 18 links and nine
  actual executable/loader identities, including the worker and all four
  bundled plug-ins. Three explicitly uninstrumented build-only generator
  objects are separate. RTTI and UBSan vptr checks remain enabled.
- Both final source/build/configuration/toolchain/resource/freshness/ELF seals
  passed. All 493 declared catalogs, 429 installed declarations and 361 staged
  application catalogs were required. Private profiles and authenticated
  loopback TCP Xvfb 1080x1920 were verified, including empty-authority rejection.

The C11/C++14 entry registration retains 18 previous groups and adds 17 groups
for seven typed zero arguments, independent ownership, scalar conversion,
ignored context slots, trusted-provider admission, owner boundaries,
replacement/close reentry, retired-byte lifetime, and actual no-Undo rendering.
These are entire source-matched registrations, not filtered case runs.

## Provenance and limits

The normal executor was replaced after its final seal and durable evidence
save. Sanitizer validation used a new detached source worktree, new build and
new snapshot. Complete source/data identity equals the normal snapshot and
final source seal. Host compiler/tool bytes and the signed locked dependency
package set match; this does not claim the old mounted runtime survived.

ASan/UBSan uses `detect_leaks=0:halt_on_error=1:abort_on_error=1` and
`halt_on_error=1:abort_on_error=1:print_stacktrace=1`. A fresh trivial toolchain
control independently reproduced the executor's LSan ptrace failure; the same
control passes with this policy. No leak check or external-dependency
instrumentation is claimed.

The normal procedure control replays retained compatibility fixtures: 8 native
Filter teardown repetitions; 320 Blinds, 196 SmallTiles and 174 Retinex byte-exact
oracles; 6 selected Retinex analytic ROI controls; 2 native Retinex raw-shadow ROI
plus 4 RGB-carrier rejections; 12 Blinds identity cases. It does not execute the
skipped old runtime. The existing legacy void ABI, other procedures, generic PDB
admission, parent 08.018 and separate core hooks remain open. This adds no UI
caller, new GUI/installed-prefix claim, whole-registry aggregate or full-port
completion. The prior exact 47202852 Clone/navigation gate remains immutable
and source-bound; this receipt does not make its current-root check pass.

## Evidence

`receipt.json` records exact outcomes, pins, qualifications and each retained
preacceptance failure. `evidence.tar.xz` contains the exact full normal and fresh
sanitizer public evidence archives, both final seals, native observations,
source snapshots, reviewed preparation, fresh build instrumentation/plan and
independent review. Its manifest binds every member. Full ordinary build logs
and Meson metadata remain in the checksum-bound durable build packages listed
in the receipt. No raw inherited environment or temporary X authority is
published.

Earlier fixture/evidence failures remain failures. They concerned source
materialization modes, a pkg-config lookup, generator classification, PHONY
convergence, explicit normal Meson policy, guarded lexical loader spelling and
fresh shallow history. They were resolved before acceptance without replacing
individual objects, editing Meson internals, changing production bytes, dropping
cases or weakening runtime checks. The recovered old raw continuation chain is
not reconstructed or claimed to remain mounted.

This closure changes only the evidence, contract and child progress. It adds no
native source, test or build-system changes after the tested revision.
