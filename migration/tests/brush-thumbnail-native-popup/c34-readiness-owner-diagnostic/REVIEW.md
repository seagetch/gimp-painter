# C34 review proposal: separate owner-readiness diagnostic

Task308 remains DOING / NOT ACCEPTED. No C34 hosted/native result is claimed.
Same-task successor retains sole accepted parent e25a94e21efca6e6604ddac6f15570c049f421e7.
Published base C33:79a40fd5b00e03401590cf9d815927e109f88952,
tree f6d7f7628c213bb795eb6f1c3155f397cdd490c2.

## Run51 terminal observations

All8 jobs completed:5success/3failure. Immutable run51-terminal-observation-summary.json
SHA256 ca1ce05db7a035d7600aecb6133a754be4eecff45ac7aef96fe3b7e9a7aa8133.
Normal mask full107 proof passes. The owned schema compiler exits0 and creates its
cache; all14 matched executions are attempted and resource/runtime identity closure
is verified. Comparison remains inconclusive: baseline sanitizer3unknown-diagnostic,
current sanitizer3leak-only and fidelity abort. Raw baseline logs are not exported;
unknown classification does not establish a specific missing/present defect.

Both previously attributed132B path and40B label stack groups are absent in native
cases01/05/21 and no-popup fixture. This validates only the172B source-stack subset,
not the remaining roughly200KB aggregate leak problem or task acceptance.
Case14 now aborts in INITIAL popup wait, stable0, elapsed6031946us and whole main
context dispatch6028452us. Target after-paint was observed at7127us, target frame
update/layout/paint completed by6839us. The long dispatch may include that first
frame; target legacy check-resize outside frameclock or another owner remains
unresolved. Earlier sampled stacks truncate before owner/wait frames; they do not
identify the exact failing phase/owner. Do not reuse run50 timing or infer a cause.

## Separate diagnostic source/build and observations

See tools/brush-ci/readiness-owner/README.md for the complete bounded contract.
A fresh exact-current-commit worktree receives an exact Meson appendix ONLY there.
Protected app/tests/meson.build and all26 common protected files remain unchanged
in the committed tree and primary build. Original primary source, binaries, build
metadata and runtime receipts are neither rewritten nor reused as diagnostic proof.
The primary receipt is an unchanged-integrity precondition; new private identity
binds separate overlay/generated sources, fresh Meson sanitizer binary/objects,
compiler/build metadata, locked dependency archive/root inventory, locales and tools.
A distinct wrapper verifies this identity and actual binary before observation.
No ad-hoc compiler/link replay, primary object borrowing or native assertion change.

Generated source adds bounded owner/clock probes around the original wait only in
the diagnostic copy. Same-child observation uses at most8existing debugger samples
and the existing cleanup mechanism. No new ptrace permissions/capabilities, inferior
expressions, extra sample capacity or security policy change. Public output contains
finite roles/counters/hashes, not dynamic window names, raw pointers or environment.
Early workflow tests select only14pure-Python checks; GTK-backed synthetic probes
are not prerequisites in the minimal bootstrap image.

## Explicit diagnostic compute delta, subject to approval

The single existing post-primary diagnostic slot is replaced, NOT duplicated, and
runs only after sanitizer primary failure plus successful runtime seal. Proposed
outer diagnostic budget changes10minutes to20minutes, potentially10extra minutes
after failure. This is NOT the native3second readiness deadline: that deadline,
stable>=3 assertion, primary tests/sanitizer settings and acceptance remain unchanged.

Configure and compile share an absolute15minute preparation cutoff from runner
entry, with5second kill grace. After identity checking, immediately before controller
exec, a981second admission cutoff reserves180seconds observation +9seconds child
cleanup +30seconds outer margin. Insufficient time means inconclusive and no debugger
launch. No per-command budget reset, retries or time-based acceptance relaxation.

Run51 primary compile took about7minutes to the compact target, making the prior
10minute slot inadequate for a full fresh build plus observation reserve. Actual
separate diagnostic build/runtime feasibility under proposed20minutes is UNVERIFIED.
No local GIMP native build, ptrace or babl execution was attempted. This remains a
reviewed proposal pending explicit replacement/expanded diagnostic-compute approval
and subsequent exact-source hosted evidence.

Local source suite1103 tests PASS; bounded synthetic real-GObject ownership hooks
and negative cases pass. WBS policy, syntax,26protected hashes and current seals
must remain valid. These results are not native GUI/sanitizer acceptance.
