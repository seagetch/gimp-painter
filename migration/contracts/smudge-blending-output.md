# Original23.003 — old independent blending output

Unchanged title **Smudge の色混合を移植する**, dependency **23.002**, criterion
**旧 blending-output を再現**. No production or native test source is changed.
`source-duty-review.json` retains all52 exact original rows, their old source
hashes, individual replacement roles, proposed exact successor and every other
whole-hunk owner. Original23.004 complete footprint/repositioning and23.005
UI/save remain independently open.

## Minimal native proof

The existing captured-old48 scenes already cross Y/YA/RGB/RGBA, rate0/50/100,
Boolean blending off/on and fixed/dynamic size. Pressure independently drives
Blending; dynamic-size scenes change pressure. Full initial/finish/Undo/Redo
buffers cross color and transparency boundaries. This package runs that same
fixture, not invented new old-runtime scenes.

Two isolated private production mutations are causally rejected:
- Replace only Blending lookup with Flow: all24 enabled scenes differ; the first
  difference is case2 with406 bytes. Disabled scenes remain unchanged.
- Write shaded output back to accumulator:20 enabled scenes differ; the first
  is case6 with301 bytes. Disabled scenes remain unchanged. Rate-zero opaque
  scenarios can overwrite feedback next dab, so absence of mismatch in those
  remaining enabled cases is expected, not a missing-case exemption.

Exact restoration then passes the four standard registrations within their
unchanged60-second budgets:26 native cases, two old48-scene routes and1024
old byte-kernel vectors. Ordinary18-unit ASan/UBSan/float-cast-overflow repeats
26 native cases plus four48-scene routes (with/without paper, synchronous/stepped
owned entries). The legacy route name does not authorize a bare unowned API;
both current routes use supported owned adapters. Host/dependencies are not
fully instrumented and LSan is disabled. Existing source-spec arithmetic and
fresh dynamics/neutral-channel proof are replayed through the closed23.002
checker; they are not claimed as newly executed native tests here.

## Source and scope

The74-input archive/source seal precedes native compilation; both mutations
have exact independent source-before-build seals and saved changed C++ bytes.
A final identical source-after seal verifies restoration. Actual executable
hashes and native mismatch summaries are retained. No raw environment dump is
packaged. The orchestration script is documentary checked input; it was not
inside the74-source runtime archive and is not retroactively labeled so.

All10 held providers remain exact committed private baselines, including prior
Options blob12fd73dc406128bc94167ee3444caa05cbdd9a9d; no held diagnostic runs.
Production `app/paint/gimppaintersmudge.cpp` remains Git blob
40c57e21ae07ffb4cc2500977e4033b983750d59 (hard-mask commit5d455fd98c).
The tests use authenticated1080×1920x24 Xvfb. No UI change/new screenshot or
new old-runtime/hardware/whole-current-HEAD rebuild claim is made. High precision,
arbitrary transformed brush geometry and UI/profile acceptance remain their
separate original obligations, not silently admitted by the byte corpus.

`check-blending-output.py` checks the exact before/after ledger proposal against
HEAD; `--commit HEAD` requires the selected commit's52 closed rows and dependency
state. Historical checkpoint scripts remain hashed worktree documentary code;
this is not a whole-commit build claim. All current production/runtime evidence
continues to be source-bound. The original23.002 historical snapshots are not
rewritten merely because its dependent23.003 now closes.
